#include "countriesPage.h"
#include "../appConfig.h"
#include "../geoUtils.h"
#include "../themeManager.h"
#include "../uiHelpers.h"
#include "../widgets/gripSplitter.h"
#include "../widgets/pixmapCache.h"
#include "../widgets/serverContextMenu.h"
#include "../widgets/starButton.h"

#include <algorithm>
#include <ranges>
#include <QDialog>
#include <QEvent>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QResizeEvent>
#include <QSplitter>
#include <QSvgRenderer>
#include <QToolButton>

// Feature metadata shared by wide + narrow city rows

// kServerFeatures is defined in uiHelpers.h

namespace
{
// Spinner animation tick for the "Loading countries/cities" placeholders.
constexpr int SPINNER_INTERVAL_MS = 200;

// Margins around the page on its own, and beside the home page, where they
// mirror the quick settings on the other side: as far from the window's edge,
// and with the title level with theirs.
constexpr int PAGE_MARGIN         = 16;
constexpr int SPLIT_SIDE_MARGIN   = 20;
constexpr int SPLIT_TOP_MARGIN    = 18;
constexpr int SPLIT_BOTTOM_MARGIN = 24;
// Style property style.qss selects the split view look by.
constexpr const char* SPLIT_VIEW_PROPERTY = "splitView";

// Wide country-list icon geometry: a fixed box so the star + flag pair is
// never downscaled, with the flag always at the same offset whether or not a
// star is drawn in front of it.
constexpr int COUNTRY_ICON_W      = 40;
constexpr int COUNTRY_ICON_H      = 16;
constexpr int COUNTRY_FLAG_X      = 12;
constexpr int COUNTRY_STAR_W      = 9;
constexpr int COUNTRY_FLAG_W      = 20;
constexpr int COUNTRY_FLAG_H      = 15;

// Narrow accordion arrows, in the accordion's text color for each theme.
constexpr QColor ACCORDION_ARROW_ON_DARK(0xea, 0xea, 0xea);
constexpr QColor ACCORDION_ARROW_ON_LIGHT(0x1a, 0x1a, 0x2e);

// Split view country cards: the flag (4:3, like the flag artwork) with
// rounded corners and a hairline edge, so a white flag still shows on a light
// card; the narrowest a card gets before a row holds one fewer; and a card's
// height, its margins included.
constexpr int    CARD_FLAG_W      = 64;
constexpr int    CARD_FLAG_H      = 48;
constexpr qreal  CARD_FLAG_RADIUS = 4.0;
constexpr QColor CARD_FLAG_EDGE(0x80, 0x80, 0x80, 0x5a);
constexpr int    CARD_MIN_W       = 104;
constexpr int    CARD_H           = 104;
// The local country's star, on a dark badge in the flag's top-right corner.
constexpr int    STAR_BADGE_SIZE  = 18;
constexpr int    STAR_BADGE_INSET = 3;
constexpr QColor STAR_BADGE_COLOR(0, 0, 0, 0x96);

// The countries' and cities' list rows. Set rather than left to the Qt
// style, which pads items differently: this is what Breeze gives them, and
// Fusion (the AppImage's style) now matches.
constexpr int LIST_ROW_HEIGHT = 41;

// Default pane sizes: side by side, equal; stacked in the split view, as
// shares of the height for the countries and for their cities.
constexpr int LIST_PANE_W           = 200;
constexpr int STACK_COUNTRIES_SHARE = 60;
constexpr int STACK_CITIES_SHARE    = 40;
} // namespace

// Helper: does a feature string contain a keyword?
static bool hasFeature(const QString& features, const char* keyword)
{
    const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);
    return std::ranges::any_of(tags, [keyword](const QString& t)
    {
        return t.trimmed().contains(QLatin1String(keyword), Qt::CaseInsensitive);
    });
}

// City-level filter predicate used by both wide and narrow city lists.
static bool cityPassesFilters(const QString& features,
                              const bool filterP2P,
                              const bool filterSecureCore,
                              const bool filterTor)
{
    if (filterP2P == false
        && filterSecureCore == false
        && filterTor == false)
    {
        return true;
    }

    // AND semantics: all enabled filters must be present.
    if (filterP2P == true
        && hasFeature(features, kServerFeatures[0].keyword) == false)
    {
        return false;
    }

    if (filterSecureCore == true
        && hasFeature(features, kServerFeatures[1].keyword) == false)
    {
        return false;
    }

    if (filterTor == true
        && hasFeature(features, kServerFeatures[2].keyword) == false)
    {
        return false;
    }

    return true;
}

// Shared star styling so wide and narrow views match exactly.
static void drawPinnedStar(QPainter& p, const QRect& r)
{
    QFont starFont = p.font();
    starFont.setBold(true);
    starFont.setPointSize(qMax(starFont.pointSize() + 1, 10));
    p.setFont(starFont);
    p.setPen(QColor(0xff, 0xd2, 0x4a)); // gold
    p.drawText(r, Qt::AlignCenter, QStringLiteral("★"));
}

// Wide country-list icon: optional star, then flag.
static QIcon makeCountryListIcon(const QString& countryCode, const bool pinned)
{
    QPixmap pm(COUNTRY_ICON_W, COUNTRY_ICON_H);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    if (pinned)
    {
        drawPinnedStar(p, QRect(0, 0, COUNTRY_STAR_W, COUNTRY_ICON_H));
    }

    const QPixmap flag = GeoUtils::svgPixmap(
        QStringLiteral(":/flags/") + countryCode.toLower(), COUNTRY_FLAG_W, COUNTRY_FLAG_H);
    if (flag.isNull() == false)
    {
        p.drawPixmap(COUNTRY_FLAG_X, 0, flag);
    }

    return QIcon(pm);
}

// Split view card icon: the flag, and the local country's star on a badge.
static QIcon makeCountryCardIcon(const QString& countryCode, const bool pinned)
{
    QPixmap pm = blankPixmap(QSizeF(CARD_FLAG_W, CARD_FLAG_H), qApp->devicePixelRatio());
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    // Inset half a pixel, so the edge is drawn on whole pixels.
    const QRectF flagRect = QRectF(0, 0, CARD_FLAG_W, CARD_FLAG_H).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath rounded;
    rounded.addRoundedRect(flagRect, CARD_FLAG_RADIUS, CARD_FLAG_RADIUS);
    p.setClipPath(rounded);
    p.drawPixmap(QPointF(0, 0), GeoUtils::svgPixmap(
        QStringLiteral(":/flags/") + countryCode.toLower(), CARD_FLAG_W, CARD_FLAG_H));
    p.setClipping(false);
    p.setPen(CARD_FLAG_EDGE);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(flagRect, CARD_FLAG_RADIUS, CARD_FLAG_RADIUS);

    if (pinned)
    {
        const QRect badge(CARD_FLAG_W - STAR_BADGE_SIZE - STAR_BADGE_INSET, STAR_BADGE_INSET,
                          STAR_BADGE_SIZE, STAR_BADGE_SIZE);
        p.setPen(Qt::NoPen);
        p.setBrush(STAR_BADGE_COLOR);
        p.drawEllipse(badge);
        drawPinnedStar(p, badge);
    }
    p.end();

    // The same pixmap when selected: the view would otherwise tint the flag
    // with the selection color.
    QIcon icon(pm);
    icon.addPixmap(pm, QIcon::Selected);
    return icon;
}

