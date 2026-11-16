#include "countriesPage.h"
#include "../appConfig.h"
#include "../cli/serverFilters.h"
#include "../geoUtils.h"
#include "../themeManager.h"
#include "../uiHelpers.h"
#include "../widgets/gripSplitter.h"
#include "../widgets/pixmapCache.h"
#include "../widgets/serverContextMenu.h"
#include "../widgets/starButton.h"

#include <algorithm>
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
#include <QScrollArea>
#include <QSplitter>
#include <QToolButton>

// kServerFeatures and the spinner frames are defined in uiHelpers.h

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

// Between the page's rows (title, search, filters, lists), the filter
// bubbles, the wide layout's lists and its button, and a pane's title and
// its list.
constexpr int PAGE_SPACING          = 10;
constexpr int BUBBLE_SPACING        = 6;
constexpr int WIDE_LAYOUT_SPACING   = 8;
constexpr int CITIES_PANE_SPACING   = 4;
constexpr int REFRESH_BUTTON_HEIGHT = 30;

// Wide country-list icon geometry: a fixed box so the star + flag pair is
// never downscaled, with the flag always at the same offset whether or not a
// star is drawn in front of it.
constexpr int COUNTRY_ICON_W      = 40;
constexpr int COUNTRY_ICON_H      = 16;
constexpr int COUNTRY_FLAG_X      = 12;
constexpr int COUNTRY_STAR_W      = 9;
constexpr int COUNTRY_FLAG_W      = 20;
constexpr int COUNTRY_FLAG_H      = 15;

// The local country's star: gold, bold, a point larger than the text around
// it and never smaller than this.
constexpr QColor PINNED_STAR_COLOR(0xff, 0xd2, 0x4a);
constexpr int    PINNED_STAR_FONT_GROWTH   = 1;
constexpr int    PINNED_STAR_FONT_MIN_SIZE = 10;

// Narrow accordion arrows, in the accordion's text color for each theme.
constexpr QColor ACCORDION_ARROW_ON_DARK(0xea, 0xea, 0xea);
constexpr QColor ACCORDION_ARROW_ON_LIGHT(0x1a, 0x1a, 0x2e);

// Narrow accordion header: its height, and its icon, which holds the arrow,
// then the local country's star, then the flag. The icon is wider with the
// star in it; the arrow is a point smaller than the text and never smaller
// than its minimum.
constexpr int ACCORDION_HEADER_HEIGHT       = 42;
constexpr int ACCORDION_ICON_W              = 38;
constexpr int ACCORDION_ICON_PINNED_W       = 48;
constexpr int ACCORDION_ARROW_W             = 10;
constexpr int ACCORDION_ARROW_FONT_SHRINK   = 1;
constexpr int ACCORDION_ARROW_FONT_MIN_SIZE = 8;
constexpr int ACCORDION_FLAG_X              = 12;
// The star's width and the padding after it.
constexpr int ACCORDION_STAR_ADVANCE        = 12;
// A country's cities are indented under its header.
constexpr int ACCORDION_CITIES_INDENT       = 24;

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
// Half a pixel, which puts a one pixel wide line on whole pixels.
constexpr qreal  HALF_PIXEL       = 0.5;
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

// The loading rows' text, and the space above the accordion's message.
constexpr QColor PLACEHOLDER_TEXT_COLOR(0x99, 0x99, 0xbb);
constexpr int    NARROW_STATUS_TOP_MARGIN = 10;

// A word-wrapped message row in a list: its margins, and its height, which
// is estimated generously (a minimum, and room beyond the text) so that list
// item padding and borders never clip it. The text wraps at the list's
// width less an inset, but never narrower than the minimum.
constexpr int INFO_ROW_H_MARGIN       = 10;
constexpr int INFO_ROW_V_MARGIN       = 12;
constexpr int INFO_ROW_MIN_WRAP_W     = 200;
constexpr int INFO_ROW_WRAP_INSET     = 24;
constexpr int INFO_ROW_MAX_TEXT_H     = 5000;
constexpr int INFO_ROW_MIN_HEIGHT     = 56;
constexpr int INFO_ROW_HEIGHT_PADDING = 36;

// The "Fastest server" row in the wide cities list.
constexpr QColor FASTEST_TEXT_COLOR(0xab, 0x8f, 0xff);
constexpr QColor FASTEST_ROW_TINT(0x6d, 0x4a, 0xff, 40);

// City rows: between a row's parts, and the feature icons in their boxes, in
// the wide list and in the accordion, whose rows also have margins and a
// button.
constexpr int CITY_ROW_SPACING         = 6;
constexpr int WIDE_FEATURE_ICON_SIZE   = 16;
constexpr int WIDE_FEATURE_BOX_SIZE    = 24;
constexpr int NARROW_FEATURE_ICON_SIZE = 14;
constexpr int NARROW_FEATURE_BOX_SIZE  = 20;
constexpr int NARROW_ROW_H_MARGIN      = 8;
constexpr int NARROW_ROW_V_MARGIN      = 6;
constexpr int NARROW_ROW_BUTTON_HEIGHT = 26;

// The lock on the connect buttons of a Free account. The style is a muted
// dark button with a subtle purple tint: it looks disabled but stays
// interactive, so the tooltip that explains the lock is reachable.
constexpr int    LOCK_ICON_SIZE = 14;
constexpr QColor LOCK_ICON_COLOR(0xaa, 0xaa, 0xaa);
constexpr const char* LOCKED_BUTTON_STYLE =
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
    "}";

// The "Disable Port Forwarding?" dialog: it is never narrower than its
// minimum, nor than its two equally wide buttons need.
constexpr int PF_DIALOG_SPACING        = 16;
constexpr int PF_DIALOG_MARGIN         = 24;
constexpr int PF_DIALOG_BOTTOM_MARGIN  = 20;
constexpr int PF_DIALOG_BUTTON_SPACING = 8;
constexpr int PF_DIALOG_BUTTON_COUNT   = 2;
constexpr int PF_DIALOG_MIN_WIDTH      = 420;

