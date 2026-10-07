#include "quickSettingsPanel.h"
#include "elidaLabel.h"
#include "numberSpinner.h"
#include "quickSettingsMenu.h"
#include "styleUtils.h"
#include "styledComboBox.h"
#include "toggleWithStatus.h"
#include "../appConfig.h"
#include "../vpnManager.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <initializer_list>
#include <tuple>
#include <utility>

namespace
{
// Around the panel's content. There is no left margin: the page's content
// keeps its own distance from the panel.
constexpr int TOP_MARGIN       = 24;
constexpr int RIGHT_MARGIN     = 20;
constexpr int BOTTOM_MARGIN    = 24;
// Between a section's header and its card, and above the next section.
constexpr int HEADER_SPACING   = 8;
constexpr int SECTION_SPACING  = 16;
// Lines the header up with the setting names in its card.
constexpr int HEADER_L_MARGIN  = 12;

constexpr int ROW_H_MARGIN     = 12;
// Above and below each setting whenever the panel has the room: less only
// when the chosen settings would otherwise not fit (see fitRows()).
constexpr int ROW_V_MARGIN     = 8;
constexpr int ROW_SPACING      = 12;
// Copies of the larger controls: no wider than the row allows, and as tall
// as a dropdown, which the button's padding matches too (style.qss).
constexpr int COMBO_MAX_WIDTH  = 190;
constexpr int NUMBER_HEIGHT    = 32;

void addSectionHeader(QWidget* parent, QVBoxLayout* layout, const QString& title)
{
    QLabel* header = new QLabel(title.toUpper(), parent);
    header->setObjectName(QStringLiteral("appSectionHeader"));
    header->setContentsMargins(HEADER_L_MARGIN, 0, 0, 0);
    layout->addWidget(header);
    layout->addSpacing(HEADER_SPACING);
}

// An empty card for a section's rows; addCardRow() fills it.
std::pair<QWidget*, QVBoxLayout*> addCard(QWidget* parent, QVBoxLayout* layout)
{
    QWidget* card = new QWidget(parent);
    card->setObjectName(QStringLiteral("infoCard"));
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);
    layout->addWidget(card);
    return {card, cardLayout};
}

// Adds `row` to a card, below a divider unless it is the card's first row.
void addCardRow(QWidget* card, QVBoxLayout* layout, QWidget* row)
{
    if (layout->count() > 0)
    {
        QFrame* divider = new QFrame(card);
        divider->setFrameShape(QFrame::HLine);
        divider->setObjectName(QStringLiteral("divider"));
        layout->addWidget(divider);
    }
    layout->addWidget(row);
}
} // namespace

QuickSettingsPanel::QuickSettingsPanel(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager), m_rowPadding(ROW_V_MARGIN)
{
    setFixedWidth(PANEL_WIDTH);

    // Only scrolls when the fonts are much larger than the design's.
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scroll);

    // The Settings page changes some originals without a signal of their own
    // (VPN settings read back from the CLI, locked while connecting, or Plus
    // only), so these are a cue to look again as well.
    connect(m_manager, &VpnManager::settingsReady, this, &QuickSettingsPanel::scheduleSync);
    connect(m_manager, &VpnManager::connectionStateChanged, this, &QuickSettingsPanel::scheduleSync);
    connect(m_manager, &VpnManager::accountTypeReady, this, &QuickSettingsPanel::scheduleSync);

    rebuild();
}

void QuickSettingsPanel::setSources(const QList<QuickSettingSource>& sources)
{
    m_sources = sources;
    using Kind = QuickSettingSource::Kind;
    for (const QuickSettingSource& source : std::as_const(m_sources))
    {
        // Enabled, shown or hidden: see eventFilter().
        source.control->installEventFilter(this);
        source.row->installEventFilter(this);
        switch (source.kind)
        {
            case Kind::Toggle:
            case Kind::CustomDns:
                connect(qobject_cast<ToggleWithStatus*>(source.control), &ToggleWithStatus::toggled,
                        this, &QuickSettingsPanel::scheduleSync);
                break;

            case Kind::Combo:
            {
                // The copy shares the original's items, so they can change
                // under it; the original refills some of them quietly.
                const QComboBox* combo = qobject_cast<QComboBox*>(source.control);
                connect(combo, &QComboBox::currentIndexChanged, this, &QuickSettingsPanel::scheduleSync);
                connect(combo->model(), &QAbstractItemModel::modelReset, this, &QuickSettingsPanel::scheduleSync);
                connect(combo->model(), &QAbstractItemModel::rowsInserted, this, &QuickSettingsPanel::scheduleSync);
                connect(combo->model(), &QAbstractItemModel::rowsRemoved, this, &QuickSettingsPanel::scheduleSync);
                break;
            }

            case Kind::Number:
                connect(qobject_cast<NumberSpinner*>(source.control), &NumberSpinner::valueChanged,
                        this, &QuickSettingsPanel::scheduleSync);
                break;

            case Kind::Button:
                break;
        }
    }
    rebuild();
}