// Header icon for narrow accordion: arrow indicator, optional star, then country flag.
static QIcon makeAccordionHeaderIcon(const QString& countryCode,
                                     const bool expanded,
                                     const bool pinned)
{
    const int iconW = pinned ? 48 : 38;
    QPixmap pm(iconW, 16);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    const QString arrow = expanded ? QStringLiteral("▾") : QStringLiteral("▸");
    const QColor arrowColor = ThemeManager::isDark() == true ? ACCORDION_ARROW_ON_DARK : ACCORDION_ARROW_ON_LIGHT;
    p.setPen(arrowColor);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(qMax(f.pointSize() - 1, 8));
    p.setFont(f);
    p.drawText(QRect(0, 0, 10, 16), Qt::AlignCenter, arrow);

    int x = 12;
    if (pinned)
    {
        drawPinnedStar(p, QRect(x, 0, 9, 16));
        p.setFont(f);
        p.setPen(arrowColor);
        x += 12; // star width + right padding
    }

    const QPixmap flag = GeoUtils::svgPixmap(
        QStringLiteral(":/flags/") + countryCode.toLower(), 20, 15);
    if (flag.isNull() == false)
    {
        p.drawPixmap(x, 0, flag);
    }

    return QIcon(pm);
}

// Add a non-selectable, word-wrapped informational row to a QListWidget.
static void addWideInfoRow(QListWidget* list, const QString& text)
{
    if (list == nullptr) return;

    auto* item = new QListWidgetItem();
    item->setFlags(Qt::NoItemFlags);

    auto* row = new QWidget(list);
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(10, 12, 10, 12);
    layout->setSpacing(0);

    auto* label = new QLabel(text, row);
    label->setObjectName(QStringLiteral("infoLabel"));
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(label);

    // Use a conservative wrapped-height estimate and a larger minimum to avoid
    // clipping with list item padding/borders in styled views.
    const int wrapW = qMax(200, list->viewport()->width() - 24);
    const QRect textRect = label->fontMetrics().boundingRect(
        QRect(0, 0, wrapW, 5000), Qt::TextWordWrap, text);
    const int rowH = qMax(56, textRect.height() + 36);
    item->setSizeHint(QSize(0, rowH));

    list->addItem(item);
    list->setItemWidget(item, row);
}

// ============================================================
// Bubble style helper
// ============================================================
// The min-height keeps the pill shape whatever the font: Qt drops a border
// radius entirely, leaving square corners, once the two radii add up to more
// than the button's height. With a font whose 12px line height is short
// (DejaVu Sans, the Flatpak runtime's default), the button would otherwise be
// 22px tall, under the 24px that two 12px radii need. 16px of content plus
// 6px of padding and 2px of border is exactly 24px.
QString CountriesPage::bubbleStyle(const bool active)
{
    if (active)
        return QStringLiteral(
            "QPushButton { background: #6d4aff; color: white; border: 1px solid #6d4aff; "
            "border-radius: 12px; padding: 3px 12px; min-height: 16px; font-size: 12px; }"
            "QPushButton:hover { background: #7d5aff; }");
    // Inactive bubbles are outlined in the theme's own colors, which the
    // light theme needs darker to show at all.
    if (ThemeManager::isDark() == false)
        return QStringLiteral(
            "QPushButton { background: transparent; color: #5a5a7a; border: 1px solid #c8c8d8; "
            "border-radius: 12px; padding: 3px 12px; min-height: 16px; font-size: 12px; }"
            "QPushButton:hover { border-color: #6d4aff; color: #1a1a2e; }");
    return QStringLiteral(
        "QPushButton { background: transparent; color: #aaaacc; border: 1px solid #444466; "
        "border-radius: 12px; padding: 3px 12px; min-height: 16px; font-size: 12px; }"
        "QPushButton:hover { border-color: #6d4aff; color: #ccccee; }");
}

// ============================================================
// Constructor
// ============================================================
CountriesPage::CountriesPage(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager),
      m_localCountryCode(GeoUtils::detectUserCountry())
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN);
    mainLayout->setSpacing(10);

    //  Header row
    auto* headerRow = new QHBoxLayout();

    m_countriesLabel = new QLabel(tr("Countries"), this);
    m_countriesLabel->setObjectName(QStringLiteral("sectionTitle"));
    headerRow->addWidget(m_countriesLabel);
    headerRow->addStretch();

    m_refreshBtn = new QPushButton(tr("\u21bb Refresh"), this);
    m_refreshBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_refreshBtn->setFixedHeight(30);
    connect(m_refreshBtn, &QPushButton::clicked, this, &CountriesPage::refresh);
    headerRow->addWidget(m_refreshBtn);
    mainLayout->addLayout(headerRow);

    //  Search
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("inputField"));
    m_searchEdit->setPlaceholderText(tr("Search countries\u2026"));
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString&){ applyFilter(); });
    mainLayout->addWidget(m_searchEdit);

    //  Filter bubbles
    auto* bubblesRow = new QHBoxLayout();
    bubblesRow->setSpacing(6);

    auto makeBubble = [&](const QString& label) -> QPushButton* {
        auto* btn = new QPushButton(label, this);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    m_bubbleAll        = makeBubble(tr("All"));
    m_bubbleP2P        = makeBubble(tr("P2P"));
    m_bubbleSecureCore = makeBubble(tr("Secure Core"));
    m_bubbleTor        = makeBubble(tr("Tor"));

    bubblesRow->addWidget(m_bubbleAll);
    bubblesRow->addWidget(m_bubbleP2P);
    bubblesRow->addWidget(m_bubbleSecureCore);
    bubblesRow->addWidget(m_bubbleTor);
    bubblesRow->addStretch();
    mainLayout->addLayout(bubblesRow);

    updateBubbleStyles();

    connect(m_bubbleAll, &QPushButton::clicked, this, [this]() {
        if (m_portForwardingEnabled && m_filterP2P)
        {
            showDisablePortForwardingDialog(
                tr("The <b>P2P</b> filter is active because <b>Port Forwarding</b> "
                   "is enabled in your settings.<br><br>"
                   "Clearing all filters will also remove the P2P filter, which "
                   "requires disabling Port Forwarding. Would you like to do that?"),
                [this]() {
                    m_portForwardingEnabled = false;
                    m_filterP2P = m_filterSecureCore = m_filterTor = false;
                    updateBubbleStyles();
                    applyFilter();
                });
            return;
        }

        m_filterP2P = m_filterSecureCore = m_filterTor = false;
        updateBubbleStyles();
        applyFilter();
    });
    connect(m_bubbleP2P, &QPushButton::clicked, this, [this]() {
        if (m_filterP2P && m_portForwardingEnabled)
        {
            showDisablePortForwardingDialog(
                tr("The <b>P2P</b> filter is active because <b>Port Forwarding</b> "
                   "is enabled in your settings.<br><br>"
                   "To turn off this filter you need to disable Port Forwarding. "
                   "Would you like to do that?"),
                [this]() {
                    m_portForwardingEnabled = false;
                    m_filterP2P = false;
                    updateBubbleStyles();
                    applyFilter();
                });
            return;
        }

        // Normal toggle (port forwarding is not driving this filter)
        m_filterP2P = !m_filterP2P;
        updateBubbleStyles();
        applyFilter();
    });
    connect(m_bubbleSecureCore, &QPushButton::clicked, this, [this]() {
        m_filterSecureCore = !m_filterSecureCore;
        updateBubbleStyles();
        applyFilter();
    });
    connect(m_bubbleTor, &QPushButton::clicked, this, [this]() {
        m_filterTor = !m_filterTor;
        updateBubbleStyles();
        applyFilter();
    });

    //  Sync P2P filter with port forwarding setting
    auto syncP2PFilter = [this](const QMap<QString, QString>& settings)
    {
        const QString v = settings.value(QStringLiteral("port-forwarding")).toLower().trimmed();
        const bool pfOn = isOnString(v);
        m_portForwardingEnabled = pfOn;

        if (pfOn == true && m_filterP2P == false)
        {
            m_filterP2P = true;
            updateBubbleStyles();
            applyFilter();
        }
        else if (pfOn == false && m_filterP2P == true)
        {
            // Port forwarding was just turned off externally (e.g. from Settings page);
            // mirror that by clearing the P2P filter.
            m_filterP2P = false;
            updateBubbleStyles();
            applyFilter();
        }
    };
    // The manager re-reads the settings at sign-in and after every change, so
    // this also follows port forwarding being toggled from the Settings page.
    connect(m_manager, &VpnManager::settingsReady, this, syncP2PFilter);
    // Seed the initial port-forwarding state.
    syncP2PFilter(m_manager->settings());

    //  Free-user lock state
    m_isFreeUser = (m_manager->accountType() == AccountType::Free);
    connect(m_manager, &VpnManager::accountTypeReady, this, [this](AccountType type) {
        m_isFreeUser = (type == AccountType::Free);
        updateConnectBtnLockState();
    });

    //  Wide / narrow container
    buildWideLayout(mainLayout);
    buildNarrowLayout(mainLayout);

    // Apply initial lock state now that buttons exist.
    updateConnectBtnLockState();

    //  Spinners
    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(SPINNER_INTERVAL_MS);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]() {
        m_spinnerFrame = (m_spinnerFrame + 1) % kSpinnerFrameCount;
        if (m_citiesList != nullptr && m_citiesList->count() > 0)
            m_citiesList->item(0)->setText(
                tr("%1 Loading cities\u2026").arg(QString::fromUtf8(kSpinnerFrames[m_spinnerFrame])));
    });

    m_countriesSpinnerTimer = new QTimer(this);
    m_countriesSpinnerTimer->setInterval(SPINNER_INTERVAL_MS);
    connect(m_countriesSpinnerTimer, &QTimer::timeout, this, [this]() {
        m_countriesSpinnerFrame = (m_countriesSpinnerFrame + 1) % kSpinnerFrameCount;
        const QString frame = QString::fromUtf8(kSpinnerFrames[m_countriesSpinnerFrame]);
        if (m_countriesList != nullptr && m_countriesList->count() > 0)
            m_countriesList->item(0)->setText(
                tr("%1 Loading countries\u2026").arg(frame));
        if (m_narrowLoadingLabel != nullptr)
            m_narrowLoadingLabel->setText(
                tr("%1 Loading countries\u2026").arg(frame));
    });

    //  VpnManager signals
    connect(m_manager, &VpnManager::countriesReady,
            this, &CountriesPage::onCountriesReady);
    connect(m_manager, &VpnManager::citiesReady, this,
            [this](const QString& code, const QList<QPair<QString,QString>>& cities) {
                onCitiesReady(code, cities);
            });

    // Determine initial layout mode based on current width
    m_narrowMode = width() < kNarrowThreshold;
    m_wideWidget->setVisible(m_narrowMode == false);
    m_narrowWidget->setVisible(m_narrowMode);

    // Initial loading state until countriesReady arrives.
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(tr("Loading\u2026"));

    if (m_countriesList != nullptr)
    {
        m_countriesList->clear();
        m_countriesSpinnerFrame = 0;
        auto* loadingItem = new QListWidgetItem(tr("\u280b Loading countries\u2026"));
        loadingItem->setFlags(Qt::NoItemFlags);
        loadingItem->setForeground(QColor(0x99, 0x99, 0xbb));
        m_countriesList->addItem(loadingItem);
    }

    // While countries are loading, reset the right pane header.
    if (m_citiesLabel != nullptr)
    {
        m_citiesLabel->setText(tr("Cities"));
    }

    if (m_narrowLayout != nullptr)
    {
        m_narrowLoadingLabel = new QLabel(tr("\u280b Loading countries\u2026"), m_narrowContent);
        m_narrowLoadingLabel->setObjectName(QStringLiteral("infoLabel"));
        m_narrowLoadingLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        m_narrowLoadingLabel->setContentsMargins(0, 10, 0, 0); // extra top padding
        m_narrowLayout->insertWidget(0, m_narrowLoadingLabel, 0, Qt::AlignHCenter);
    }

    m_countriesSpinnerTimer->start();
}