// Takes everything out of a layout, deleting its widgets. They are hidden
// first: they stay around until the event loop deletes them, and would
// otherwise show through what is put in their place until then.
void clearLayout(QLayout* layout)
{
    while (layout->count() > 0)
    {
        QLayoutItem* item = layout->takeAt(0);
        if (item->widget() != nullptr)
        {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        delete item;
    }
}

// A list row that says the list is loading: not a country or a city, so it
// cannot be selected.
QListWidgetItem* makeLoadingItem(const QString& text)
{
    QListWidgetItem* item = new QListWidgetItem(text);
    item->setFlags(Qt::NoItemFlags);
    item->setForeground(PLACEHOLDER_TEXT_COLOR);
    return item;
}

// Shared star styling so wide and narrow views match exactly.
void drawPinnedStar(QPainter& p, const QRect& r)
{
    QFont starFont = p.font();
    starFont.setBold(true);
    starFont.setPointSize(qMax(starFont.pointSize() + PINNED_STAR_FONT_GROWTH, PINNED_STAR_FONT_MIN_SIZE));
    p.setFont(starFont);
    p.setPen(PINNED_STAR_COLOR);
    p.drawText(r, Qt::AlignCenter, QStringLiteral("★"));
}

// Wide country-list icon: optional star, then flag.
QIcon makeCountryListIcon(const QString& countryCode, const bool pinned)
{
    QPixmap pm(COUNTRY_ICON_W, COUNTRY_ICON_H);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    if (pinned == true)
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
QIcon makeCountryCardIcon(const QString& countryCode, const bool pinned)
{
    QPixmap pm = blankPixmap(QSizeF(CARD_FLAG_W, CARD_FLAG_H), qApp->devicePixelRatio());
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    // Inset half a pixel, so the edge is drawn on whole pixels.
    const QRectF flagRect = QRectF(0, 0, CARD_FLAG_W, CARD_FLAG_H)
                                .adjusted(HALF_PIXEL, HALF_PIXEL, -HALF_PIXEL, -HALF_PIXEL);
    QPainterPath rounded;
    rounded.addRoundedRect(flagRect, CARD_FLAG_RADIUS, CARD_FLAG_RADIUS);
    p.setClipPath(rounded);
    p.drawPixmap(QPointF(0, 0), GeoUtils::svgPixmap(
        QStringLiteral(":/flags/") + countryCode.toLower(), CARD_FLAG_W, CARD_FLAG_H));
    p.setClipping(false);
    p.setPen(CARD_FLAG_EDGE);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(flagRect, CARD_FLAG_RADIUS, CARD_FLAG_RADIUS);

    if (pinned == true)
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

// The accordion header icon's width, which the local country's star adds to.
int accordionIconWidth(const bool pinned)
{
    return pinned == true ? ACCORDION_ICON_PINNED_W : ACCORDION_ICON_W;
}

// Header icon for narrow accordion: arrow indicator, optional star, then country flag.
QIcon makeAccordionHeaderIcon(const QString& countryCode,
                              const bool expanded,
                              const bool pinned)
{
    QPixmap pm(accordionIconWidth(pinned), COUNTRY_ICON_H);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    const QString arrow = expanded == true ? QStringLiteral("▾") : QStringLiteral("▸");
    const QColor arrowColor = ThemeManager::isDark() == true ? ACCORDION_ARROW_ON_DARK : ACCORDION_ARROW_ON_LIGHT;
    p.setPen(arrowColor);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(qMax(f.pointSize() - ACCORDION_ARROW_FONT_SHRINK, ACCORDION_ARROW_FONT_MIN_SIZE));
    p.setFont(f);
    p.drawText(QRect(0, 0, ACCORDION_ARROW_W, COUNTRY_ICON_H), Qt::AlignCenter, arrow);

    int x = ACCORDION_FLAG_X;
    if (pinned == true)
    {
        drawPinnedStar(p, QRect(x, 0, COUNTRY_STAR_W, COUNTRY_ICON_H));
        x += ACCORDION_STAR_ADVANCE;
    }

    const QPixmap flag = GeoUtils::svgPixmap(
        QStringLiteral(":/flags/") + countryCode.toLower(), COUNTRY_FLAG_W, COUNTRY_FLAG_H);
    if (flag.isNull() == false)
    {
        p.drawPixmap(x, 0, flag);
    }

    return QIcon(pm);
}

// Add a non-selectable, word-wrapped informational row to a QListWidget.
void addWideInfoRow(QListWidget* list, const QString& text)
{
    QListWidgetItem* item = new QListWidgetItem();
    item->setFlags(Qt::NoItemFlags);

    QWidget* row = new QWidget(list);
    QVBoxLayout* layout = new QVBoxLayout(row);
    layout->setContentsMargins(INFO_ROW_H_MARGIN, INFO_ROW_V_MARGIN, INFO_ROW_H_MARGIN, INFO_ROW_V_MARGIN);
    layout->setSpacing(0);

    QLabel* label = new QLabel(text, row);
    label->setObjectName(QStringLiteral("infoLabel"));
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(label);

    // Use a conservative wrapped-height estimate and a larger minimum to avoid
    // clipping with list item padding/borders in styled views.
    const int wrapW = qMax(INFO_ROW_MIN_WRAP_W, list->viewport()->width() - INFO_ROW_WRAP_INSET);
    const QRect textRect = label->fontMetrics().boundingRect(
        QRect(0, 0, wrapW, INFO_ROW_MAX_TEXT_H), Qt::TextWordWrap, text);
    const int rowH = qMax(INFO_ROW_MIN_HEIGHT, textRect.height() + INFO_ROW_HEIGHT_PADDING);
    item->setSizeHint(QSize(0, rowH));

    list->addItem(item);
    list->setItemWidget(item, row);
}

// Adds an icon to a city's row for each feature its servers have.
void addFeatureIcons(QHBoxLayout* layout, QWidget* row, const QString& features,
                     const int iconSize, const int boxSize)
{
    for (const FeatureMeta& meta : kServerFeatures)
    {
        if (ServerFilters::hasFeature(features, meta.keyword) == false) continue;

        QLabel* iconLabel = new QLabel(row);
        iconLabel->setPixmap(GeoUtils::svgPixmap(QLatin1String(meta.resource), iconSize));
        iconLabel->setFixedSize(boxSize, boxSize);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setToolTip(translatedFeatureTooltip(meta));
        layout->addWidget(iconLabel, 0, Qt::AlignVCenter);
    }
}
} // namespace

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
    if (active == true)
    {
        return QStringLiteral(
            "QPushButton { background: #6d4aff; color: white; border: 1px solid #6d4aff; "
            "border-radius: 12px; padding: 3px 12px; min-height: 16px; font-size: 12px; }"
            "QPushButton:hover { background: #7d5aff; }");
    }
    // Inactive bubbles are outlined in the theme's own colors, which the
    // light theme needs darker to show at all.
    if (ThemeManager::isDark() == false)
    {
        return QStringLiteral(
            "QPushButton { background: transparent; color: #5a5a7a; border: 1px solid #c8c8d8; "
            "border-radius: 12px; padding: 3px 12px; min-height: 16px; font-size: 12px; }"
            "QPushButton:hover { border-color: #6d4aff; color: #1a1a2e; }");
    }
    return QStringLiteral(
        "QPushButton { background: transparent; color: #aaaacc; border: 1px solid #444466; "
        "border-radius: 12px; padding: 3px 12px; min-height: 16px; font-size: 12px; }"
        "QPushButton:hover { border-color: #6d4aff; color: #ccccee; }");
}

// ============================================================
// Constructor
// ============================================================
CountriesPage::CountriesPage(VpnManager* manager, QWidget* parent, const Mode mode)
    : QWidget(parent), m_manager(manager), m_mode(mode),
      m_localCountryCode(GeoUtils::detectUserCountry())
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN, PAGE_MARGIN);
    mainLayout->setSpacing(PAGE_SPACING);

    //  Header row
    QHBoxLayout* headerRow = new QHBoxLayout();

    m_countriesLabel = new QLabel(tr("Countries"), this);
    m_countriesLabel->setObjectName(QStringLiteral("sectionTitle"));
    headerRow->addWidget(m_countriesLabel);
    headerRow->addStretch();

    m_refreshBtn = new QPushButton(tr("↻ Refresh"), this);
    m_refreshBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_refreshBtn->setFixedHeight(REFRESH_BUTTON_HEIGHT);
    connect(m_refreshBtn, &QPushButton::clicked, this, &CountriesPage::refresh);
    headerRow->addWidget(m_refreshBtn);
    mainLayout->addLayout(headerRow);

    //  Search
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("inputField"));
    m_searchEdit->setPlaceholderText(tr("Search countries…"));
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString&)
    {
        applyFilter();
    });
    mainLayout->addWidget(m_searchEdit);

    //  Filter bubbles
    QHBoxLayout* bubblesRow = new QHBoxLayout();
    bubblesRow->setSpacing(BUBBLE_SPACING);

    auto makeBubble = [this, bubblesRow](const QString& label) -> QPushButton*
    {
        QPushButton* btn = new QPushButton(label, this);
        btn->setCursor(Qt::PointingHandCursor);
        bubblesRow->addWidget(btn);
        return btn;
    };

    m_bubbleAll        = makeBubble(tr("All"));
    m_bubbleP2P        = makeBubble(tr("P2P"));
    m_bubbleSecureCore = makeBubble(tr("Secure Core"));
    m_bubbleTor        = makeBubble(tr("Tor"));
    bubblesRow->addStretch();
    mainLayout->addLayout(bubblesRow);

    updateBubbleStyles();

    connect(m_bubbleAll, &QPushButton::clicked, this, [this]()
    {
        if (m_portForwardingEnabled == true && m_filterP2P == true)
        {
            showDisablePortForwardingDialog(
                tr("The <b>P2P</b> filter is active because <b>Port Forwarding</b> "
                   "is enabled in your settings.<br><br>"
                   "Clearing all filters will also remove the P2P filter, which "
                   "requires disabling Port Forwarding. Would you like to do that?"),
                [this]()
                {
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
    connect(m_bubbleP2P, &QPushButton::clicked, this, [this]()
    {
        if (m_filterP2P == true && m_portForwardingEnabled == true)
        {
            showDisablePortForwardingDialog(
                tr("The <b>P2P</b> filter is active because <b>Port Forwarding</b> "
                   "is enabled in your settings.<br><br>"
                   "To turn off this filter you need to disable Port Forwarding. "
                   "Would you like to do that?"),
                [this]()
                {
                    m_portForwardingEnabled = false;
                    m_filterP2P = false;
                    updateBubbleStyles();
                    applyFilter();
                });
            return;
        }

        // Normal toggle (port forwarding is not driving this filter)
        m_filterP2P = (m_filterP2P == false);
        updateBubbleStyles();
        applyFilter();
    });
    connect(m_bubbleSecureCore, &QPushButton::clicked, this, [this]()
    {
        m_filterSecureCore = (m_filterSecureCore == false);
        updateBubbleStyles();
        applyFilter();
    });
    connect(m_bubbleTor, &QPushButton::clicked, this, [this]()
    {
        m_filterTor = (m_filterTor == false);
        updateBubbleStyles();
        applyFilter();
    });

    //  Wide / narrow container
    m_isFreeUser = (m_manager->accountType() == AccountType::Free);
    buildWideLayout(mainLayout);
    buildNarrowLayout(mainLayout);

    //  Spinners
    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(SPINNER_INTERVAL_MS);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_spinnerFrame = (m_spinnerFrame + 1) % kSpinnerFrameCount;
        // Only ever running while the first row is the loading row: whatever
        // else fills the pane stops it first (see clearWideCities()).
        if (m_citiesList->count() > 0)
        {
            m_citiesList->item(0)->setText(loadingCitiesText());
        }
    });

    m_countriesSpinnerTimer = new QTimer(this);
    m_countriesSpinnerTimer->setInterval(SPINNER_INTERVAL_MS);
    connect(m_countriesSpinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_countriesSpinnerFrame = (m_countriesSpinnerFrame + 1) % kSpinnerFrameCount;
        // While loading, the list's one row is the loading row.
        if (m_countriesState != ListState::Loading || m_countriesList->count() == 0) return;
        const QString text = loadingCountriesText();
        m_countriesList->item(0)->setText(text);
        m_narrowStatusLabel->setText(text);
    });

    //  VpnManager signals
    connect(m_manager, &VpnManager::countriesReady,
            this, &CountriesPage::onCountriesReady);
    connect(m_manager, &VpnManager::countriesFetchFailed,
            this, &CountriesPage::onCountriesFetchFailed);
    connect(m_manager, &VpnManager::citiesReady,
            this, &CountriesPage::onCitiesReady);
    connect(m_manager, &VpnManager::citiesFetchFailed,
            this, &CountriesPage::onCitiesFetchFailed);
    // The manager re-reads the settings at sign-in and after every change, so
    // this also follows port forwarding being toggled from the Settings page.
    connect(m_manager, &VpnManager::settingsReady, this, &CountriesPage::onSettingsRead);
    connect(m_manager, &VpnManager::accountTypeReady, this, [this](const AccountType type)
    {
        m_isFreeUser = (type == AccountType::Free);
        updateConnectButtons();
        const QList<QPushButton*> rowButtons = m_narrowContent->findChildren<QPushButton*>();
        for (QPushButton* button : rowButtons)
        {
            updateRowButtonLock(button);
        }
    });

    // Determine initial layout mode based on current width
    m_narrowMode = width() < NARROW_THRESHOLD;
    m_wideWidget->setVisible(m_narrowMode == false);
    m_narrowWidget->setVisible(m_narrowMode);

    // Loading until countriesReady arrives.
    showCountriesLoading();

    // Seed the initial port-forwarding state.
    onSettingsRead(m_manager->settings());
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
    const bool narrow = split == false && width() < NARROW_THRESHOLD;
    if (narrow != m_narrowMode)
    {
        switchLayout(narrow);
    }
    else
    {
        // Rebuilt with the countries' cards or list icons, keeping the selection.
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
    const bool listViewport = obj == m_countriesList->viewport() || obj == m_citiesList->viewport();
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
    QVBoxLayout* wideLayout = new QVBoxLayout(m_wideWidget);
    wideLayout->setContentsMargins(0, 0, 0, 0);
    wideLayout->setSpacing(WIDE_LAYOUT_SPACING);

    m_listSplitter = new QSplitter(Qt::Horizontal, m_wideWidget);

    // Countries panel
    m_countriesPane = new QWidget(m_listSplitter);
    QVBoxLayout* countriesLayout = new QVBoxLayout(m_countriesPane);
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
    QVBoxLayout* citiesLayout = new QVBoxLayout(m_citiesPane);
    citiesLayout->setContentsMargins(0, 0, 0, 0);
    citiesLayout->setSpacing(CITIES_PANE_SPACING);
    m_citiesLabel = new ElideLabel(tr("Cities"), m_citiesPane);
    m_citiesLabel->setObjectName(QStringLiteral("listHeader"));
    citiesLayout->addWidget(m_citiesLabel);
    m_citiesList = new QListWidget(m_citiesPane);
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
    m_listSplitter->addWidget(m_citiesPane);

    m_listSplitter->setSizes({LIST_PANE_W, LIST_PANE_W});
    wideLayout->addWidget(m_listSplitter, 1);

    // The split view's arrangement, which setSplitView() moves the panes into.
    m_stackSplitter = new GripSplitter(Qt::Vertical, m_wideWidget);
    m_stackSplitter->setChildrenCollapsible(false);
    m_stackSplitter->setVisible(false);
    connect(m_stackSplitter, &GripSplitter::handleDoubleClicked, this, &CountriesPage::resetStackSizes);
    wideLayout->addWidget(m_stackSplitter, 1);

    // Connect button
    m_connectBtn = new QPushButton(noSelectionText(), m_wideWidget);
    m_connectBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(m_connectBtn, &QPushButton::clicked, this, &CountriesPage::onConnectClicked);
    wideLayout->addWidget(m_connectBtn);

    parent->addWidget(m_wideWidget, 1);
}

// ============================================================
// buildNarrowLayout
// ============================================================
void CountriesPage::buildNarrowLayout(QVBoxLayout* parent)
{
    m_narrowWidget = new QWidget(this);
    QVBoxLayout* narrowOuterLayout = new QVBoxLayout(m_narrowWidget);
    narrowOuterLayout->setContentsMargins(0, 0, 0, 0);
    narrowOuterLayout->setSpacing(0);

    QScrollArea* scroll = new QScrollArea(m_narrowWidget);
    scroll->setObjectName(QStringLiteral("countriesAccordion"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    // The loading and failure message, then the accordion's sections, then
    // the room left over.
    m_narrowContent = new QWidget();
    QVBoxLayout* contentLayout = new QVBoxLayout(m_narrowContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_narrowStatusLabel = new QLabel(m_narrowContent);
    m_narrowStatusLabel->setObjectName(QStringLiteral("infoLabel"));
    m_narrowStatusLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    m_narrowStatusLabel->setWordWrap(true);
    m_narrowStatusLabel->setContentsMargins(0, NARROW_STATUS_TOP_MARGIN, 0, 0);
    m_narrowStatusLabel->setVisible(false);
    contentLayout->addWidget(m_narrowStatusLabel);

    m_narrowLayout = new QVBoxLayout();
    m_narrowLayout->setContentsMargins(0, 0, 0, 0);
    m_narrowLayout->setSpacing(0);
    contentLayout->addLayout(m_narrowLayout);
    contentLayout->addStretch();

    scroll->setWidget(m_narrowContent);
    narrowOuterLayout->addWidget(scroll, 1);

    // The accordion's own connect button. It always shows what the wide
    // layout's does: updateConnectButtons() sets both.
    m_narrowConnectBtn = new QPushButton(noSelectionText(), m_narrowWidget);
    m_narrowConnectBtn->setObjectName(QStringLiteral("primaryButton"));
    connect(m_narrowConnectBtn, &QPushButton::clicked, this, &CountriesPage::onConnectClicked);
    narrowOuterLayout->addWidget(m_narrowConnectBtn);

    parent->addWidget(m_narrowWidget, 1);
}

// ============================================================
// resizeEvent – switch layout at threshold
// ============================================================
void CountriesPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    const bool shouldBeNarrow = m_splitView == false && event->size().width() < NARROW_THRESHOLD;
    if (shouldBeNarrow != m_narrowMode)
    {
        switchLayout(shouldBeNarrow);
    }
}

void CountriesPage::switchLayout(const bool narrow)
{
    m_narrowMode = narrow;
    m_wideWidget->setVisible(narrow == false);
    m_narrowWidget->setVisible(narrow);

    // Rebuilds the layout that just came on screen around the selection, as
    // it may have changed while the other layout was the one shown.
    applyFilter();
}

// ============================================================
// Loading state
// ============================================================
QString CountriesPage::loadingCountriesText() const
{
    return tr("%1 Loading countries…").arg(QString::fromUtf8(kSpinnerFrames[m_countriesSpinnerFrame]));
}

QString CountriesPage::loadingCitiesText() const
{
    return tr("%1 Loading cities…").arg(QString::fromUtf8(kSpinnerFrames[m_spinnerFrame]));
}

// Both layouts show the loading message and nothing else: the lists are
// emptied here, and stay empty until the countries are in (applyFilter()
// builds nothing before that), so the message is never mixed with countries.
void CountriesPage::showCountriesLoading()
{
    m_countriesState = ListState::Loading;
    m_countriesSpinnerFrame = 0;
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(tr("Loading…"));
    m_countriesLabel->setText(tr("Countries"));

    const QString text = loadingCountriesText();
    m_countriesList->clear();
    m_countriesList->addItem(makeLoadingItem(text));
    updateCountriesListMode();
    clearWideCities();
    m_citiesLabel->setText(tr("Cities"));

    clearAccordion();
    m_narrowStatusLabel->setText(text);
    m_narrowStatusLabel->setVisible(true);

    m_countriesSpinnerTimer->start();
    updateConnectButtons();
}

void CountriesPage::showCountriesFailed()
{
    m_countriesSpinnerTimer->stop();
    m_countriesState = ListState::Failed;
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText(tr("↻ Refresh"));

    const QString text = tr("The countries could not be loaded. Press Refresh to try again.");
    m_countriesList->clear();
    addWideInfoRow(m_countriesList, text);
    updateCountriesListMode();

    m_narrowStatusLabel->setText(text);
    m_narrowStatusLabel->setVisible(true);
    updateConnectButtons();
}

// ============================================================
// refresh
// ============================================================
void CountriesPage::refresh()
{
    m_allCountries.clear();
    m_cityCache.clear();
    m_pendingCityCodes.clear();
    m_selectedCode.clear();
    m_selectedCity.clear();
    m_selectedCountry.clear();
    m_selectionHidden = false;

    showCountriesLoading();
    m_manager->fetchCountries();
}

void CountriesPage::startChoosing(const QString& countryCode, const QString& city)
{
    m_initialCode = countryCode;
    m_initialCity = city;
    if (m_manager->countries().isEmpty() == true)
    {
        refresh();
        return;
    }
    // From the main Countries page's last fetch: fetching again would also
    // set that page back to the local country.
    m_cityCache = m_manager->cities();
    onCountriesReady(m_manager->countries());
}

void CountriesPage::requestLocation(const QString& countryCode, const QString& city)
{
    if (m_mode == Mode::Choose)
    {
        emit locationChosen(countryCode, city);
        return;
    }
    // Every way to ask for a location comes through here, the accordion's
    // row buttons included, so a Free account is stopped here.
    if (isLocked() == true) return;
    emit connectRequested(countryCode, city);
}

void CountriesPage::onConnectClicked()
{
    // The buttons show the selection, so the selection is what they ask for;
    // with none there is nothing to ask for.
    if (m_selectedCode.isEmpty() == true) return;
    requestLocation(m_selectedCode, m_selectedCity);
}

QString CountriesPage::actionText(const QString& country, const QString& city) const
{
    if (m_mode == Mode::Choose)
    {
        return city.isEmpty() == true ? tr("Choose %1").arg(country) : tr("Choose %1, %2").arg(country, city);
    }
    return city.isEmpty() == true ? tr("Connect to %1").arg(country) : tr("Connect to %1, %2").arg(country, city);
}

QString CountriesPage::noSelectionText() const
{
    return m_mode == Mode::Choose ? tr("Choose a Location") : tr("Connect to Selected");
}

// ============================================================
// onCountriesReady
// ============================================================
void CountriesPage::onCountriesReady(const QMap<QString, QString>& countries)
{
    m_countriesSpinnerTimer->stop();
    m_countriesState = ListState::Ready;
    m_allCountries = countries;
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText(tr("↻ Refresh"));
    m_narrowStatusLabel->setVisible(false);

    // A selection the new list no longer has is no selection.
    if (m_selectedCode.isEmpty() == false && countries.key(m_selectedCode).isEmpty() == true)
    {
        m_selectedCode.clear();
        m_selectedCity.clear();
        m_selectedCountry.clear();
    }

    applyFilter();

    // Select the location being chosen from (see startChoosing()), or else
    // the local country if detected.
    const QString preferredCode = m_initialCode.isEmpty() == true ? m_localCountryCode : m_initialCode;
    bool selected = false;
    if (preferredCode.isEmpty() == false && m_narrowMode == false)
    {
        for (int i = 0; i < m_countriesList->count(); ++i)
        {
            QListWidgetItem* item = m_countriesList->item(i);
            if (item->data(Qt::UserRole).toString().compare(preferredCode, Qt::CaseInsensitive) == 0)
            {
                m_countriesList->setCurrentItem(item);
                m_countriesList->scrollToItem(item);
                selected = true;
                onWideCountrySelected(item);
                break;
            }
        }
    }
    // The city waits for the country's cities (see fillWideCities()), unless
    // the country is not there to select.
    if (selected == false || m_initialCity.isEmpty() == true)
    {
        m_initialCode.clear();
        m_initialCity.clear();
    }
}

// A list that is showing keeps showing: the failure is only news to a page
// that was waiting for this fetch.
void CountriesPage::onCountriesFetchFailed()
{
    if (m_countriesState != ListState::Loading) return;
    showCountriesFailed();
}

// ============================================================
// applyFilter – rebuild the visible country list around the selection
// ============================================================
void CountriesPage::applyFilter()
{
    // Nothing to filter until the countries are in; the search text and the
    // filters are applied when they arrive.
    if (m_countriesState != ListState::Ready) return;

    const CountryList matching = matchingCountries();
    m_countriesLabel->setText(tr("Countries (%1)").arg(matching.size()));

    // A selected country that is not listed stays selected: it is shown
    // again as soon as the search or the filters let it back in.
    const bool listed = std::ranges::any_of(matching, [this](const QPair<QString, QString>& country)
    {
        return country.second == m_selectedCode;
    });
    m_selectionHidden = m_selectedCode.isEmpty() == false && listed == false;

    if (m_narrowMode == true)
    {
        // The wide panes are rebuilt when they come back on screen.
        clearWideCities();
        populateNarrow(matching);
        updateConnectButtons();
        return;
    }

    populateWide(matching);

    if (m_selectedCode.isEmpty() == true)
    {
        clearWideCities();
        addWideInfoRow(m_citiesList, tr("Select a country to view cities."));
        m_citiesLabel->setText(tr("Cities"));
        updateConnectButtons();
        return;
    }

    if (m_selectionHidden == true)
    {
        // Keep the previously selected country context and show an empty-state
        // message instead of collapsing the header back to plain "Cities".
        clearWideCities();
        // Hidden by the search text when the feature filters would let the
        // country through; the message names what the user has to change.
        addWideInfoRow(m_citiesList, countryPassesFilter(m_selectedCode) == true
                           ? tr("This country does not match the search.")
                           : tr("No servers in this country match the selected filters."));
        m_citiesLabel->setText(tr("Cities (0) – %1").arg(m_selectedCountry));
        updateConnectButtons();
        return;
    }

    // Keep the selection, and show its cities as the feature filters leave them.
    for (int i = 0; i < m_countriesList->count(); ++i)
    {
        QListWidgetItem* item = m_countriesList->item(i);
        if (item->data(Qt::UserRole).toString() == m_selectedCode)
        {
            m_countriesList->setCurrentItem(item);
            break;
        }
    }
    showWideCities();
}

bool CountriesPage::anyFilterActive() const
{
    return m_filterP2P == true || m_filterSecureCore == true || m_filterTor == true;
}

bool CountriesPage::isPinned(const QString& code) const
{
    return m_localCountryCode.isEmpty() == false
        && code.compare(m_localCountryCode, Qt::CaseInsensitive) == 0;
}

// Country passes if at least one city satisfies all selected feature filters.
// If we have no city cache yet, keep it visible until data arrives.
bool CountriesPage::countryPassesFilter(const QString& code) const
{
    if (anyFilterActive() == false) return true;
    if (m_cityCache.contains(code) == false) return true;
    return filteredCities(m_cityCache.value(code)).isEmpty() == false;
}

CountriesPage::CountryList CountriesPage::matchingCountries() const
{
    const QString search = m_searchEdit->text();
    CountryList matching;
    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it)
    {
        const QString& name = it.key();
        const QString& code = it.value();
        if (search.isEmpty() == false && name.contains(search, Qt::CaseInsensitive) == false) continue;
        if (countryPassesFilter(code) == false) continue;

        if (isPinned(code) == true)
        {
            matching.prepend({name, code});
        }
        else
        {
            matching.append({name, code});
        }
    }
    return matching;
}

CountriesPage::CityList CountriesPage::filteredCities(const CityList& cities) const
{
    CityList passing;
    for (const QPair<QString, QString>& city : cities)
    {
        if (ServerFilters::cityPassesFilters(city.second, m_filterP2P, m_filterSecureCore, m_filterTor) == true)
        {
            passing.append(city);
        }
    }
    return passing;
}

void CountriesPage::onSettingsRead(const QMap<QString, QString>& settings)
{
    const bool forwarding = isOnString(settings.value(QStringLiteral("port-forwarding")).toLower().trimmed());
    const bool filterP2P = ServerFilters::p2pFilterAfterSettingsRead(m_portForwardingEnabled, forwarding, m_filterP2P);
    m_portForwardingEnabled = forwarding;
    if (filterP2P == m_filterP2P) return;

    m_filterP2P = filterP2P;
    updateBubbleStyles();
    applyFilter();
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
    layout->setSpacing(PF_DIALOG_SPACING);
    layout->setContentsMargins(PF_DIALOG_MARGIN, PF_DIALOG_MARGIN, PF_DIALOG_MARGIN, PF_DIALOG_BOTTOM_MARGIN);

    QLabel* msg = new QLabel(bodyText, dlg);
    msg->setWordWrap(true);
    msg->setTextFormat(Qt::RichText);
    layout->addWidget(msg);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(PF_DIALOG_BUTTON_SPACING);

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
    const int needed = disableBtn->sizeHint().width() * PF_DIALOG_BUTTON_COUNT
                       + btnRow->spacing()
                       + layout->contentsMargins().left()
                       + layout->contentsMargins().right();
    dlg->setMinimumWidth(qMax(PF_DIALOG_MIN_WIDTH, needed));

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
    m_bubbleAll->setStyleSheet(bubbleStyle(anyFilterActive() == false));
    m_bubbleP2P->setStyleSheet(bubbleStyle(m_filterP2P));
    m_bubbleSecureCore->setStyleSheet(bubbleStyle(m_filterSecureCore));
    m_bubbleTor->setStyleSheet(bubbleStyle(m_filterTor));
}

// ============================================================
// Connect buttons
// ============================================================
bool CountriesPage::isLocked() const
{
    return m_isFreeUser == true && m_mode == Mode::Connect;
}

QString CountriesPage::lockedTooltip() const
{
    return tr("Country and city selection requires Proton VPN Plus.\n"
              "Free accounts are connected to a server chosen by Proton.\n"
              "Upgrade your plan to choose any country or city.");
}

// The one place the connect buttons are given their text and state, from the
// selection alone. Both buttons, whichever layout is on screen: the other one
// is then right when its layout comes back, and what a button says is always
// what onConnectClicked() asks for.
void CountriesPage::updateConnectButtons() const
{
    const bool hasSelection = m_selectedCode.isEmpty() == false;
    QString text = noSelectionText();
    if (hasSelection == true && m_selectedCity.isEmpty() == true)
    {
        text = actionText(m_selectedCountry);
    }
    else if (hasSelection == true)
    {
        text = actionText(m_selectedCountry, GeoUtils::cityWithRegion(m_selectedCode, m_selectedCity));
    }
    // Not while the list is loading or failed to load, nor for a country the
    // search or the filters have taken out of the list.
    const bool enabled = hasSelection == true && m_selectionHidden == false
                      && m_countriesState == ListState::Ready;

    const bool locked = isLocked();
    QIcon lockIcon;
    if (locked == true)
    {
        lockIcon = QIcon(GeoUtils::svgPixmap(QStringLiteral(":/assets/lock-fill.svg"), LOCK_ICON_SIZE, LOCK_ICON_COLOR));
    }

    for (QPushButton* button : {m_connectBtn, m_narrowConnectBtn})
    {
        if (locked == true)
        {
            button->setEnabled(true); // keep enabled so tooltip fires on hover
            button->setText(tr("Plus Only"));
            button->setIcon(lockIcon);
            button->setIconSize(QSize(LOCK_ICON_SIZE, LOCK_ICON_SIZE));
            button->setStyleSheet(QString::fromLatin1(LOCKED_BUTTON_STYLE));
            button->setToolTip(lockedTooltip());
            button->setCursor(Qt::ForbiddenCursor);
        }
        else
        {
            button->setEnabled(enabled);
            button->setText(text);
            button->setIcon(QIcon());
            button->setStyleSheet(QString()); // restore QSS object-name styling
            button->setToolTip(QString());
            button->setCursor(Qt::PointingHandCursor);
        }
    }
}

// Like the big buttons, a locked row button stays enabled so that its
// tooltip is reachable; requestLocation() is what stops the click.
void CountriesPage::updateRowButtonLock(QPushButton* button) const
{
    const bool locked = isLocked();
    button->setToolTip(locked == true ? lockedTooltip() : QString());
    button->setCursor(locked == true ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
}

// ============================================================
// populateWide – rebuild the countries QListWidget
// ============================================================
void CountriesPage::populateWide(const CountryList& countries) const
{
    m_countriesList->clear();

    for (const auto& [name, code] : countries)
    {
        const bool pinned = isPinned(code);
        QListWidgetItem* item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, code);
        if (m_splitView == true)
        {
            item->setIcon(makeCountryCardIcon(code, pinned));
            // The card elides the name.
            item->setToolTip(name);
        }
        else
        {
            item->setIcon(makeCountryListIcon(code, pinned));
            item->setSizeHint(QSize(0, LIST_ROW_HEIGHT));
        }
        m_countriesList->addItem(item);
    }
    updateCountriesListMode();
}

// ============================================================
// populateNarrow – rebuild accordion
// ============================================================
void CountriesPage::clearAccordion()
{
    clearLayout(m_narrowLayout);
    m_accordion.clear();
}

void CountriesPage::populateNarrow(const CountryList& countries)
{
    clearAccordion();

    for (const auto& [name, code] : countries)
    {
        QWidget* container = new QWidget(m_narrowContent);
        QVBoxLayout* containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        // Header button
        const bool pinned = isPinned(code);
        QToolButton* btn = new QToolButton(container);
        btn->setObjectName(QStringLiteral("accordionHeader"));
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setFixedHeight(ACCORDION_HEADER_HEIGHT);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setProperty("countryCode", code);
        btn->setProperty("isPinned", pinned);
        btn->setText(name);
        btn->setIcon(makeAccordionHeaderIcon(code, false, pinned));
        btn->setIconSize(QSize(accordionIconWidth(pinned), COUNTRY_ICON_H));
        containerLayout->addWidget(btn);

        // Cities area (hidden initially)
        QWidget* citiesWidget = new QWidget(container);
        citiesWidget->setVisible(false);
        QVBoxLayout* citiesLayout = new QVBoxLayout(citiesWidget);
        citiesLayout->setContentsMargins(ACCORDION_CITIES_INDENT, 0, 0, 0);
        citiesLayout->setSpacing(0);
        containerLayout->addWidget(citiesWidget);

        AccordionItem acc;
        acc.headerBtn    = btn;
        acc.citiesWidget = citiesWidget;
        acc.citiesLayout = citiesLayout;
        acc.expanded     = false;
        m_accordion.insert(code, acc);

        // Copies for the lambdas: the loop's names are references into the list.
        const QString countryCode = code;
        const QString countryName = name;
        connect(btn, &QToolButton::clicked, this, [this, countryCode]()
        {
            toggleAccordion(countryCode);
        });
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(btn, &QWidget::customContextMenuRequested, this, [this, btn, countryCode, countryName](const QPoint& pos)
        {
            ServerContextMenu::exec(m_manager, countryCode, countryName, QString(), btn->mapToGlobal(pos), window());
        });

        m_narrowLayout->addWidget(container);
    }
}

// ============================================================
// onCitiesReady
// ============================================================
void CountriesPage::onCitiesReady(const QString& code, const CityList& cities)
{
    m_cityCache.insert(code, cities);
    m_pendingCityCodes.remove(code);
    // Kept for when the countries are in: until then nothing shows cities.
    if (m_countriesState != ListState::Ready) return;

    const bool selected = code.compare(m_selectedCode, Qt::CaseInsensitive) == 0;
    if (anyFilterActive() == true && selected == false)
    {
        // Now that its cities are known, the country may not pass the
        // filters any longer. Not for the selected country, which keeps its
        // place in the list and says so itself when none of its cities pass.
        applyFilter();
        return;
    }

    if (m_narrowMode == true)
    {
        fillAccordionCities(code);
        return;
    }
    if (selected == true && m_selectionHidden == false)
    {
        fillWideCities(cities);
    }
}

// The country is no longer waited for, so asking for it again fetches it
// again; whoever was showing its loading message says what happened instead.
void CountriesPage::onCitiesFetchFailed(const QString& code)
{
    m_pendingCityCodes.remove(code);
    if (m_countriesState != ListState::Ready || m_cityCache.contains(code) == true) return;

    const QString text = tr("The cities could not be loaded. Select the country again to retry.");
    if (m_narrowMode == true)
    {
        showAccordionMessage(code, text);
        return;
    }
    if (code.compare(m_selectedCode, Qt::CaseInsensitive) == 0 && m_selectionHidden == false)
    {
        clearWideCities();
        addWideInfoRow(m_citiesList, text);
    }
}

// ============================================================
// Wide cities pane
// ============================================================
void CountriesPage::clearWideCities() const
{
    m_spinnerTimer->stop();
    m_citiesList->clear();
}

void CountriesPage::showWideCities()
{
    if (m_cityCache.contains(m_selectedCode) == true)
    {
        fillWideCities(m_cityCache.value(m_selectedCode));
    }
    else
    {
        clearWideCities();
        m_citiesLabel->setText(tr("Cities – %1").arg(m_selectedCountry));
        m_spinnerFrame = 0;
        m_citiesList->addItem(makeLoadingItem(loadingCitiesText()));
        m_spinnerTimer->start();
        m_manager->fetchCities(m_selectedCode);
    }
    updateConnectButtons();
}

// The selected country's cities as the feature filters leave them, with the
// selected city, if it is among them, selected again.
void CountriesPage::fillWideCities(const CityList& cities)
{
    clearWideCities();

    const CityList passing = filteredCities(cities);
    m_citiesLabel->setText(tr("Cities (%1) – %2").arg(passing.size()).arg(m_selectedCountry));

    if (anyFilterActive() == true && passing.isEmpty() == true)
    {
        addWideInfoRow(m_citiesList,
                       tr("No servers in this country match the selected filters."));
        m_selectedCity.clear();
        updateConnectButtons();
        return;
    }

    // "Fastest server" row (always available when not in a zero-match filtered state)
    QListWidgetItem* anyItem = new QListWidgetItem(tr("⚡  Fastest server"));
    anyItem->setData(Qt::UserRole, QString());
    anyItem->setToolTip(tr("Connects to the fastest server in this country."));
    anyItem->setForeground(FASTEST_TEXT_COLOR);
    QFont f = m_citiesList->font();
    f.setBold(true);
    f.setItalic(true);
    anyItem->setFont(f);
    anyItem->setBackground(FASTEST_ROW_TINT);
    anyItem->setSizeHint(QSize(0, LIST_ROW_HEIGHT));
    m_citiesList->addItem(anyItem);

    for (const auto& [city, features] : passing)
    {
        addWideCityItem(city, features);
    }

    // The city being chosen from (see startChoosing()).
    if (m_initialCity.isEmpty() == false && m_selectedCode.compare(m_initialCode, Qt::CaseInsensitive) == 0)
    {
        m_selectedCity = m_initialCity;
        m_initialCode.clear();
        m_initialCity.clear();
    }

    // The selected city keeps its row selected through a rebuild. A city the
    // filters took out is not selected any longer.
    QListWidgetItem* cityItem = nullptr;
    for (int i = 0; i < m_citiesList->count() && m_selectedCity.isEmpty() == false; ++i)
    {
        QListWidgetItem* item = m_citiesList->item(i);
        if (item->data(Qt::UserRole).toString().compare(m_selectedCity, Qt::CaseInsensitive) == 0)
        {
            cityItem = item;
            break;
        }
    }
    if (cityItem != nullptr)
    {
        m_selectedCity = cityItem->data(Qt::UserRole).toString();
        m_citiesList->setCurrentItem(cityItem);
        m_citiesList->scrollToItem(cityItem);
    }
    else
    {
        m_selectedCity.clear();
    }
    updateConnectButtons();
}

void CountriesPage::addWideCityItem(const QString& city, const QString& features) const
{
    QWidget* row = new QWidget();
    row->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout* hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(CITY_ROW_SPACING);

    // Display name only - the raw city is what gets passed to the CLI, and it
    // is carried on the item's UserRole data.
    QLabel* cityLabel = new QLabel(GeoUtils::cityWithRegion(m_selectedCode, city), row);
    cityLabel->setObjectName(QStringLiteral("cityLabel"));
    hbox->addWidget(cityLabel, 0, Qt::AlignVCenter);
    hbox->addStretch();

    addFeatureIcons(hbox, row, features, WIDE_FEATURE_ICON_SIZE, WIDE_FEATURE_BOX_SIZE);

    // Star button (favorites)
    if (AppConfig::instance().favoritesEnabled() == true)
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
// Accordion sections
// ============================================================
// The one place a section gets its city rows, for cities that just arrived
// and for cached ones alike, so the feature filters apply to both.
void CountriesPage::fillAccordionCities(const QString& code)
{
    if (m_accordion.contains(code) == false) return;

    const CityList passing = filteredCities(m_cityCache.value(code));
    if (anyFilterActive() == true && passing.isEmpty() == true)
    {
        showAccordionMessage(code, tr("No servers in this country match the selected filters."));
        return;
    }

    const AccordionItem acc = m_accordion.value(code);
    clearLayout(acc.citiesLayout);
    // "Fastest server" row first.
    addNarrowCityItem(acc.citiesLayout, QString(), QString(), code);
    for (const auto& [city, features] : passing)
    {
        addNarrowCityItem(acc.citiesLayout, city, features, code);
    }
}

void CountriesPage::showAccordionMessage(const QString& code, const QString& text)
{
    if (m_accordion.contains(code) == false) return;

    const AccordionItem acc = m_accordion.value(code);
    clearLayout(acc.citiesLayout);
    QLabel* label = new QLabel(text, acc.citiesWidget);
    label->setObjectName(QStringLiteral("infoLabel"));
    label->setWordWrap(true);
    acc.citiesLayout->addWidget(label);
}

void CountriesPage::addNarrowCityItem(QVBoxLayout* layout, const QString& city,
                                       const QString& features, const QString& code)
{
    QWidget* row = new QWidget();
    row->setObjectName(QStringLiteral("cityRow"));
    row->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(NARROW_ROW_H_MARGIN, NARROW_ROW_V_MARGIN, NARROW_ROW_H_MARGIN, NARROW_ROW_V_MARGIN);
    hbox->setSpacing(CITY_ROW_SPACING);

    const bool isFastest = city.isEmpty();
    QLabel* cityLabel = new QLabel(isFastest == true ? tr("⚡  Fastest server")
                                                     : GeoUtils::cityWithRegion(code, city), row);
    cityLabel->setObjectName(QStringLiteral("cityLabel"));
    if (isFastest == true)
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

    addFeatureIcons(hbox, row, features, NARROW_FEATURE_ICON_SIZE, NARROW_FEATURE_BOX_SIZE);

    // Star button (favorites) - narrow mode
    if (AppConfig::instance().favoritesEnabled() == true)
    {
        const QString countryName = GeoUtils::countryCodeToName(code);
        hbox->addWidget(makeStarButton(code, countryName, city, row),
                        0, Qt::AlignVCenter);
    }

    // Connect button
    QPushButton* connectBtn = new QPushButton(m_mode == Mode::Choose ? tr("Choose") : tr("Connect"), row);
    connectBtn->setObjectName(QStringLiteral("secondaryButton"));
    connectBtn->setFixedHeight(NARROW_ROW_BUTTON_HEIGHT);
    updateRowButtonLock(connectBtn);
    connect(connectBtn, &QPushButton::clicked, this, [this, code, city]()
    {
        requestLocation(code, city);
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
    // The loading row and the row that says the list failed to load are
    // clicked like any other, and are not countries.
    const QString code = item->data(Qt::UserRole).toString();
    if (code.isEmpty() == true) return;

    m_selectedCountry = item->text();
    m_selectedCode    = code;
    m_selectedCity.clear();
    m_selectionHidden = false;
    showWideCities();
}

void CountriesPage::onWideCitySelected(const QListWidgetItem* item)
{
    // Not the loading and empty-state messages.
    if (item->flags() == Qt::NoItemFlags) return;

    m_selectedCity = item->data(Qt::UserRole).toString();
    updateConnectButtons();
}

// ============================================================
// Accordion helpers
// ============================================================
void CountriesPage::ensureCities(const QString& code)
{
    if (m_pendingCityCodes.contains(code) == false)
    {
        m_pendingCityCodes.insert(code);
        m_manager->fetchCities(code);
    }
    // Also for cities already on their way: a rebuilt section starts empty.
    showAccordionMessage(code, tr("%1 Loading cities…").arg(QString::fromUtf8(kSpinnerFrames[0])));
}

void CountriesPage::toggleAccordion(const QString& code)
{
    if (m_accordion.contains(code) == false) return;
    AccordionItem& acc = m_accordion[code];
    acc.expanded = (acc.expanded == false);

    acc.headerBtn->setIcon(makeAccordionHeaderIcon(code, acc.expanded, isPinned(code)));
    acc.citiesWidget->setVisible(acc.expanded);
    if (acc.expanded == false) return;

    // The country opened last is the selected one.
    m_selectedCode = code;
    m_selectedCity.clear();
    m_selectedCountry = m_allCountries.key(code, code);
    m_selectionHidden = false;
    updateConnectButtons();

    if (m_cityCache.contains(code) == true)
    {
        // No citiesReady will come for these, so the section is filled now.
        fillAccordionCities(code);
    }
    else
    {
        // onCitiesReady() fills the section, which shows that it is loading
        // until then.
        ensureCities(code);
    }
}