void QuickSettingsPanel::rebuild()
{
    m_rows.clear();

    QWidget* content = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, TOP_MARGIN, RIGHT_MARGIN, BOTTOM_MARGIN);
    layout->setSpacing(0);

    using Section = QuickSettingSource::Section;
    const QStringList chosen = AppConfig::instance().quickSettings();
    for (const auto& [section, title] : std::initializer_list<std::pair<Section, QString>>{
             {Section::Vpn, tr("VPN")},
             {Section::Appearance, tr("Appearance")},
             {Section::App, tr("App")}})
    {
        // Started with the section's first chosen setting, so a section
        // with none takes no room.
        QWidget* card = nullptr;
        QVBoxLayout* cardLayout = nullptr;
        for (const QuickSettingSource& source : std::as_const(m_sources))
        {
            if (source.section != section || chosen.contains(source.id) == false) continue;
            if (card == nullptr)
            {
                if (layout->count() > 0)
                {
                    layout->addSpacing(SECTION_SPACING);
                }
                addSectionHeader(content, layout, title);
                std::tie(card, cardLayout) = addCard(content, layout);
            }
            addCardRow(card, cardLayout, makeRow(card, source));
        }
    }
    layout->addStretch(1);

    QWidget* old = m_scroll->takeWidget();
    if (old != nullptr)
    {
        old->deleteLater();
    }
    m_scroll->setWidget(content);
    sync();
    // Once the new rows have their style's sizes.
    QTimer::singleShot(0, this, &QuickSettingsPanel::fitRows);
}

void QuickSettingsPanel::fitRows()
{
    QWidget* content = m_scroll->widget();
    if (content == nullptr || m_rows.isEmpty() == true) return;
    content->ensurePolished();

    // What the content needs without padding around the settings, and so
    // what is left of the panel's height to share out as that padding, up
    // to the full amount: a short window or many settings tighten the rows
    // rather than make the panel scroll.
    const int rows = static_cast<int>(m_rows.size());
    const int unpadded = content->sizeHint().height() - 2 * m_rowPadding * rows;
    const int padding = std::clamp((height() - unpadded) / (2 * rows), 0, ROW_V_MARGIN);
    if (padding == m_rowPadding) return;
    m_rowPadding = padding;
    for (const Row& row : std::as_const(m_rows))
    {
        row.layout->setContentsMargins(ROW_H_MARGIN, padding, ROW_H_MARGIN, padding);
    }
}

QWidget* QuickSettingsPanel::makeRow(QWidget* parent, const QuickSettingSource& source)
{
    QWidget* row = new QWidget(parent);
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    // The padding the other rows have; fitRows() then fits them all.
    rowLayout->setContentsMargins(ROW_H_MARGIN, m_rowPadding, ROW_H_MARGIN, m_rowPadding);
    rowLayout->setSpacing(ROW_SPACING);

    // One line each, so every row is the same height; a name too long for
    // the row is cut short and shown whole on hover.
    ElideLabel* label = new ElideLabel(source.label, row);
    label->setObjectName(QStringLiteral("infoKey"));
    rowLayout->addWidget(label, 1);

    QWidget* control = makeControl(row, source);
    rowLayout->addWidget(control, 0, Qt::AlignVCenter);

    // Right-clicking a setting offers to take it out, as on the Settings
    // page; its labels leave right-clicks to the row.
    row->setContextMenuPolicy(Qt::CustomContextMenu);
    const QList<QLabel*> labels = row->findChildren<QLabel*>();
    for (QLabel* rowLabel : labels)
    {
        rowLabel->setContextMenuPolicy(Qt::NoContextMenu);
    }
    connect(row, &QWidget::customContextMenuRequested, this, [this, row, id = source.id](const QPoint& pos)
    {
        // Over the window: inside the VPN page its stylesheet would clear
        // the menu's background.
        if (QuickSettingsMenu::exec(id, row->mapToGlobal(pos), window()) == true)
        {
            emit quickSettingsChanged();
        }
    });

    m_rows.append({.source = source, .widget = row, .layout = rowLayout, .label = label, .control = control});
    return row;
}