void CountriesPage::setSplitView(const bool split)
{
    if (m_splitView == split) return;
    m_splitView = split;
    setProperty(SPLIT_VIEW_PROPERTY, split);
    if (split == true)
    {
        layout()->setContentsMargins(SPLIT_SIDE_MARGIN, SPLIT_TOP_MARGIN, PAGE_MARGIN, SPLIT_BOTTOM_MARGIN);
    }
    else
    {
        layout()->setContentsMargins(PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN);
    }

    // Countries above their cities beside the home page, which leaves each
    // pane the full width; side by side on the page of its own.
    QSplitter* splitter = split == true ? static_cast<QSplitter*>(m_stackSplitter) : m_listSplitter;
    splitter->addWidget(m_countriesPane);
    splitter->addWidget(m_citiesPane);
    m_stackSplitter->setVisible(split);
    m_listSplitter->setVisible(split == false);
    if (split == true)
    {
        resetStackSizes();
    }
    else
    {
        m_listSplitter->setSizes({LIST_PANE_W, LIST_PANE_W});
    }
    m_countriesList->setObjectName(split == true ? QStringLiteral("countryGrid") : QStringLiteral("serverList"));

    // The style rules hang off this page's property, and Qt only applies a
    // changed rule to a widget when it is polished again.
    const QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : widgets)
    {
        style()->unpolish(widget);
        style()->polish(widget);
    }

    // Beside the home page the panes always have the width the narrow
    // accordion is for, so it is not used there.
    const bool narrow = split == false && width() < kNarrowThreshold;
    if (narrow != m_narrowMode)
    {
        switchLayout(narrow);
    }
    // Rebuilt with the countries' cards or list icons, keeping the selection.
    if (m_allCountries.isEmpty() == false)
    {
        applyFilter();
    }
    updateCountriesListMode();
}

void CountriesPage::resetStackSizes() const
{
    m_stackSplitter->setSizes({STACK_COUNTRIES_SHARE, STACK_CITIES_SHARE});
}

// Beside the home page the countries are a grid of cards: the flag over the
// name, elided to the card. Elsewhere they are a list, as is the split view's
// list while it holds only its loading message, which a card could not show.
void CountriesPage::updateCountriesListMode() const
{
    const bool grid = m_splitView == true && m_countriesList->count() > 0
                   && m_countriesList->item(0)->data(Qt::UserRole).toString().isEmpty() == false;
    const bool isGrid = m_countriesList->viewMode() == QListView::IconMode;
    if (grid != isGrid)
    {
        if (grid == true)
        {
            // Icon mode also turns on wrapping and free movement; the cards
            // stay where the grid puts them.
            m_countriesList->setViewMode(QListView::IconMode);
            m_countriesList->setMovement(QListView::Static);
            m_countriesList->setResizeMode(QListView::Adjust);
            m_countriesList->setUniformItemSizes(true);
            m_countriesList->setIconSize(QSize(CARD_FLAG_W, CARD_FLAG_H));
            m_countriesList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        }
        else
        {
            m_countriesList->setViewMode(QListView::ListMode);
            m_countriesList->setResizeMode(QListView::Fixed);
            m_countriesList->setUniformItemSizes(false);
            m_countriesList->setGridSize(QSize());
            m_countriesList->setIconSize(QSize(COUNTRY_ICON_W, COUNTRY_ICON_H));
            m_countriesList->resetVerticalScrollMode();
        }
    }
    updateCountryGridSize();
}

void CountriesPage::updateCountryGridSize() const
{
    if (m_countriesList->viewMode() != QListView::IconMode) return;
    // As many whole cards as fit, stretched to share the row between them.
    // Each card fills its grid cell: on its own, a card is only as wide as
    // its flag and name need.
    const int available = m_countriesList->viewport()->width();
    const int columns = std::max(1, available / CARD_MIN_W);
    const QSize cell(available / columns, CARD_H);
    m_countriesList->setGridSize(cell);
    // Every time: a search or filter rebuilds the cards at their own size.
    for (int i = 0; i < m_countriesList->count(); ++i)
    {
        m_countriesList->item(i)->setSizeHint(cell);
    }
}

