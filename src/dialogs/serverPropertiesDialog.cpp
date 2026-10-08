#include "serverPropertiesDialog.h"
#include "../connectionHistory.h"
#include "../debug.h"
#include "../favoritesManager.h"
#include "../geoUtils.h"
#include "../vpnManager.h"
#include "../cli/serverListCache.h"
#include "../cli/statusMonitor.h"
#include "../widgets/sortArrowHeader.h"

#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace
{
constexpr int DIALOG_WIDTH        = 900;
constexpr int DIALOG_HEIGHT       = 640;
constexpr int DIALOG_MIN_WIDTH    = 560;
constexpr int DIALOG_MARGIN       = 20;
constexpr int DIALOG_SPACING      = 10;
constexpr int HEADER_SPACING      = 14;
constexpr int FLAG_W              = 48;
constexpr int FLAG_H              = 36;
constexpr int INFO_ROW_H_MARGIN   = 16;
constexpr int INFO_ROW_V_MARGIN   = 8;
constexpr int SECTION_TOP_SPACING = 6;
// Coordinates to two decimal places, as the server list gives them.
constexpr int COORDINATE_DECIMALS = 2;
// A cell of several addresses shows this many, and how many more there are.
constexpr int LIST_VALUES_SHOWN = 1;
// Room a column gives beyond its widest text: style.qss's padding (8 px each
// side of a cell, 8 and 22 around a header's text, the latter for the sort
// arrow) and some to spare.
constexpr int CELL_TEXT_ROOM   = 28;
constexpr int HEADER_TEXT_ROOM = 38;
// Above and below a header's text: style.qss's padding (6 px each side) and
// bottom border, and 6 px more each side. Set here, as each Qt style sizes a
// header its own way.
constexpr int HEADER_TEXT_V_ROOM = 25;

// The table's columns, in order.
enum Column : int
{
    NameColumn,
    CityColumn,
    LoadColumn,
    PlanColumn,
    FeaturesColumn,
    StatusColumn,
    EntryIpColumn,
    ExitIpColumn,
    DomainColumn,
    COLUMN_COUNT,
};

QLabel* addSectionHeader(QWidget* parent, QVBoxLayout* layout, const QString& title)
{
    layout->addSpacing(SECTION_TOP_SPACING);
    QLabel* header = new QLabel(title.toUpper(), parent);
    header->setObjectName(QStringLiteral("appSectionHeader"));
    layout->addWidget(header);
    return header;
}

// A "name ... value" row in an info card; returns the value label.
QLabel* addInfoRow(QWidget* card, QVBoxLayout* cardLayout, const QString& name, const QString& value)
{
    if (cardLayout->count() > 0)
    {
        QFrame* divider = new QFrame(card);
        divider->setFrameShape(QFrame::HLine);
        divider->setObjectName(QStringLiteral("divider"));
        cardLayout->addWidget(divider);
    }
    QWidget* row = new QWidget(card);
    row->setObjectName(QStringLiteral("infoRow"));
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(INFO_ROW_H_MARGIN, INFO_ROW_V_MARGIN, INFO_ROW_H_MARGIN, INFO_ROW_V_MARGIN);
    QLabel* nameLabel = new QLabel(name, row);
    nameLabel->setObjectName(QStringLiteral("infoKey"));
    rowLayout->addWidget(nameLabel);
    rowLayout->addStretch();
    QLabel* valueLabel = new QLabel(value, row);
    valueLabel->setObjectName(QStringLiteral("infoValue"));
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rowLayout->addWidget(valueLabel);
    cardLayout->addWidget(row);
    return valueLabel;
}

// "40.79° N, 74.06° W"
QString coordinatesText(const double latitude, const double longitude)
{
    const QLocale locale;
    return QCoreApplication::translate("ServerPropertiesDialog", "%1° %2, %3° %4")
        .arg(locale.toString(std::abs(latitude), 'f', COORDINATE_DECIMALS),
             latitude >= 0.0 ? QCoreApplication::translate("ServerPropertiesDialog", "N")
                             : QCoreApplication::translate("ServerPropertiesDialog", "S"),
             locale.toString(std::abs(longitude), 'f', COORDINATE_DECIMALS),
             longitude >= 0.0 ? QCoreApplication::translate("ServerPropertiesDialog", "E")
                              : QCoreApplication::translate("ServerPropertiesDialog", "W"));
}

// The table header's font: style.qss makes its text bold.
QFont headerFont(const QTableWidget* table)
{
    QFont font = table->horizontalHeader()->font();
    font.setBold(true);
    return font;
}

QTableWidgetItem* makeItem(const QString& text)
{
    QTableWidgetItem* item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

// The first of a server's addresses and how many more there are; all of them
// on hover. Secure Core servers can have a dozen, which would swamp the table.
QTableWidgetItem* makeListItem(const QStringList& values)
{
    QString text = values.mid(0, LIST_VALUES_SHOWN).join(QStringLiteral(", "));
    if (values.size() > LIST_VALUES_SHOWN)
    {
        text += QCoreApplication::translate("ServerPropertiesDialog", " (+%1)").arg(values.size() - LIST_VALUES_SHOWN);
    }
    QTableWidgetItem* item = makeItem(text);
    if (values.size() > LIST_VALUES_SHOWN)
    {
        item->setToolTip(values.join(QLatin1Char('\n')));
    }
    return item;
}
} // namespace

ServerPropertiesDialog::ServerPropertiesDialog(VpnManager* manager, const QString& countryCode,
                                               const QString& countryName, const QString& city,
                                               QWidget* parent)
    : QDialog(parent), m_manager(manager), m_countryCode(countryCode.toUpper()),
      m_countryName(countryName), m_city(city)
{
    const QString place = m_city.isEmpty() == true
        ? m_countryName
        : GeoUtils::cityWithRegion(m_countryCode, m_city);
    setWindowTitle(tr("%1 Properties").arg(place));
    setModal(true);
    setMinimumWidth(DIALOG_MIN_WIDTH);
    resize(DIALOG_WIDTH, DIALOG_HEIGHT);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(DIALOG_MARGIN, DIALOG_MARGIN, DIALOG_MARGIN, DIALOG_MARGIN);
    layout->setSpacing(DIALOG_SPACING);

    //  Header: flag, place, and what it is
    {
        QHBoxLayout* header = new QHBoxLayout();
        header->setSpacing(HEADER_SPACING);
        QLabel* flag = new QLabel(this);
        flag->setPixmap(GeoUtils::svgPixmap(QStringLiteral(":/flags/") + m_countryCode.toLower(), FLAG_W, FLAG_H));
        header->addWidget(flag, 0, Qt::AlignVCenter);

        QVBoxLayout* titles = new QVBoxLayout();
        titles->setSpacing(0);
        QLabel* title = new QLabel(place, this);
        title->setObjectName(QStringLiteral("sectionTitle"));
        titles->addWidget(title);
        QLabel* subtitle = new QLabel(m_city.isEmpty() == true
                                          ? tr("Connects to the fastest server in the country")
                                          : m_countryName,
                                      this);
        subtitle->setObjectName(QStringLiteral("infoKey"));
        titles->addWidget(subtitle);
        header->addLayout(titles, 1);
        layout->addLayout(header);
    }

    //  Location
    addSectionHeader(this, layout, tr("Location"));
    QWidget* card = new QWidget(this);
    card->setObjectName(QStringLiteral("infoCard"));
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);
    layout->addWidget(card);

    addInfoRow(card, cardLayout, tr("Country"), QStringLiteral("%1 (%2)").arg(m_countryName, m_countryCode));
    addInfoRow(card, cardLayout, tr("City"), m_city.isEmpty() == true
                                                 ? tr("Any: the fastest server")
                                                 : GeoUtils::cityWithRegion(m_countryCode, m_city));
    m_featuresValue = addInfoRow(card, cardLayout, tr("Features"), tr("Loading…"));
    if (m_city.isEmpty() == false)
    {
        m_coordinatesValue = addInfoRow(card, cardLayout, tr("Coordinates"), tr("Loading…"));
    }
    addInfoRow(card, cardLayout, tr("Favorite"),
               FavoritesManager::instance().isFavorite(m_countryCode, m_city) == true ? tr("Yes") : tr("No"));

    // The history keeps only the most recent connections.
    QString lastConnected = tr("Not in your recent connections");
    for (const ConnectionEntry& entry : ConnectionHistory::instance().entries())
    {
        if (entry.countryCode.compare(m_countryCode, Qt::CaseInsensitive) == 0
            && entry.city.compare(m_city, Qt::CaseInsensitive) == 0)
        {
            lastConnected = QLocale().toString(entry.connectedAt, QLocale::LongFormat);
            break;
        }
    }
    addInfoRow(card, cardLayout, tr("Last Connected"), lastConnected);

    const QString connectedServer = connectedServerHere();
    addInfoRow(card, cardLayout, tr("Connection"), connectedServer.isEmpty() == true
                                                       ? tr("Not connected here")
                                                       : tr("Connected to %1").arg(connectedServer));

    //  Servers
    addSectionHeader(this, layout, tr("Servers"));
    m_serversSummary = new QLabel(tr("Loading the server list…"), this);
    m_serversSummary->setObjectName(QStringLiteral("infoKey"));
    m_serversSummary->setWordWrap(true);
    layout->addWidget(m_serversSummary);

    m_table = new QTableWidget(0, COLUMN_COUNT, this);
    m_table->setObjectName(QStringLiteral("serverTable"));
    m_table->setHorizontalHeader(new SortArrowHeader(m_table));
    m_table->horizontalHeader()->setFixedHeight(QFontMetrics(headerFont(m_table)).height() + HEADER_TEXT_V_ROOM);
    m_table->setHorizontalHeaderLabels({tr("Server"), tr("City"), tr("Load %"), tr("Plan"), tr("Features"),
                                        tr("Status"), tr("Entry IP"), tr("Exit IP"), tr("Domain")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setHighlightSections(false);
    // Every server here is in the one city.
    m_table->setColumnHidden(CityColumn, m_city.isEmpty() == false);
    m_table->setVisible(false);
    layout->addWidget(m_table, 1);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // Shown at once from the last read, then again when this one finishes:
    // the CLI updates loads every few minutes.
    ServerListCache& cache = ServerListCache::instance();
    if (cache.hasServers() == true)
    {
        showServers(true);
    }
    connect(&cache, &ServerListCache::loaded, this, [this](const bool ok)
    {
        showServers(ok);
        logServers();
    });
    cache.reload();
}

void ServerPropertiesDialog::logServers() const
{
    if (VerboseLog::isOn() == false) return;
    const QString where = m_city.isEmpty() == true ? m_countryName : m_city + QStringLiteral(", ") + m_countryName;
    const ServerListCache& cache = ServerListCache::instance();
    if (cache.hasServers() == false)
    {
        DBG_VERBOSE(QStringLiteral("Properties of %1: no servers, as the CLI's server list could not be read.")
                        .arg(where));
        return;
    }

    // In the table's order.
    QList<ServerList::Server> servers = ServerList::serversAt(cache.servers(), m_countryCode, m_city);
    std::sort(servers.begin(), servers.end(),
              [](const ServerList::Server& a, const ServerList::Server& b) { return a.name < b.name; });
    qsizetype nodeCount = 0;
    for (const ServerList::Server& server : servers)
    {
        nodeCount += server.nodes.size();
    }
    DBG_VERBOSE(QStringLiteral("Properties of %1 (%2): %3 servers, %4 nodes.")
                    .arg(where, m_countryCode).arg(servers.size()).arg(nodeCount));
    for (const ServerList::Server& server : servers)
    {
        DBG_VERBOSE(QStringLiteral("  ") + ServerList::describe(server));
        for (const ServerList::Node& node : server.nodes)
        {
            DBG_VERBOSE(QStringLiteral("    ") + ServerList::describe(node));
        }
    }
}

void ServerPropertiesDialog::sizeColumns() const
{
    // From the text alone, not resizeColumnsToContents(): each Qt style pads
    // cells its own way, and the columns should come out the same in all.
    const QFontMetrics headerMetrics(headerFont(m_table));
    // Bold, as the connected server's row is.
    QFont itemFont = m_table->font();
    itemFont.setBold(true);
    const QFontMetrics itemMetrics(itemFont);
    for (int column = 0; column < COLUMN_COUNT; ++column)
    {
        int width = headerMetrics.horizontalAdvance(m_table->horizontalHeaderItem(column)->text()) + HEADER_TEXT_ROOM;
        for (int row = 0; row < m_table->rowCount(); ++row)
        {
            const QTableWidgetItem* item = m_table->item(row, column);
            if (item != nullptr)
            {
                width = std::max(width, itemMetrics.horizontalAdvance(item->text()) + CELL_TEXT_ROOM);
            }
        }
        m_table->setColumnWidth(column, width);
    }
}

QString ServerPropertiesDialog::connectedServerHere() const
{
    const QString status = m_manager->connectedServer();
    const bool here = m_manager->currentState() == VpnState::Connected
        && StatusMonitor::parseCountryFromServer(status).compare(m_countryCode, Qt::CaseInsensitive) == 0
        && (m_city.isEmpty() == true
            || StatusMonitor::parseCityFromServer(status).compare(m_city, Qt::CaseInsensitive) == 0);
    return here == true ? StatusMonitor::parseServerName(status) : QString();
}

void ServerPropertiesDialog::showServers(const bool ok)
{
    const ServerListCache& cache = ServerListCache::instance();
    if (cache.hasServers() == false)
    {
        if (ok == false)
        {
            m_serversSummary->setText(tr("The server details are not available: the Proton VPN CLI's "
                                         "server list could not be read."));
            m_featuresValue->setText(tr("Unknown"));
            if (m_coordinatesValue != nullptr)
            {
                m_coordinatesValue->setText(tr("Unknown"));
            }
            // The CLI still names a city's features.
            if (m_city.isEmpty() == false)
            {
                QPointer<QLabel> featuresValue = m_featuresValue;
                m_manager->fetchCityFeatures(m_countryCode, m_city, [featuresValue](const QString& features)
                {
                    if (featuresValue != nullptr && features.isEmpty() == false)
                    {
                        featuresValue->setText(features);
                    }
                });
            }
        }
        return;
    }

    const QList<ServerList::Server> servers = ServerList::serversAt(cache.servers(), m_countryCode, m_city);
    const ServerList::Summary summary = ServerList::summarize(servers);

    int features = 0;
    for (const ServerList::Server& server : servers)
    {
        features |= server.features;
    }
    const QStringList featureNames = ServerList::featureNames(features);
    m_featuresValue->setText(featureNames.isEmpty() == true ? tr("None") : featureNames.join(QStringLiteral(", ")));
    if (m_coordinatesValue != nullptr)
    {
        m_coordinatesValue->setText(servers.isEmpty() == true
                                        ? tr("Unknown")
                                        : coordinatesText(servers.first().latitude, servers.first().longitude));
    }

    if (servers.isEmpty() == true)
    {
        m_serversSummary->setText(tr("The Proton VPN CLI's server list has no servers here."));
        m_table->setVisible(false);
        return;
    }
    QString text = summary.count == 1
        ? tr("1 server, %1 online, %2 for free accounts.").arg(summary.online).arg(summary.free)
        : tr("%1 servers, %2 online, %3 for free accounts.").arg(summary.count).arg(summary.online).arg(summary.free);
    if (summary.online > 0)
    {
        text += QLatin1Char(' ') + tr("Load %1% to %2%, %3% on average.")
                                       .arg(summary.minLoad).arg(summary.maxLoad).arg(summary.averageLoad);
    }
    m_serversSummary->setText(text);

    // Rebuilt in full: a reload replaces every row.
    const QString connected = connectedServerHere();
    m_table->setSortingEnabled(false);
    m_table->setRowCount(static_cast<int>(servers.size()));
    for (int row = 0; row < servers.size(); ++row)
    {
        const ServerList::Server& server = servers.at(row);
        QTableWidgetItem* load = makeItem(QString());
        // A number, so the column sorts by it.
        load->setData(Qt::DisplayRole, server.load);
        const QList<QTableWidgetItem*> items = {
            makeItem(server.name),
            makeItem(server.city),
            load,
            makeItem(ServerList::tierName(server.tier)),
            makeItem(ServerList::featureNames(server.features).join(QStringLiteral(", "))),
            makeItem(server.online == true ? tr("Online") : tr("Offline")),
            makeListItem(server.entryIps),
            makeListItem(server.exitIps),
            makeListItem(server.domains),
        };
        for (int column = 0; column < COLUMN_COUNT; ++column)
        {
            QTableWidgetItem* item = items.at(column);
            if (server.name == connected)
            {
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
                item->setToolTip(tr("Connected to this server"));
            }
            m_table->setItem(row, column, item);
        }
    }
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(NameColumn, Qt::AscendingOrder);
    sizeColumns();
    m_table->setVisible(true);
}