QWidget* QuickSettingsPanel::makeControl(QWidget* parent, const QuickSettingSource& source)
{
    // Every change goes to the original, which may refuse or ask about it
    // (a reconnect, a missing tool), so the copy then takes its state.
    using Kind = QuickSettingSource::Kind;
    switch (source.kind)
    {
        case Kind::Toggle:
        {
            ToggleWithStatus* original = qobject_cast<ToggleWithStatus*>(source.control);
            ToggleWithStatus* toggle = new ToggleWithStatus(parent);
            connect(toggle, &ToggleWithStatus::toggled, this, [this, original](const bool on)
            {
                original->setOnAsUser(on);
                sync();
            });
            return toggle;
        }

        case Kind::Combo:
        {
            QComboBox* original = qobject_cast<QComboBox*>(source.control);
            StyledComboBox* combo = new StyledComboBox(parent);
            combo->setModel(original->model());
            combo->setMaximumWidth(COMBO_MAX_WIDTH);
            // Only the user's choices: syncing sets the index too.
            connect(combo, &QComboBox::activated, this, [this, original](const int index)
            {
                original->setCurrentIndex(index);
                sync();
            });
            return combo;
        }

        case Kind::Number:
        {
            NumberSpinner* original = qobject_cast<NumberSpinner*>(source.control);
            NumberSpinner* number = new NumberSpinner(parent);
            number->setRange(original->minimum(), original->maximum());
            number->setFixedHeight(NUMBER_HEIGHT);
            connect(number, &NumberSpinner::valueChanged, this, [this, original](const int value)
            {
                original->setValueAsUser(value);
                sync();
            });
            return number;
        }

        case Kind::Button:
        {
            QPushButton* original = qobject_cast<QPushButton*>(source.control);
            QPushButton* button = new QPushButton(original->text(), parent);
            button->setObjectName(original->objectName());
            button->setCursor(Qt::PointingHandCursor);
            connect(button, &QPushButton::clicked, this, [this, original]()
            {
                original->click();
                sync();
            });
            return button;
        }

        case Kind::CustomDns:
        {
            ToggleWithStatus* toggle = new ToggleWithStatus(parent);
            connect(toggle, &ToggleWithStatus::toggled, this, [this, source](const bool on)
            {
                turnCustomDns(source, on);
            });
            return toggle;
        }
    }
    return nullptr;
}

void QuickSettingsPanel::turnCustomDns(const QuickSettingSource& source, const bool on)
{
    ToggleWithStatus* original = qobject_cast<ToggleWithStatus*>(source.control);
    if (on == false)
    {
        original->setOnAsUser(false);
        sync();
        return;
    }

    // There is no room for the servers field here. Over the window: inside
    // the VPN page its stylesheet would clear the dialog's background.
    bool ok = false;
    const QString servers = QInputDialog::getText(
        window(), tr("Custom DNS"),
        tr("DNS servers to use, separated by commas:"),
        QLineEdit::Normal, source.dnsEdit->text(), &ok);
    if (ok == true && servers.trimmed().isEmpty() == false)
    {
        original->setOnAsUser(true);
        source.dnsEdit->setText(servers);
        source.dnsApply->click();
    }
    sync();
}

void QuickSettingsPanel::scheduleSync()
{
    if (m_syncPending == true) return;
    m_syncPending = true;
    QTimer::singleShot(0, this, &QuickSettingsPanel::sync);
}

void QuickSettingsPanel::sync()
{
    m_syncPending = false;
    const bool isFree = m_manager->accountType() == AccountType::Free;
    using Kind = QuickSettingSource::Kind;
    for (const Row& row : std::as_const(m_rows))
    {
        const QuickSettingSource& source = row.source;
        switch (source.kind)
        {
            case Kind::Toggle:
            case Kind::CustomDns:
                qobject_cast<ToggleWithStatus*>(row.control)->setOn(
                    qobject_cast<ToggleWithStatus*>(source.control)->isOn(), false);
                break;

            case Kind::Combo:
            {
                QComboBox* combo = qobject_cast<QComboBox*>(row.control);
                const QSignalBlocker blocker(combo);
                combo->setCurrentIndex(qobject_cast<QComboBox*>(source.control)->currentIndex());
                break;
            }

            case Kind::Number:
                qobject_cast<NumberSpinner*>(row.control)->setValue(
                    qobject_cast<NumberSpinner*>(source.control)->value());
                break;

            case Kind::Button:
                break;
        }

        // Unavailable whenever the original is: disabled (while connecting,
        // for a free account, by another setting), or with its row hidden
        // because the setting does not apply right now.
        // Only the control: a disabled row would also lose its tooltip and
        // its right-click menu.
        const bool applies = source.row->isHidden() == false;
        const bool enabled = applies == true && source.control->isEnabled() == true;
        row.control->setEnabled(enabled);
        setStyleProperty(row.label, "disabled", enabled == false);

        QString tip = source.row->toolTip();
        if (source.plusOnly == true && isFree == true)
        {
            tip = tr("Available to Plus Members.");
        }
        else if (applies == false)
        {
            tip = tr("Not available with your current settings.");
        }
        row.widget->setToolTip(tip);
    }
}

void QuickSettingsPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Whatever changed on the Settings page while this was out of sight.
    sync();
    fitRows();
}

void QuickSettingsPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    fitRows();
}

bool QuickSettingsPanel::eventFilter(QObject* obj, QEvent* event)
{
    // Installed on the originals and their rows.
    switch (event->type())
    {
        case QEvent::EnabledChange:
        case QEvent::ShowToParent:
        case QEvent::HideToParent:
            scheduleSync();
            break;

        default:
            break;
    }
    return QWidget::eventFilter(obj, event);
}