void CountriesPage::refreshAccordionIcons() const
{
    for (const AccordionItem& acc : m_accordion)
    {
        acc.headerBtn->setIcon(makeAccordionHeaderIcon(acc.headerBtn->property("countryCode").toString(),
                                                       acc.expanded,
                                                       acc.headerBtn->property("isPinned").toBool()));
    }
}

bool CountriesPage::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_countriesList->viewport() && event->type() == QEvent::Resize)
    {
        updateCountryGridSize();
    }
    // A right-click opens the location's menu and leaves the selection alone:
    // a left-click selects, and the cities shown and the Connect button
    // follow it, which a right-click selecting another row would leave behind.
    const bool listViewport = obj == m_countriesList->viewport()
                              || (m_citiesList != nullptr && obj == m_citiesList->viewport());
    if (listViewport == true
        && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
        && static_cast<const QMouseEvent*>(event)->button() == Qt::RightButton)
    {
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void CountriesPage::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        refreshAccordionIcons();
        updateBubbleStyles();
    }
}

// ============================================================
// buildWideLayout
// ============================================================
void CountriesPage::buildWideLayout(QVBoxLayout* parent)
{
    m_wideWidget = new QWidget(this);
    auto* wideLayout = new QVBoxLayout(m_wideWidget);
    wideLayout->setContentsMargins(0, 0, 0, 0);
    wideLayout->setSpacing(8);

    m_listSplitter = new QSplitter(Qt::Horizontal, m_wideWidget);

    // Countries panel
    m_countriesPane = new QWidget(m_listSplitter);
    auto* countriesLayout = new QVBoxLayout(m_countriesPane);
    countriesLayout->setContentsMargins(0, 0, 0, 0);
    m_countriesList = new QListWidget(m_countriesPane);
    m_countriesList->setObjectName(QStringLiteral("serverList"));
    // Wide mode uses a composite icon (star + flag) so reserve enough width.
    m_countriesList->setIconSize(QSize(COUNTRY_ICON_W, COUNTRY_ICON_H));
    // As a grid, its cards are sized to the viewport (see updateCountryGridSize()),
    // and a right-click selects none of them (see eventFilter()).
    m_countriesList->viewport()->installEventFilter(this);
    connect(m_countriesList, &QListWidget::itemClicked,
            this, &CountriesPage::onWideCountrySelected);
    // Right-clicking a country, a list row or a card, offers to favorite its
    // fastest server or show its properties.
    m_countriesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_countriesList, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
    {
        const QListWidgetItem* item = m_countriesList->itemAt(pos);
        if (item == nullptr) return;
        const QString code = item->data(Qt::UserRole).toString();
        if (code.isEmpty() == true) return; // the loading message
        ServerContextMenu::exec(m_manager, code, item->text(), QString(),
                                m_countriesList->viewport()->mapToGlobal(pos), window());
    });
    countriesLayout->addWidget(m_countriesList);
    m_listSplitter->addWidget(m_countriesPane);

    // Cities panel
    m_citiesPane = new QWidget(m_listSplitter);
    auto* citiesWidget = m_citiesPane;
    auto* citiesLayout = new QVBoxLayout(citiesWidget);
    citiesLayout->setContentsMargins(0, 0, 0, 0);
    citiesLayout->setSpacing(4);
    m_citiesLabel = new ElideLabel(tr("Cities"), citiesWidget);
    m_citiesLabel->setObjectName(QStringLiteral("listHeader"));
    citiesLayout->addWidget(m_citiesLabel);
    m_citiesList = new QListWidget(citiesWidget);
    m_citiesList->setObjectName(QStringLiteral("serverList"));
    m_citiesList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Rows laid out again on every resize: they carry widgets (feature icons,
    // the star) along their right edge, which otherwise keep the width the
    // list had when they were added and can end up out of sight.
    m_citiesList->setResizeMode(QListView::Adjust);
    // Keeps a right-click from selecting a row (see eventFilter()).
    m_citiesList->viewport()->installEventFilter(this);
    connect(m_citiesList, &QListWidget::itemClicked,
            this, &CountriesPage::onWideCitySelected);
    // The same for a city, or the country's fastest server.
    m_citiesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_citiesList, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
    {
        const QListWidgetItem* item = m_citiesList->itemAt(pos);
        // Not the loading and empty-state messages.
        if (item == nullptr || item->flags() == Qt::NoItemFlags || m_selectedCode.isEmpty() == true) return;
        ServerContextMenu::exec(m_manager, m_selectedCode, m_selectedCountry, item->data(Qt::UserRole).toString(),
                                m_citiesList->viewport()->mapToGlobal(pos), window());
    });
    citiesLayout->addWidget(m_citiesList);
    m_listSplitter->addWidget(citiesWidget);

    m_listSplitter->setSizes({LIST_PANE_W, LIST_PANE_W});
    wideLayout->addWidget(m_listSplitter, 1);

    // The split view's arrangement, which setSplitView() moves the panes into.
    m_stackSplitter = new GripSplitter(Qt::Vertical, m_wideWidget);
    m_stackSplitter->setChildrenCollapsible(false);
    m_stackSplitter->setVisible(false);
    connect(m_stackSplitter, &GripSplitter::handleDoubleClicked, this, &CountriesPage::resetStackSizes);
    wideLayout->addWidget(m_stackSplitter, 1);

    // Connect button
    m_connectBtn = new QPushButton(tr("Connect to Selected"), m_wideWidget);
    m_connectBtn->setObjectName(QStringLiteral("primaryButton"));
    m_connectBtn->setEnabled(false);
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_connectBtn, &QPushButton::clicked, this, [this]()
    {
        if (m_isFreeUser == false)
        {
            emit connectRequested(m_selectedCode, m_selectedCity);
        }
    });
    wideLayout->addWidget(m_connectBtn);

    parent->addWidget(m_wideWidget, 1);
}

// ============================================================
// buildNarrowLayout
// ============================================================
void CountriesPage::buildNarrowLayout(QVBoxLayout* parent)
{
    m_narrowWidget = new QWidget(this);
    auto* narrowOuterLayout = new QVBoxLayout(m_narrowWidget);
    narrowOuterLayout->setContentsMargins(0, 0, 0, 0);
    narrowOuterLayout->setSpacing(0);

    m_narrowScroll = new QScrollArea(m_narrowWidget);
    m_narrowScroll->setObjectName(QStringLiteral("countriesAccordion"));
    m_narrowScroll->setWidgetResizable(true);
    m_narrowScroll->setFrameShape(QFrame::NoFrame);

    m_narrowContent = new QWidget();
    m_narrowLayout = new QVBoxLayout(m_narrowContent);
    m_narrowLayout->setContentsMargins(0, 0, 0, 0);
    m_narrowLayout->setSpacing(0);
    m_narrowLayout->addStretch();

    m_narrowScroll->setWidget(m_narrowContent);
    narrowOuterLayout->addWidget(m_narrowScroll, 1);

    // Connect button for narrow mode
    auto* narrowConnectBtn = new QPushButton(tr("Connect to Selected"), m_narrowWidget);
    narrowConnectBtn->setObjectName(QStringLiteral("primaryButton"));
    narrowConnectBtn->setEnabled(false);
    narrowConnectBtn->setCursor(Qt::PointingHandCursor);
    // Share the same enabled/text state with the wide connect button by re-using m_connectBtn
    // (we keep them in sync manually)
    connect(narrowConnectBtn, &QPushButton::clicked, this, [this]()
    {
        if (m_isFreeUser == false)
        {
            emit connectRequested(m_selectedCode, m_selectedCity);
        }
    });
    // Store as the narrow connect button - we'll sync text/enabled with m_connectBtn
    narrowOuterLayout->addWidget(narrowConnectBtn);

    // Keep the narrow connect button pointer for sync
    m_narrowConnectBtn = narrowConnectBtn;
    m_narrowWidget->setProperty("connectBtn", QVariant::fromValue(static_cast<QObject*>(narrowConnectBtn)));

    parent->addWidget(m_narrowWidget, 1);
}

// ============================================================
// resizeEvent – switch layout at threshold
// ============================================================
void CountriesPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    const bool shouldBeNarrow = m_splitView == false && event->size().width() < kNarrowThreshold;
    if (shouldBeNarrow != m_narrowMode)
        switchLayout(shouldBeNarrow);
}

void CountriesPage::switchLayout(bool narrow)
{
    m_narrowMode = narrow;
    m_wideWidget->setVisible(narrow == false);
    m_narrowWidget->setVisible(narrow);

    // Re-populate whichever view just became visible
    if (m_allCountries.isEmpty() == false)
    {
        if (narrow == true)
        {
            populateNarrow();
        }
        else
        {
            populateWide();
        }
    }
}

// ============================================================
// refresh
// ============================================================
void CountriesPage::refresh()
{
    m_cityCache.clear();
    m_pendingCityCodes.clear();
    m_accordion.clear();
    m_selectedCode.clear();
    m_selectedCity.clear();
    m_selectedCountry.clear();

    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(tr("Loading\u2026"));

    if (m_connectBtn != nullptr)
    {
        m_connectBtn->setEnabled(false);
    }
    updateConnectBtnLockState();

    // Wide: show spinner
    if (m_countriesList != nullptr)
    {
        m_countriesList->clear();
        m_countriesSpinnerFrame = 0;
        auto* item = new QListWidgetItem(tr("\u280b Loading countries\u2026"));
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(QColor(0x99, 0x99, 0xbb));
        m_countriesList->addItem(item);
        updateCountriesListMode();
        m_countriesSpinnerTimer->start();
    }

    // While countries are loading, reset the right pane header and list.
    if (m_citiesLabel != nullptr)
    {
        m_citiesLabel->setText(tr("Cities"));
    }
    if (m_citiesList != nullptr)
    {
        m_citiesList->clear();
    }

    // Narrow: clear accordion
    while (m_narrowLayout->count() > 1)
    {
        const QLayoutItem* item = m_narrowLayout->takeAt(0);
        if (item->widget() != nullptr)
        {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Narrow: show loading text/spinner as well.
    if (m_narrowLoadingLabel == nullptr)
    {
        m_narrowLoadingLabel = new QLabel(tr("\u280b Loading countries\u2026"), m_narrowContent);
        m_narrowLoadingLabel->setObjectName(QStringLiteral("infoLabel"));
        m_narrowLoadingLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        m_narrowLoadingLabel->setContentsMargins(0, 10, 0, 0); // extra top padding
        m_narrowLayout->insertWidget(0, m_narrowLoadingLabel, 0, Qt::AlignHCenter);
    }

    m_manager->fetchCountries();
}

// ============================================================
// onCountriesReady
// ============================================================
void CountriesPage::onCountriesReady(const QMap<QString, QString>& countries)
{
    m_countriesSpinnerTimer->stop();
    m_allCountries = countries;
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText(tr("\u21bb Refresh"));

    if (m_narrowLoadingLabel != nullptr)
    {
        m_narrowLoadingLabel->deleteLater();
        m_narrowLoadingLabel = nullptr;
    }

    applyFilter();

    // Auto-select local country if detected
    if (m_localCountryCode.isEmpty() == false
        && m_narrowMode == false &&
        m_countriesList != nullptr)
    {
        for (int i = 0; i < m_countriesList->count(); ++i)
        {
            auto* item = m_countriesList->item(i);
            if (item != nullptr && item->data(Qt::UserRole).toString().compare(
                    m_localCountryCode, Qt::CaseInsensitive) == 0)
            {
                m_countriesList->setCurrentItem(item);
                onWideCountrySelected(item);
                break;
            }
        }
    }
}

// ============================================================
// applyFilter – rebuild the visible country lists
// ============================================================
void CountriesPage::applyFilter()
{
    const bool anyFilter = m_filterP2P || m_filterSecureCore || m_filterTor;
    const QString search = m_searchEdit != nullptr ? m_searchEdit->text() : QString();

    QList<QPair<QString,QString>> matching; // {name, code}
    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it)
    {
        const QString& name = it.key();
        const QString& code = it.value();
        if (search.isEmpty() == false
            && name.contains(search, Qt::CaseInsensitive) == false)
            continue;
        if (anyFilter == true && countryPassesFilter(code) == false)
            continue;
        matching.append({name, code});
    }

    m_countriesLabel->setText(tr("Countries (%1)").arg(matching.size()));

    if (m_narrowMode)
    {
        // If the selected country is no longer visible after filtering, clear its selection.
        if (m_selectedCode.isEmpty() == false && matching.isEmpty() == false)
        {
            const bool stillVisible = std::any_of(matching.constBegin(), matching.constEnd(),
                [this](const QPair<QString,QString>& p)
                {
                    return p.second.compare(m_selectedCode, Qt::CaseInsensitive) == 0;
                }
            );
            if (stillVisible == false)
            {
                m_selectedCode.clear();
                m_selectedCity.clear();
                m_selectedCountry.clear();
                if (m_narrowConnectBtn != nullptr)
                {
                    m_narrowConnectBtn->setEnabled(false);
                    m_narrowConnectBtn->setText(tr("Connect to Selected"));
                }
            }
        }
        populateNarrow();
        updateConnectBtnLockState();
        return;
    }

    // Wide mode: rebuild countries and then refresh/clear the city panel.
    populateWide();

    if (m_selectedCode.isEmpty())
    {
        if (m_citiesList != nullptr)
        {
            m_citiesList->clear();
            addWideInfoRow(m_citiesList, tr("Select a country to view cities."));
        }
        if (m_citiesLabel != nullptr)
        {
            m_citiesLabel->setText(tr("Cities"));
        }
        if (m_connectBtn != nullptr)
        {
            m_connectBtn->setEnabled(false);
            m_connectBtn->setText(tr("Connect to Selected"));
        }
        updateConnectBtnLockState();
        return;
    }

    QListWidgetItem* selectedItem = nullptr;
    if (m_countriesList != nullptr)
    {
        for (int i = 0; i < m_countriesList->count(); ++i)
        {
            QListWidgetItem* item = m_countriesList->item(i);
            if (item != nullptr && item->data(Qt::UserRole).toString() == m_selectedCode)
            {
                selectedItem = item;
                break;
            }
        }
    }

    if (selectedItem == nullptr)
    {
        // Keep the previously selected country context and show an empty-state
        // message instead of collapsing the header back to plain "Cities".
        m_selectedCity.clear();

        if (m_citiesList != nullptr)
        {
            m_citiesList->clear();
            addWideInfoRow(m_citiesList, tr("No servers in this country match the selected filters."));
        }

        if (m_citiesLabel != nullptr && m_selectedCountry.isEmpty() == false)
        {
            m_citiesLabel->setText(tr("Cities (0) \u2013 %1").arg(m_selectedCountry));
        }

        if (m_connectBtn != nullptr)
        {
            m_connectBtn->setEnabled(false);
            if (m_selectedCountry.isEmpty() == false)
            {
                m_connectBtn->setText(tr("Connect to %1").arg(m_selectedCountry));
            }
            else
            {
                m_connectBtn->setText(tr("Connect to Selected"));
            }
        }
        updateConnectBtnLockState();
        return;
    }

    // Keep selection and refresh cities to reflect the active feature filters.
    m_countriesList->setCurrentItem(selectedItem);
    onWideCountrySelected(selectedItem);
}

// Country passes if at least one city satisfies all selected feature filters.
// If we have no city cache yet, keep it visible until data arrives.
bool CountriesPage::countryPassesFilter(const QString& code) const
{
    if (m_filterP2P == false && m_filterSecureCore == false && m_filterTor == false)
        return true;

    if (m_cityCache.contains(code) == false)
        return true;

    const auto& cities = m_cityCache[code];
    for (const QString& val : cities | std::views::values)
    {
        const QString& features = val;
        if (cityPassesFilters(features, m_filterP2P, m_filterSecureCore, m_filterTor))
            return true;
    }
    return false;
}

// ============================================================
// showDisablePortForwardingDialog
// ============================================================
void CountriesPage::showDisablePortForwardingDialog(const QString& bodyText,
                                                    const std::function<void()>& onConfirm)
{
    QDialog* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Disable Port Forwarding?"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModal(true);

    QVBoxLayout* layout = new QVBoxLayout(dlg);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 20);

    QLabel* msg = new QLabel(bodyText, dlg);
    msg->setWordWrap(true);
    msg->setTextFormat(Qt::RichText);
    layout->addWidget(msg);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    QPushButton* goBackBtn = new QPushButton(tr("Go Back"), dlg);
    goBackBtn->setObjectName(QStringLiteral("secondaryButton"));

    QPushButton* disableBtn = new QPushButton(tr("Disable Port Forwarding"), dlg);
    disableBtn->setObjectName(QStringLiteral("primaryButton"));
    disableBtn->setDefault(true);

    // Match height to the primary button's natural height, same as the VPN page dialog.
    const int btnH = disableBtn->sizeHint().height();
    goBackBtn->setFixedHeight(btnH);
    disableBtn->setFixedHeight(btnH);

    // Stretch factor 1 on both so they share available width equally.
    btnRow->addWidget(goBackBtn, 1);
    btnRow->addWidget(disableBtn, 1);
    layout->addLayout(btnRow);

    // Ensure the dialog is wide enough that "Disable Port Forwarding" is never clipped.
    const int needed = disableBtn->sizeHint().width() * 2
                       + btnRow->spacing()
                       + layout->contentsMargins().left()
                       + layout->contentsMargins().right();
    dlg->setMinimumWidth(qMax(420, needed));

    connect(goBackBtn,  &QPushButton::clicked, dlg, &QDialog::reject);
    connect(disableBtn, &QPushButton::clicked, dlg, [this, dlg, onConfirm]()
    {
        m_manager->applyConfigValue(QStringLiteral("port-forwarding"),
                                    QStringLiteral("off"));
        onConfirm();
        dlg->accept();
    });

    dlg->exec();
}

void CountriesPage::updateBubbleStyles() const
{
    const bool anyActive = m_filterP2P || m_filterSecureCore || m_filterTor;
    m_bubbleAll->setStyleSheet(bubbleStyle(!anyActive));
    m_bubbleP2P->setStyleSheet(bubbleStyle(m_filterP2P));
    m_bubbleSecureCore->setStyleSheet(bubbleStyle(m_filterSecureCore));
    m_bubbleTor->setStyleSheet(bubbleStyle(m_filterTor));
}

// ============================================================
// updateConnectBtnLockState
// ============================================================
void CountriesPage::updateConnectBtnLockState() const
{
    // Locked style: muted dark button with a subtle purple tint - looks
    // "disabled" but stays interactive so the hover tooltip is reachable.
    static const QString kLockedStyle = QStringLiteral(
        "QPushButton {"
        "  background-color: #2a2a45;"
        "  color: #6a6a85;"
        "  border: 1px solid #3a3a55;"
        "  border-radius: 6px;"
        "  padding: 10px 24px;"
        "  font-weight: bold;"
        "  font-size: 14px;"
        "  min-width: 140px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #2e2e4e;"
        "  color: #8888aa;"
        "  border-color: rgba(109,74,255,0.45);"
        "}");

    static const QString kLockedTooltip = tr(
        "Country and city selection requires Proton VPN Plus.\n"
        "Free accounts are connected to a server chosen by Proton.\n"
        "Upgrade your plan to choose any country or city.");

    const QPixmap lockPix = GeoUtils::svgPixmap(QStringLiteral(":/assets/lock-fill.svg"), 14, QColor(0xAA, 0xAA, 0xAA));
    const QIcon   lockIcon(lockPix);

    auto applyLock = [&](QPushButton* btn)
    {
        if (btn == nullptr) return;

        btn->setEnabled(true);           // keep enabled so tooltip fires on hover
        btn->setText(tr("Plus Only"));
        btn->setIcon(lockIcon);
        btn->setIconSize(QSize(14, 14));
        btn->setStyleSheet(kLockedStyle);
        btn->setToolTip(kLockedTooltip);
        btn->setCursor(Qt::ForbiddenCursor);
    };

    auto clearLock = [](QPushButton* btn)
    {
        if (btn == nullptr) return;

        btn->setIcon(QIcon());
        btn->setStyleSheet(QString());   // restore QSS object-name styling
        btn->setToolTip(QString());
        btn->setCursor(Qt::PointingHandCursor);
    };

    if (m_isFreeUser)
    {
        applyLock(m_connectBtn);
        applyLock(m_narrowConnectBtn);
    }
    else
    {
        clearLock(m_connectBtn);
        clearLock(m_narrowConnectBtn);
    }
}

// ============================================================
// populateWide – rebuild the countries QListWidget
// ============================================================
void CountriesPage::populateWide() const
{
    if (m_countriesList == nullptr) return;
    m_countriesList->clear();

    const bool anyFilter = m_filterP2P || m_filterSecureCore || m_filterTor;
    const QString search = m_searchEdit != nullptr ? m_searchEdit->text() : QString();

    QListWidgetItem* pinnedItem = nullptr;
    QList<QListWidgetItem*> others;

    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it)
    {
        const QString& name = it.key();
        const QString& code = it.value();
        if (search.isEmpty() == false && name.contains(search, Qt::CaseInsensitive) == false)
            continue;
        if (anyFilter && countryPassesFilter(code) == false)
            continue;

        const bool isPinned = (!m_localCountryCode.isEmpty() &&
                               code.compare(m_localCountryCode, Qt::CaseInsensitive) == 0);

        QListWidgetItem* item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, code);
        item->setData(Qt::UserRole + 10, isPinned);
        if (m_splitView == true)
        {
            item->setIcon(makeCountryCardIcon(code, isPinned));
            // The card elides the name.
            item->setToolTip(name);
        }
        else
        {
            item->setIcon(makeCountryListIcon(code, isPinned));
            item->setSizeHint(QSize(0, LIST_ROW_HEIGHT));
        }

        if (isPinned)
        {
            pinnedItem = item;
        }
        else
        {
            others.append(item);
        }
    }

    if (pinnedItem != nullptr)
    {
        m_countriesList->addItem(pinnedItem);
    }

    for (auto* it : others)
    {
        m_countriesList->addItem(it);
    }
    updateCountriesListMode();
}

// ============================================================
// populateNarrow – rebuild accordion
// ============================================================
void CountriesPage::populateNarrow()
{
    // Remove all except the trailing stretch
    while (m_narrowLayout->count() > 1)
    {
        const QLayoutItem* layoutItem = m_narrowLayout->takeAt(0);
        if (layoutItem->widget() != nullptr)
        {
            layoutItem->widget()->deleteLater();
        }

        delete layoutItem;
    }
    m_accordion.clear();

    const bool anyFilter = m_filterP2P || m_filterSecureCore || m_filterTor;
    const QString search = m_searchEdit != nullptr ? m_searchEdit->text() : QString();

    // Pinned country first
    QString pinnedCode;
    if (m_localCountryCode.isEmpty() == false)
    {
        pinnedCode = m_localCountryCode.toUpper();
    }

    auto addAccordion = [&](const QString& name, const QString& code)
    {
        QWidget* container = new QWidget(m_narrowContent);
        QVBoxLayout* containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        // Header button
        QToolButton* btn = new QToolButton(container);
        btn->setObjectName(QStringLiteral("accordionHeader"));
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setFixedHeight(42);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

        const bool isPinned = (code.compare(pinnedCode, Qt::CaseInsensitive) == 0);
        const QString displayName = name;
        btn->setProperty("baseText", displayName);
        btn->setProperty("countryCode", code);
        btn->setProperty("isPinned", isPinned);
        btn->setText(displayName);
        btn->setIcon(makeAccordionHeaderIcon(code, false, isPinned));
        btn->setIconSize({isPinned ? 48 : 38, 16});

        containerLayout->addWidget(btn);

        // Cities area (hidden initially)
        QWidget* citiesWidget = new QWidget(container);
        citiesWidget->setVisible(false);
        QVBoxLayout* citiesLayout = new QVBoxLayout(citiesWidget);
        citiesLayout->setContentsMargins(24, 0, 0, 0);
        citiesLayout->setSpacing(0);
        containerLayout->addWidget(citiesWidget);

        AccordionItem acc;
        acc.headerBtn    = btn;
        acc.citiesWidget = citiesWidget;
        acc.citiesLayout = citiesLayout;
        acc.expanded     = false;
        m_accordion.insert(code, acc);

        connect(btn, &QToolButton::clicked, this, [this, code]()
        {
            toggleAccordion(code);
        });
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QWidget::customContextMenuRequested, this, [this, btn, code, displayName](const QPoint& pos)
        {
            ServerContextMenu::exec(m_manager, code, displayName, QString(), btn->mapToGlobal(pos), window());
        });

        // Insert before the stretch
        m_narrowLayout->insertWidget(m_narrowLayout->count() - 1, container);
    };

    // Insert pinned first
    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it)
    {
        if (it.value().compare(pinnedCode, Qt::CaseInsensitive) != 0) continue;
        if (search.isEmpty() == false && it.key().contains(search, Qt::CaseInsensitive) == false) continue;
        if (anyFilter && !countryPassesFilter(it.value())) continue;

        addAccordion(it.key(), it.value());
    }
    // Then the rest alphabetically
    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it)
    {
        if (it.value().compare(pinnedCode, Qt::CaseInsensitive) == 0) continue;
        if (search.isEmpty() == false && it.key().contains(search, Qt::CaseInsensitive) == false) continue;
        if (anyFilter && !countryPassesFilter(it.value())) continue;

        addAccordion(it.key(), it.value());
    }
}

// ============================================================
// onCitiesReady
// ============================================================
void CountriesPage::onCitiesReady(const QString& code,
                                  const QList<QPair<QString,QString>>& cities)
{
    m_cityCache.insert(code, cities);
    m_pendingCityCodes.remove(code);

    const bool anyFilter = m_filterP2P || m_filterSecureCore || m_filterTor;
    if (anyFilter && code.compare(m_selectedCode, Qt::CaseInsensitive) != 0)
    {
        // Re-apply only for non-selected countries. Reapplying for the selected
        // country can recurse via applyFilter -> onWideCountrySelected -> onCitiesReady.
        applyFilter();
    }

    //  Wide mode: fill cities list if this is the selected country
    if (m_narrowMode == false && code.compare(m_selectedCode, Qt::CaseInsensitive) == 0)
    {
        m_spinnerTimer->stop();
        if (m_citiesList == nullptr) return;
        m_citiesList->clear();

        const QString displayName = m_allCountries.key(code, code);

        int filteredCount = 0;
        for (const QString& features : cities | std::views::values)
        {
            if (cityPassesFilters(features, m_filterP2P, m_filterSecureCore, m_filterTor) == false)
                continue;
            ++filteredCount;
        }

        if (m_citiesLabel != nullptr)
        {
            m_citiesLabel->setText(tr("Cities (%1) \u2013 %2")
                                   .arg(filteredCount).arg(displayName));
        }

        if ((m_filterP2P || m_filterSecureCore || m_filterTor) && filteredCount == 0)
        {
            addWideInfoRow(m_citiesList,
                           tr("No servers in this country match the selected filters."));
            return;
        }

        // "Fastest server" row (always available when not in a zero-match filtered state)
        QListWidgetItem* anyItem = new QListWidgetItem(tr("\u26a1  Fastest server"));
        anyItem->setData(Qt::UserRole, QString());
        anyItem->setToolTip(tr("Connects to the fastest server in this country."));
        anyItem->setForeground(QColor(0xab, 0x8f, 0xff));
        QFont f = m_citiesList->font(); f.setBold(true); f.setItalic(true);
        anyItem->setFont(f);
        anyItem->setBackground(QColor(0x6d, 0x4a, 0xff, 40));
        anyItem->setSizeHint(QSize(0, LIST_ROW_HEIGHT));
        m_citiesList->addItem(anyItem);

        for (const auto& [city, features] : cities)
        {
            if (cityPassesFilters(features, m_filterP2P, m_filterSecureCore, m_filterTor) == false)
                continue;

            addWideCityItem(city, features);
        }
    }

    //  Narrow mode: fill the accordion for this country
    if (m_narrowMode && m_accordion.contains(code))
    {
        const AccordionItem& acc = m_accordion[code];
        // Clear old placeholder
        while (acc.citiesLayout->count() > 0)
        {
            const QLayoutItem* li = acc.citiesLayout->takeAt(0);
            if (li->widget() != nullptr)
            {
                li->widget()->deleteLater();
            }

            delete li;
        }

        // Count how many cities pass the filter first.
        int added = 0;
        for (const QString& features : cities | std::views::values)
        {
            if (cityPassesFilters(features, m_filterP2P, m_filterSecureCore, m_filterTor) == false)
                continue;

            ++added;
        }

        // "Fastest server" row - always shown unless ALL cities were filtered out.
        if (added > 0 || !(m_filterP2P || m_filterSecureCore || m_filterTor))
        {
            addNarrowCityItem(acc.citiesLayout, QString(), QString(), code);
        }

        // Now add the actual city rows.
        added = 0;
        for (const auto& [city, features] : cities)
        {
            if (cityPassesFilters(features, m_filterP2P, m_filterSecureCore, m_filterTor) == false)
                continue;
            addNarrowCityItem(acc.citiesLayout, city, features, code);
            ++added;
        }

        if ((m_filterP2P || m_filterSecureCore || m_filterTor) && added == 0)
        {
            QLabel* msg = new QLabel(tr("No servers in this country match the selected filters."), acc.citiesWidget);
            msg->setObjectName(QStringLiteral("infoLabel"));
            msg->setWordWrap(true);
            acc.citiesLayout->addWidget(msg);
        }

        if (acc.expanded)
        {
            acc.citiesWidget->setVisible(true);
        }
    }
}

// ============================================================
// Wide city item
// ============================================================
void CountriesPage::addWideCityItem(const QString& city, const QString& features) const
{
    const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);

    QWidget* row = new QWidget();
    row->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout* hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(6);

    // Display name only - the raw city is what gets passed to the CLI, and it
    // is carried on the item's UserRole data by the caller.
    auto* cityLabel = new QLabel(GeoUtils::cityWithRegion(m_selectedCode, city), row);
    cityLabel->setObjectName(QStringLiteral("cityLabel"));
    hbox->addWidget(cityLabel, 0, Qt::AlignVCenter);
    hbox->addStretch();

    for (const auto& meta : kServerFeatures)
    {
        bool matched = false;
        for (const QString& tag : tags)
        {
            if (tag.trimmed().contains(QLatin1String(meta.keyword), Qt::CaseInsensitive))
            {
                matched = true; break;
            }
        }
        if (matched == false) continue;

        QLabel* iconLabel = new QLabel(row);
        iconLabel->setPixmap(GeoUtils::svgPixmap(QLatin1String(meta.resource), 16));
        iconLabel->setFixedSize(24, 24);
        iconLabel->setScaledContents(false);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setToolTip(translatedFeatureTooltip(meta));
        hbox->addWidget(iconLabel, 0, Qt::AlignVCenter);
    }

    // Star button (favorites)
    if (AppConfig::instance().favoritesEnabled() && !m_selectedCode.isEmpty())
    {
        hbox->addWidget(
            makeStarButton(m_selectedCode, m_selectedCountry, city, row),
            0, Qt::AlignVCenter);
    }

    QListWidgetItem* item = new QListWidgetItem();
    item->setData(Qt::UserRole, city);
    item->setSizeHint(QSize(0, LIST_ROW_HEIGHT));
    m_citiesList->addItem(item);
    m_citiesList->setItemWidget(item, row);
}

// ============================================================
// Narrow city item
// ============================================================
void CountriesPage::addNarrowCityItem(QVBoxLayout* layout, const QString& city,
                                       const QString& features, const QString& code)
{
    const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);

    QWidget* row = new QWidget();
    row->setObjectName(QStringLiteral("cityRow"));
    row->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(8, 6, 8, 6);
    hbox->setSpacing(6);

    const bool isFastest = city.isEmpty();
    QLabel* cityLabel = new QLabel(isFastest ? tr("\u26a1  Fastest server")
                                             : GeoUtils::cityWithRegion(code, city), row);
    cityLabel->setObjectName(QStringLiteral("cityLabel"));
    if (isFastest)
    {
        QFont f = cityLabel->font();
        f.setBold(true);
        f.setItalic(true);
        cityLabel->setFont(f);
        cityLabel->setProperty("fastest", true);
        // Tinted through style.qss (#cityRow[fastest]) instead of a local
        // stylesheet, which would also paint every label and icon in the row.
        row->setProperty("fastest", true);
    }
    hbox->addWidget(cityLabel, 1, Qt::AlignVCenter);

    for (const auto& meta : kServerFeatures)
    {
        bool matched = false;
        for (const QString& tag : tags)
        {
            if (tag.trimmed().contains(QLatin1String(meta.keyword), Qt::CaseInsensitive))
            {
                matched = true; break;
            }
        }
        if (matched == false) continue;

        QLabel* iconLabel = new QLabel(row);
        iconLabel->setPixmap(GeoUtils::svgPixmap(QLatin1String(meta.resource), 14));
        iconLabel->setFixedSize(20, 20);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setToolTip(translatedFeatureTooltip(meta));
        hbox->addWidget(iconLabel, 0, Qt::AlignVCenter);
    }

    // Star button (favorites) - narrow mode
    if (AppConfig::instance().favoritesEnabled())
    {
        const QString countryName = GeoUtils::countryCodeToName(code);
        hbox->addWidget(makeStarButton(code, countryName, city, row),
                        0, Qt::AlignVCenter);
    }

    // Connect button
    QPushButton* connectBtn = new QPushButton(tr("Connect"), row);
    connectBtn->setObjectName(QStringLiteral("secondaryButton"));
    connectBtn->setFixedHeight(26);
    connectBtn->setCursor(Qt::PointingHandCursor);
    connect(connectBtn, &QPushButton::clicked, this, [this, code, city]()
    {
        emit connectRequested(code, city);
    });
    hbox->addWidget(connectBtn, 0, Qt::AlignVCenter);

    row->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(row, &QWidget::customContextMenuRequested, this, [this, row, code, city](const QPoint& pos)
    {
        ServerContextMenu::exec(m_manager, code, GeoUtils::countryCodeToName(code), city,
                                row->mapToGlobal(pos), window());
    });

    layout->addWidget(row);

    // Divider
    QFrame* div = new QFrame();
    div->setFrameShape(QFrame::HLine);
    div->setObjectName(QStringLiteral("divider"));
    layout->addWidget(div);
}

// ============================================================
// Wide country selected
// ============================================================
void CountriesPage::onWideCountrySelected(const QListWidgetItem* item)
{
    // Country name text is now star-free; keep as-is.
    const QString displayName = item->text();

    m_selectedCountry = displayName;
    m_selectedCode    = item->data(Qt::UserRole).toString();
    m_selectedCity.clear();

    if (m_citiesList != nullptr)
    {
        m_citiesList->clear();
    }

    if (m_citiesLabel != nullptr)
    {
        m_citiesLabel->setText(tr("Cities \u2013 %1").arg(m_selectedCountry));
    }

    if (m_connectBtn != nullptr)
    {
        m_connectBtn->setEnabled(!m_selectedCode.isEmpty());
        m_connectBtn->setText(tr("Connect to %1").arg(m_selectedCountry));
    }

    if (m_selectedCode.isEmpty()) return;

    if (m_cityCache.contains(m_selectedCode))
    {
        onCitiesReady(m_selectedCode, m_cityCache[m_selectedCode]);
    }
    else
    {
        m_spinnerFrame = 0;
        QListWidgetItem* li = new QListWidgetItem(tr("\u280b Loading cities\u2026"));
        li->setFlags(Qt::NoItemFlags);
        li->setForeground(QColor(0x99, 0x99, 0xbb));
        if (m_citiesList != nullptr)
        {
            m_citiesList->addItem(li);
        }
        m_spinnerTimer->start();
        m_manager->fetchCities(m_selectedCode);
    }
    updateConnectBtnLockState();
}

void CountriesPage::onWideCitySelected(const QListWidgetItem* item)
{
    m_selectedCity = item->data(Qt::UserRole).toString();
    if (m_connectBtn == nullptr) return;

    if (m_selectedCity.isEmpty())
    {
        m_connectBtn->setText(tr("Connect to %1").arg(m_selectedCountry));
    }
    else
    {
        m_connectBtn->setText(tr("Connect to %1, %2")
                                  .arg(m_selectedCountry,
                                       GeoUtils::cityWithRegion(m_selectedCode, m_selectedCity)));
    }
    updateConnectBtnLockState();
}

// ============================================================
// Accordion helpers
// ============================================================
void CountriesPage::ensureCities(const QString& code)
{
    if (m_cityCache.contains(code) || m_pendingCityCodes.contains(code))
        return;

    m_pendingCityCodes.insert(code);
    m_manager->fetchCities(code);

    // Show a loading placeholder in the narrow accordion
    if (m_accordion.contains(code))
    {
        const AccordionItem& acc = m_accordion[code];
        QLabel* li = new QLabel(tr("\u280b Loading cities\u2026"), acc.citiesWidget);
        li->setObjectName(QStringLiteral("infoLabel"));
        acc.citiesLayout->addWidget(li);
    }
}

void CountriesPage::toggleAccordion(const QString& code)
{
    if (m_accordion.contains(code) == false) return;
    auto& acc = m_accordion[code];
    acc.expanded = !acc.expanded;

    const QString baseText = acc.headerBtn->property("baseText").toString();
    const QString headerCode = acc.headerBtn->property("countryCode").toString();
    const bool pinned = acc.headerBtn->property("isPinned").toBool();
    acc.headerBtn->setText(baseText);
    acc.headerBtn->setIcon(makeAccordionHeaderIcon(headerCode, acc.expanded, pinned));

    if (acc.expanded)
    {
        // Show immediately so loading text and then cities are visible.
        acc.citiesWidget->setVisible(true);
        ensureCities(code);

        // Update selection state
        m_selectedCode = code;
        m_selectedCity.clear();
        m_selectedCountry = m_allCountries.key(code, code);

        // Sync narrow connect button
        if (m_narrowConnectBtn != nullptr)
        {
            m_narrowConnectBtn->setEnabled(true);
            m_narrowConnectBtn->setText(tr("Connect to %1").arg(m_selectedCountry));
        }
        updateConnectBtnLockState();

        if (m_cityCache.contains(code))
        {
            // Cached path (common for local country): populate now, because no
            // citiesReady signal will fire to fill this accordion section.
            while (acc.citiesLayout->count() > 0)
            {
                const QLayoutItem* li = acc.citiesLayout->takeAt(0);
                if (li->widget() != nullptr)
                {
                    li->widget()->deleteLater();
                }
                delete li;
            }
            // "Fastest server" row first.
            addNarrowCityItem(acc.citiesLayout, QString(), QString(), code);
            for (const auto& [city, features] : m_cityCache[code])
            {
                addNarrowCityItem(acc.citiesLayout, city, features, code);
            }

            acc.citiesWidget->setVisible(true);
        }
        // if not yet cached, onCitiesReady will fill the already-visible container
    }
    else
    {
        acc.citiesWidget->setVisible(false);
    }
}
