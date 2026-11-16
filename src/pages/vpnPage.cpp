#include "vpnPage.h"
#include "../appConfig.h"
#include "../appVersion.h"
#include "../cli/cliErrorSummary.h"
#include "../geoUtils.h"
#include "../connectionHistory.h"
#include "../favoritesManager.h"
#include "../uiHelpers.h"
#include "../motionPreference.h"
#include "../themeManager.h"
#include "../globe/countryCenters.h"
#include "../dialogs/errorDetailsDialog.h"
#include "../widgets/serverContextMenu.h"
#include "../widgets/svgBanner.h"
#include "../widgets/starButton.h"
#include "../widgets/styleUtils.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QTextDocument>
#include <QPropertyAnimation>
#include <QGuiApplication>
#include <QClipboard>
#include <QScrollArea>
#include <QVersionNumber>
#include <algorithm>
#include <cmath>
#include <initializer_list>

// ============================================================
// PowerButton implementation
// ============================================================

namespace
{
// PowerButton geometry and animation
constexpr int   BTN_SIZE               = 160;
constexpr int   RING_WIDTH             = 7;
constexpr int   ICON_SIZE              = 80;
constexpr qreal RING_OUTER_PAD         = 4.0;
constexpr qreal RING_MARGIN            = RING_WIDTH / 2.0 + RING_OUTER_PAD;
constexpr qreal HOVER_GLOW_INSET       = RING_MARGIN + RING_WIDTH / 2.0;
// QPainter::drawArc() takes its angles in sixteenths of a degree, counted
// counterclockwise from three o'clock. The spinner's arc starts at the top.
constexpr int   ARC_UNITS_PER_DEGREE   = 16;
constexpr qreal ARC_TOP_DEGREES        = 90.0;
constexpr int   SPIN_ARC_SPAN_DEGREES  = -270;
constexpr int   SPIN_ARC_SPAN_16TH     = SPIN_ARC_SPAN_DEGREES * ARC_UNITS_PER_DEGREE;
constexpr qreal FULL_TURN_DEGREES      = 360.0;
constexpr int   SPIN_ANIM_DURATION_MS  = 900;
// QAbstractAnimation's loop count for "until stopped".
constexpr int   LOOP_FOREVER           = -1;
constexpr int   FOCUS_RING_WIDTH       = 2;
constexpr qreal FOCUS_RING_INSET       = 1.5;
constexpr QColor SPIN_ARC_COLOR(0xa0, 0xa0, 0xa0);
constexpr QColor RING_CONNECTED_COLOR(0x1a, 0x9c, 0x5b);
constexpr QColor RING_DISCONNECTED_COLOR(0xd6, 0x3f, 0x3f);
constexpr QColor RING_UNKNOWN_COLOR(0x55, 0x55, 0x77);
constexpr QColor HOVER_GLOW(0xff, 0xff, 0xff, 18);

// Picker header
// Margins/spacing and the leading-icon box live in PickerBase, which owns the
// shared header construction (see PickerBase::buildHeader).
constexpr int   PICKER_WIDTH              = 260;
constexpr int   LOADING_TIMER_INTERVAL_MS = 120;
constexpr qreal LOGO_SCALE                = 4.0;
constexpr int   SMALL_ICON_PIX            = 14;
constexpr QColor STAR_FILL_COLOR(0xFF, 0xD2, 0x4A);
// The drawer notch background is light on the light theme, so the icon needs
// a dark tint there instead of the light tint used against the dark theme's
// dark notch background; otherwise it's too low-contrast to see.
constexpr QColor NOTCH_ICON_COLOR_DARK_BG(200, 200, 220);
constexpr QColor NOTCH_ICON_COLOR_LIGHT_BG(90, 90, 122);

// Picker list rows
constexpr int   ROW_HEIGHT         = 34;
constexpr int   ROW_H_MARGIN       = 10;
constexpr int   ROW_V_MARGIN       = 6;
constexpr int   ROW_SPACING        = 8;
// Flags are 4:3; rendered at any other shape they come out stretched.
constexpr int   SMALL_FLAG_PIX_W   = 20;
constexpr int   SMALL_FLAG_PIX_H   = 15;
constexpr int   SMALL_FLAG_W       = 24;
constexpr int   SMALL_FLAG_H       = 18;
constexpr int   FEATURE_ICON_PIX   = 16;
constexpr int   FEATURE_ICON_SIZE  = 22;
// What a recent or favorite connection's list item keeps: its country code,
// and its city (empty for the country's fastest server).
constexpr int   COUNTRY_CODE_ROLE  = Qt::UserRole;
constexpr int   CITY_ROLE          = Qt::UserRole + 1;

// VpnPage layout
// Content scaling (see VpnPage::updateContentScale()): the page size at the
// default window size, where the scale is 1, and the most it scales up to.
constexpr int   REFERENCE_PAGE_WIDTH       = 595;
constexpr int   REFERENCE_PAGE_HEIGHT      = 600;
constexpr qreal MAX_CONTENT_SCALE          = 2.0;
// The scale is rounded to this step, so a window drag only restyles the page
// when the scale has visibly changed rather than on every pixel.
constexpr qreal CONTENT_SCALE_STEP         = 0.05;
constexpr int   LOGO_MAX_WIDTH             = 500;
// Minimum space left between the content and the dropdowns below it before
// wide mode stops centering the content on the main column's full width.
constexpr int   CENTERED_CONTENT_GAP       = 16;
// Label property holding the font style.qss gives the label (see scaleLabelFont()).
constexpr const char* BASE_FONT_PROPERTY   = "baseFont";
// Decimals a scaled point size is written with (see scaleLabelFont()).
constexpr int   POINT_SIZE_DECIMALS        = 1;
// Extra width given to the info label beyond its text, so rounding never
// pushes the last word onto a line of its own.
constexpr int   INFO_LABEL_SLACK           = 4;
constexpr int   PAGE_H_MARGIN              = 40;
// A layout item given this stretch takes the room its neighbors leave.
constexpr int   FILL_STRETCH               = 1;
// The content scale at the default window size, which is also the least.
constexpr qreal BASE_CONTENT_SCALE         = 1.0;
// The wide-mode grid: the dropdown sidebar's column and the content's, in two
// rows. Stacked, the content spans both columns of the top row.
constexpr int   GRID_SIDEBAR_COLUMN        = 0;
constexpr int   GRID_CONTENT_COLUMN        = 1;
constexpr int   GRID_TOP_ROW               = 0;
constexpr int   GRID_BOTTOM_ROW            = 1;
constexpr int   GRID_ONE_ROW               = 1;
constexpr int   GRID_BOTH_COLUMNS          = 2;
// Where the warnings go in the wide-mode sidebar: after the stretch at its
// top, above the dropdowns.
constexpr int   SIDEBAR_BANNER_INDEX       = 1;
constexpr int   LOGO_TOP_MARGIN            = 40;
constexpr int   TOP_SECTION_SPACING        = 24;
constexpr int   TOP_SECTION_TOP_MARGIN     = 24;
constexpr int   TOP_SECTION_BTM_MARGIN     = 8;
constexpr int   SCROLL_SECTION_SPACING     = 16;
constexpr int   SCROLL_SECTION_TOP_MARGIN  = 8;
constexpr int   SCROLL_SECTION_BTM_MARGIN  = 16;
constexpr int   PORT_ROW_V_MARGIN          = 4;
constexpr int   PORT_ROW_SPACING           = 8;
constexpr int   PORT_COPY_BTN_W            = 34;
constexpr int   PORT_COPY_BTN_MIN_H        = 30;
// The forwarded port is set this many points larger than the text around it.
constexpr int   PORT_LABEL_POINT_BUMP      = 1;
constexpr int   ERROR_DETAILS_BTN_W        = 140;
constexpr int   SIDEBAR_L_MARGIN           = 20;
constexpr int   SIDEBAR_B_MARGIN           = 24;
constexpr int   SIDEBAR_SPACING            = 8;
constexpr int   BANNER_SCROLL_MAX_HEIGHT_WIDE = 260;
constexpr int   DRAWER_NOTCH_W             = 28;
constexpr int   DRAWER_NOTCH_H             = 64;
constexpr int   DRAWER_NOTCH_BTN_H         = 60;
constexpr int   DRAWER_NOTCH_H_MARGIN      = 4;

// Timers and elapsed time
constexpr int   ELAPSED_TIMER_INTERVAL_MS    = 1000;
constexpr int   CHECKING_SPINNER_INTERVAL_MS = 200;
constexpr int   SECONDS_PER_HOUR             = 3600;
constexpr int   SECONDS_PER_MINUTE           = 60;
constexpr int   MS_PER_SECOND                = 1000;

// Change-location dialog
constexpr int   CHANGE_LOCATION_DLG_MIN_W = 360;
constexpr int   DIALOG_SPACING            = 16;
constexpr int   DIALOG_H_MARGIN           = 24;
constexpr int   DIALOG_BTM_MARGIN         = 20;
constexpr int   DIALOG_BTN_SPACING        = 8;

// Display text for a saved connection: "United States, Secaucus, New Jersey"
// for a specific city, or "* Fastest in France" for a country-level entry.
// US cities carry their state - see GeoUtils::cityWithRegion().
QString connectionLabel(const QString& countryCode, const QString& countryName,
                        const QString& city)
{
    return city.isEmpty()
        ? QCoreApplication::translate("PickerRow", "\u26a1  Fastest in %1").arg(countryName)
        : QStringLiteral("%1, %2").arg(countryName,
                                       GeoUtils::cityWithRegion(countryCode, city));
}

// One "flag + label [+ date] [+ star]" popup row.
//
// The recent and favorites pickers built this identically; they differ only in
// whether a date column is present and whether the star is conditional.
// Pass an empty dateText to omit the date column.
QWidget* makeConnectionRow(const QString& countryCode, const QString& countryName,
                           const QString& city, const QString& dateText, const bool withStar)
{
    QWidget* row = new QWidget();
    row->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(ROW_H_MARGIN, ROW_V_MARGIN, ROW_H_MARGIN, ROW_V_MARGIN);
    hbox->setSpacing(ROW_SPACING);

    QLabel* flagLbl = new QLabel(row);
    const QPixmap pm = GeoUtils::svgPixmap(
        QStringLiteral(":/flags/") + countryCode.toLower(), SMALL_FLAG_PIX_W, SMALL_FLAG_PIX_H);
    if (pm.isNull() == false)
    {
        flagLbl->setPixmap(pm);
        flagLbl->setFixedSize(SMALL_FLAG_W, SMALL_FLAG_H);
    }
    hbox->addWidget(flagLbl, 0, Qt::AlignVCenter);

    ElideLabel* lbl = new ElideLabel(connectionLabel(countryCode, countryName, city), row);
    lbl->setObjectName(QStringLiteral("locationPickerItemLabel"));
    hbox->addWidget(lbl, FILL_STRETCH, Qt::AlignVCenter);

    if (dateText.isEmpty() == false)
    {
        QLabel* dateLbl = new QLabel(dateText, row);
        dateLbl->setObjectName(QStringLiteral("locationPickerTop"));
        hbox->addWidget(dateLbl, 0, Qt::AlignVCenter);
    }

    if (withStar == true)
    {
        hbox->addWidget(makeStarButton(countryCode, countryName, city, row),
                        0, Qt::AlignVCenter);
    }

    return row;
}

// Records the font style.qss gives `label`, as the base scaleLabelFont() scales.
// Called once at construction: the info label later swaps its object name to
// errorLabel, whose rule sets no font size, so reading it later could pick up
// the wrong base.
void rememberBaseFont(QLabel* label)
{
    label->ensurePolished();
    label->setProperty(BASE_FONT_PROPERTY, label->font());
}

// Scales a label's font to `scale` times its base size. A local font-size rule
// is the only thing that overrides style.qss, so that is what is set; at scale
// 1 it is cleared and the label is exactly as style.qss draws it.
void scaleLabelFont(QLabel* label, const qreal scale)
{
    if (qFuzzyCompare(scale, BASE_CONTENT_SCALE))
    {
        label->setStyleSheet(QString());
        return;
    }
    const QFont base = label->property(BASE_FONT_PROPERTY).value<QFont>();
    const QString size = base.pixelSize() > 0
        ? QString::number(qRound(base.pixelSize() * scale)) + QStringLiteral("px")
        : QString::number(base.pointSizeF() * scale, 'f', POINT_SIZE_DECIMALS) + QStringLiteral("pt");
    label->setStyleSheet(QStringLiteral("font-size: ") + size + QLatin1Char(';'));
}
} // namespace

PowerButton::PowerButton(QWidget* parent) : QWidget(parent)
{
    setFixedSize(BTN_SIZE, BTN_SIZE);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TranslucentBackground);
    // The primary control of the app has to be reachable without a mouse.
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Connect or disconnect"));

    // Clip the widget to a circle so the background/hover area isn't a square
    const QRegion mask(0, 0, BTN_SIZE, BTN_SIZE, QRegion::Ellipse);
    setMask(mask);

    m_anim = new QPropertyAnimation(this, "spinAngle", this);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(FULL_TURN_DEGREES);
    m_anim->setDuration(SPIN_ANIM_DURATION_MS);
    m_anim->setLoopCount(LOOP_FOREVER);
}

void PowerButton::setScale(const qreal scale)
{
    if (qFuzzyCompare(m_scale, scale)) return;
    m_scale = scale;
    const int size = qRound(BTN_SIZE * scale);
    setFixedSize(size, size);
    setMask(QRegion(0, 0, size, size, QRegion::Ellipse));
    update();
}

void PowerButton::setState(const RingState s)
{
    if (m_state == s) return;
    m_state = s;
    if (s == RingState::Spinning)
    {
        startSpin();
    }
    else
    {
        stopSpin();
    }
    update();
}

void PowerButton::startSpin() const
{
    if (m_anim->state() != QAbstractAnimation::Running)
    {
        m_anim->start();
    }
}

void PowerButton::stopSpin()
{
    m_anim->stop();
    m_spinAngle = 0.0;
}

void PowerButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // The geometry below is in BTN_SIZE design units; scaling the painter keeps
    // the ring, glow and focus ring (pen widths included) in proportion.
    p.scale(m_scale, m_scale);
    const QRectF widgetRect(0, 0, BTN_SIZE, BTN_SIZE);
    const QRectF ringRect = widgetRect.adjusted(RING_MARGIN, RING_MARGIN, -RING_MARGIN, -RING_MARGIN);

    //  ring / arc
    QPen ringPen;
    ringPen.setWidth(RING_WIDTH);
    ringPen.setCapStyle(Qt::RoundCap);

    if (m_state == RingState::Spinning)
    {
        ringPen.setColor(SPIN_ARC_COLOR);
        p.setPen(ringPen);
        const int startAngle = static_cast<int>((ARC_TOP_DEGREES - m_spinAngle) * ARC_UNITS_PER_DEGREE);
        p.drawArc(ringRect, startAngle, SPIN_ARC_SPAN_16TH);
    }
    else
    {
        QColor ringColor;
        if (m_state == RingState::Connected)
        {
            ringColor = RING_CONNECTED_COLOR;
        }
        else if (m_state == RingState::Disconnected)
        {
            ringColor = RING_DISCONNECTED_COLOR;
        }
        else
        {
            ringColor = RING_UNKNOWN_COLOR;
        }

        ringPen.setColor(ringColor);
        p.setPen(ringPen);
        p.drawEllipse(ringRect);
    }

    //  hover glow
    if (m_hovered == true)
    {
        p.setBrush(HOVER_GLOW);
        p.setPen(Qt::NoPen);
        p.drawEllipse(widgetRect.adjusted(HOVER_GLOW_INSET, HOVER_GLOW_INSET,
                                          -HOVER_GLOW_INSET, -HOVER_GLOW_INSET));
    }

    //  keyboard focus indicator - a thin ring outside the state ring, rather
    //  than the hover glow, which as a filled disc reads as a stuck hover.
    if (hasFocus() == true)
    {
        QPen focusPen(palette().color(QPalette::Highlight));
        focusPen.setWidth(FOCUS_RING_WIDTH);
        p.setPen(focusPen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(widgetRect.adjusted(FOCUS_RING_INSET, FOCUS_RING_INSET,
                                          -FOCUS_RING_INSET, -FOCUS_RING_INSET));
    }

    //  power SVG
    // Rendered at the scaled size and drawn without the painter's scale: a
    // pixmap stretched by the painter would come out blurry.
    p.resetTransform();
    const int iconSize = qRound(ICON_SIZE * m_scale);
    const bool darkMode = ThemeManager::isDark();
    const QPixmap& icon = m_iconCache.get({iconSize, devicePixelRatioF(), darkMode}, [iconSize, darkMode]()
    {
        return darkMode
            ? GeoUtils::svgPixmap(QStringLiteral(":/assets/power.svg"), iconSize, QColor(Qt::white))
            : GeoUtils::svgPixmap(QStringLiteral(":/assets/power.svg"), iconSize);
    });
    // Point overload, not the rect one: the pixmap is already exactly
    // iconSize in device-independent pixels, so this blits it without any
    // rescaling on HiDPI screens.
    p.drawPixmap(QPointF((width() - iconSize) / 2.0, (height() - iconSize) / 2.0), icon);
}

void PowerButton::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        m_pressed = true;
    }
    QWidget::mousePressEvent(e);
}

// Activation happens on release inside the widget, the way every other button
// behaves: pressing and then dragging away cancels, instead of firing a
// connect/disconnect the moment the mouse goes down.
void PowerButton::mouseReleaseEvent(QMouseEvent* e)
{
    const bool wasPressed = m_pressed;
    m_pressed = false;

    if (e->button() == Qt::LeftButton && wasPressed == true &&
        rect().contains(e->position().toPoint()))
    {
        emit clicked();
    }
    QWidget::mouseReleaseEvent(e);
}

void PowerButton::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
    {
        emit clicked();
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

void PowerButton::focusInEvent(QFocusEvent* e)
{
    update();
    QWidget::focusInEvent(e);
}

void PowerButton::focusOutEvent(QFocusEvent* e)
{
    m_pressed = false;
    update();
    QWidget::focusOutEvent(e);
}

void PowerButton::enterEvent(QEnterEvent* e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void PowerButton::leaveEvent(QEvent* e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}


// ============================================================
// LocationPicker implementation
// ============================================================

LocationPicker::LocationPicker(const QString& countryCode, const QString& countryName, QWidget* parent)
    : PickerBase(parent), m_countryCode(countryCode), m_countryName(countryName)
{
    setObjectName(QStringLiteral("locationPicker"));
    setFixedWidth(PICKER_WIDTH);

    // Flag icon for the header's leading slot.
    m_flagLabel = new QLabel(this);
    if (countryCode.isEmpty() == false)
    {
        // At the header's icon size (see buildHeader()), which is 4:3 like
        // the flag: a square pixmap would stretch it and lose its top and
        // bottom to the label.
        const QPixmap pm = GeoUtils::svgPixmap(
            QStringLiteral(":/flags/") + countryCode.toLower(), LEADING_ICON_W, LEADING_ICON_H);
        if (pm.isNull() == false)
        {
            m_flagLabel->setPixmap(pm);
        }
    }

    buildHeader(m_flagLabel, tr("Selected Location"), tr("\u26a1  Fastest server"));

    // Start in loading state immediately
    setLoading(true);
}

bool LocationPicker::eventFilter(QObject* obj, QEvent* ev)
{
    if (handleCommonEvents(obj, ev) == true) return true;
    return PickerBase::eventFilter(obj, ev);
}

void LocationPicker::onRowClicked(QListWidgetItem* item)
{
    const QString clicked = item->data(Qt::UserRole).toString();
    closePopup();

    if (clicked == QLatin1String("__change_country__"))
    {
        emit changeCountryRequested();
        return;
    }

    if (m_unknownConnection == false && clicked == m_selectedCity) return;

    m_unknownConnection = false;
    m_selectedCity = clicked;
    updateHeader();
    emit selectionChanged(m_selectedCity);
}

PickerBase::RowLocation LocationPicker::rowLocation(const QListWidgetItem* item) const
{
    // Every row is a city of the one country, or its fastest server, but
    // for the "Change country" action.
    const QString city = item->data(Qt::UserRole).toString();
    if (city == QLatin1String("__change_country__"))
    {
        return {};
    }
    return {.countryCode = m_countryCode, .countryName = m_countryName, .city = city};
}

void LocationPicker::updateHeader() const
{
    if (m_selectedCity.isEmpty() == false)
    {
        // m_selectedCity stays the raw CLI city name; only the display is
        // decorated with the state.
        m_bottomLine->setText(GeoUtils::cityWithRegion(m_countryCode, m_selectedCity));
    }
    else if (m_unknownConnection == true)
    {
        m_bottomLine->setText(tr("Active connection"));
    }
    else if (m_countryName.isEmpty() == false)
    {
        m_bottomLine->setText(tr("\u26a1  Fastest in %1").arg(m_countryName));
    }
    else
    {
        m_bottomLine->setText(tr("\u26a1  Fastest server"));
    }
}

void LocationPicker::setLoading(const bool loading)
{
    if (loading == true)
    {
        if (m_loadingTimer == nullptr)
        {
            m_loadingTimer = new QTimer(this);
            m_loadingTimer->setInterval(LOADING_TIMER_INTERVAL_MS);
            connect(m_loadingTimer, &QTimer::timeout, this, [this]()
            {
                m_loadingFrame = (m_loadingFrame + 1) % kSpinnerFrameCount;
                m_bottomLine->setText(
                    tr("%1 Loading locations\u2026")
                        .arg(QString::fromUtf8(kSpinnerFrames[m_loadingFrame])));
            });
        }
        m_bottomLine->setText(
            tr("%1 Loading locations\u2026")
                .arg(QString::fromUtf8(kSpinnerFrames[0])));
        m_loadingTimer->start();
        m_chevron->setVisible(false);
        setVisible(true);
    }
    else
    {
        if (m_loadingTimer != nullptr)
        {
            m_loadingTimer->stop();
        }

        m_chevron->setVisible(m_collapsed == false);
        updateHeader();
    }
}

void LocationPicker::setUnknownConnection(bool unknown)
{
    m_unknownConnection = unknown;
    updateHeader();
}

void LocationPicker::selectCity(const QString& city)
{
    setSelectedCity(city);
    emit selectionChanged(city);
}

int LocationPicker::rowForCity(const QString& city) const
{
    for (int i = 0; i < m_list->count(); ++i)
    {
        if (m_list->item(i)->data(Qt::UserRole).toString() == city)
        {
            return i;
        }
    }
    return NO_ROW;
}

void LocationPicker::setSelectedCity(const QString& city)
{
    m_unknownConnection = false;
    m_selectedCity = city;
    updateHeader();

    const int row = rowForCity(city);
    m_list->setCurrentRow(row == NO_ROW ? 0 : row);
}

bool LocationPicker::trySelectCity(const QString& city)
{
    const int row = rowForCity(city);
    // City not in the local country's list (e.g. connected to a different country).
    // Leave the picker display unchanged so it keeps showing the last local selection.
    if (row == NO_ROW) return false;

    m_unknownConnection = false;
    m_selectedCity = city;
    updateHeader();
    m_list->setCurrentRow(row);
    return true;
}

void LocationPicker::populate(const QList<QPair<QString, QString>>& cities)
{
    // Kept across the rebuild: the list is filled again whenever the country's
    // cities are fetched again (the Countries list's Refresh does it), which
    // is no reason to drop what the user chose.
    const QString previousCity = m_selectedCity;

    setLoading(false);
    m_list->clear();

    //  Fastest server entry
    QListWidgetItem* fastestItem = new QListWidgetItem();
    fastestItem->setData(Qt::UserRole, QString());

    QWidget* fastestRow = new QWidget();
    fastestRow->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* fbox = new QHBoxLayout(fastestRow);
    fbox->setContentsMargins(ROW_H_MARGIN, ROW_V_MARGIN, ROW_H_MARGIN, ROW_V_MARGIN);
    fbox->setSpacing(ROW_SPACING);

    QLabel* fIcon = new QLabel(fastestRow);
    if (m_countryCode.isEmpty() == false)
    {
        const QPixmap pm = GeoUtils::svgPixmap(QStringLiteral(":/flags/") + m_countryCode.toLower(),
                                               SMALL_FLAG_PIX_W, SMALL_FLAG_PIX_H);
        if (pm.isNull() == false)
        {
            fIcon->setPixmap(pm);
            fIcon->setFixedSize(SMALL_FLAG_W, SMALL_FLAG_H);
        }
    }
    fbox->addWidget(fIcon, 0, Qt::AlignVCenter);

    ElideLabel* fLabel = new ElideLabel(tr("\u26a1  Fastest server"), fastestRow);
    fLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
    QFont bold = fLabel->font();
    bold.setBold(true);
    bold.setItalic(true);
    fLabel->setFont(bold);
    fLabel->setProperty("fastest", true);
    fbox->addWidget(fLabel, FILL_STRETCH, Qt::AlignVCenter);

    if (AppConfig::instance().favoritesEnabled() == true && m_countryCode.isEmpty() == false)
    {
        const QString countryName = GeoUtils::countryCodeToName(m_countryCode);
        fbox->addWidget(makeStarButton(m_countryCode, countryName, QString(), fastestRow),
                        0, Qt::AlignVCenter);
    }

    fastestItem->setSizeHint(QSize(0, ROW_HEIGHT));
    m_list->addItem(fastestItem);
    m_list->setItemWidget(fastestItem, fastestRow);
    installOnRowWidget(fastestRow);

    //  "Change country" action item
    QListWidgetItem* changeItem = new QListWidgetItem();
    changeItem->setData(Qt::UserRole, QStringLiteral("__change_country__"));

    QWidget* changeRow = new QWidget();
    changeRow->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* cbox = new QHBoxLayout(changeRow);
    cbox->setContentsMargins(ROW_H_MARGIN, ROW_V_MARGIN, ROW_H_MARGIN, ROW_V_MARGIN);
    cbox->setSpacing(ROW_SPACING);

    ElideLabel* cLabel = new ElideLabel(tr("\U0001f310  Change country\u2026"), changeRow);
    cLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
    QFont italicFont = cLabel->font();
    italicFont.setItalic(true);
    cLabel->setFont(italicFont);
    cLabel->setProperty("action", true);
    cbox->addWidget(cLabel, FILL_STRETCH, Qt::AlignVCenter);

    changeItem->setSizeHint(QSize(0, ROW_HEIGHT));
    m_list->addItem(changeItem);
    m_list->setItemWidget(changeItem, changeRow);
    installOnRowWidget(changeRow);

    //  City entries
    for (const auto& [city, features] : cities)
    {
        QListWidgetItem* item = new QListWidgetItem();
        item->setData(Qt::UserRole, city);

        QWidget* row = new QWidget();
        row->setCursor(Qt::PointingHandCursor);
        QHBoxLayout* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(ROW_H_MARGIN, ROW_V_MARGIN, ROW_H_MARGIN, ROW_V_MARGIN);
        hbox->setSpacing(ROW_SPACING);

        ElideLabel* cityLabel = new ElideLabel(GeoUtils::cityWithRegion(m_countryCode, city), row);
        cityLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
        hbox->addWidget(cityLabel, FILL_STRETCH, Qt::AlignVCenter);

        const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const FeatureMeta& meta : kServerFeatures)
        {
            const bool matched = std::ranges::any_of(tags, [&meta](const QString& tag)
            {
                return tag.trimmed().contains(QLatin1String(meta.keyword), Qt::CaseInsensitive);
            });
            if (matched == false) continue;

            QLabel* iconLabel = new QLabel(row);
            iconLabel->setPixmap(GeoUtils::svgPixmap(QLatin1String(meta.resource), FEATURE_ICON_PIX));
            iconLabel->setFixedSize(FEATURE_ICON_SIZE, FEATURE_ICON_SIZE);
            iconLabel->setAlignment(Qt::AlignCenter);
            iconLabel->setToolTip(translatedFeatureTooltip(meta));
            hbox->addWidget(iconLabel, 0, Qt::AlignVCenter);
        }

        if (AppConfig::instance().favoritesEnabled() == true && m_countryCode.isEmpty() == false)
        {
            const QString countryName = GeoUtils::countryCodeToName(m_countryCode);
            hbox->addWidget(makeStarButton(m_countryCode, countryName, city, row),
                            0, Qt::AlignVCenter);
        }

        item->setSizeHint(QSize(0, ROW_HEIGHT));
        m_list->addItem(item);
        m_list->setItemWidget(item, row);
        installOnRowWidget(row);
    }

    // The header last read the selection before the list was rebuilt, so it
    // is brought back in line with what is selected now: the same city if
    // the new list still has it, the fastest server if not.
    const int previousRow = previousCity.isEmpty() == true ? NO_ROW : rowForCity(previousCity);
    if (previousRow == NO_ROW)
    {
        m_selectedCity.clear();
        m_list->setCurrentRow(0);
    }
    else
    {
        m_selectedCity = previousCity;
        m_list->setCurrentRow(previousRow);
    }
    updateHeader();
}

// ============================================================
// SavedConnectionPicker implementation
// ============================================================

SavedConnectionPicker::SavedConnectionPicker(QWidget* parent)
    : PickerBase(parent)
{
}

void SavedConnectionPicker::addConnectionRow(const QString& countryCode, const QString& countryName,
                                             const QString& city, const QString& dateText, const bool withStar)
{
    QListWidgetItem* item = new QListWidgetItem();
    item->setData(COUNTRY_CODE_ROLE, countryCode);
    item->setData(CITY_ROLE, city);

    QWidget* row = makeConnectionRow(countryCode, countryName, city, dateText, withStar);

    item->setSizeHint(QSize(0, ROW_HEIGHT));
    m_list->addItem(item);
    m_list->setItemWidget(item, row);
    installOnRowWidget(row);
}

bool SavedConnectionPicker::eventFilter(QObject* obj, QEvent* ev)
{
    if (handleCommonEvents(obj, ev) == true) return true;
    return PickerBase::eventFilter(obj, ev);
}

PickerBase::RowLocation SavedConnectionPicker::rowLocation(const QListWidgetItem* item) const
{
    const QString code = item->data(COUNTRY_CODE_ROLE).toString();
    return {.countryCode = code, .countryName = GeoUtils::countryCodeToName(code),
            .city = item->data(CITY_ROLE).toString()};
}

void SavedConnectionPicker::onRowClicked(QListWidgetItem* item)
{
    const QString code = item->data(COUNTRY_CODE_ROLE).toString();
    const QString city = item->data(CITY_ROLE).toString();
    closePopup();
    if (code.isEmpty() == false)
    {
        emit connectionSelected(code, city);
    }
}

// ============================================================
// RecentPicker implementation
// ============================================================

RecentPicker::RecentPicker(QWidget* parent)
    : SavedConnectionPicker(parent)
{
    setObjectName(QStringLiteral("locationPicker")); // reuse same stylesheet
    setFixedWidth(PICKER_WIDTH);

    // Clock icon - mirrors the flag icon in LocationPicker for visual parity
    m_clockIcon = new QLabel(this);
    updateClockIcon();
    buildHeader(m_clockIcon, tr("Recent Connections"), tr("None yet"));

    refresh();
}

void RecentPicker::updateClockIcon()
{
    const QColor tintColor = ThemeManager::isDark() == true
        ? NOTCH_ICON_COLOR_DARK_BG
        : NOTCH_ICON_COLOR_LIGHT_BG;
    m_clockIcon->setPixmap(GeoUtils::svgPixmap(QStringLiteral(":/assets/clock-history.svg"),
                                               SMALL_ICON_PIX, tintColor));
}

void RecentPicker::changeEvent(QEvent* event)
{
    SavedConnectionPicker::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && m_clockIcon != nullptr)
    {
        updateClockIcon();
    }
}

void RecentPicker::refresh()
{
    m_list->clear();
    const QList<ConnectionEntry> entries = ConnectionHistory::instance().entries();

    if (entries.isEmpty())
    {
        m_bottomLine->setText(tr("None yet"));
        m_chevron->setVisible(false);
        return;
    }

    m_chevron->setVisible(m_collapsed == false);
    // Show most recent in header
    const ConnectionEntry& first = entries.first();
    m_bottomLine->setText(connectionLabel(first.countryCode, first.countryName, first.city));

    const bool withStar = AppConfig::instance().favoritesEnabled();
    for (const ConnectionEntry& e : entries)
    {
        addConnectionRow(e.countryCode, e.countryName, e.city,
                         QLocale().toString(e.connectedAt, tr("MMM d")), withStar);
    }
}

// ============================================================
// FavoritesPicker implementation
// ============================================================

FavoritesPicker::FavoritesPicker(QWidget* parent)
    : SavedConnectionPicker(parent)
{
    setObjectName(QStringLiteral("locationPicker")); // reuse same stylesheet
    setFixedWidth(PICKER_WIDTH);

    // Star icon
    QLabel* starIcon = new QLabel(this);
    starIcon->setPixmap(GeoUtils::svgPixmap(QStringLiteral(":/assets/star-fill.svg"),
                                            SMALL_ICON_PIX, STAR_FILL_COLOR));

    buildHeader(starIcon, tr("Favorites"), tr("None yet"));

    refresh();
}

void FavoritesPicker::refresh()
{
    m_list->clear();
    const QList<FavoriteEntry> entries = FavoritesManager::instance().entries();

    if (entries.isEmpty())
    {
        m_bottomLine->setText(tr("None yet"));
        m_chevron->setVisible(false);
        // The last one was taken out with the list open.
        closePopup();
        return;
    }

    m_chevron->setVisible(m_collapsed == false);
    // Show first favorite in header
    const FavoriteEntry& first = entries.first();
    m_bottomLine->setText(connectionLabel(first.countryCode, first.countryName, first.city));

    for (const FavoriteEntry& e : entries)
    {
        // Always starred here - the star is the "unfavorite" control.
        addConnectionRow(e.countryCode, e.countryName, e.city, QString(), true);
    }

    // If the popup is currently open, resize it to match the new item count.
    if (m_popup != nullptr && m_popup->isVisible())
    {
        resizeList();
    }
}

// ============================================================
// VpnPage implementation
// ============================================================

VpnPage::VpnPage(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager),
      m_localCountryCode(GeoUtils::detectUserCountry())
{
    // Background globe: created first so every other child stacks above it.
    // Not in the layout; resizeEvent() keeps it covering the whole page, the
    // quick settings included.
    m_globe = new GlobeWidget(this);
    if (m_localCountryCode.isEmpty() == false)
    {
        const std::optional<GeoPoint> home = CountryCenters::find(m_localCountryCode);
        if (home.has_value() == true)
        {
            m_globe->setSpinLongitude(home->longitude);
        }
    }

    // The main column, with the quick settings beside it once resizeEvent()
    // finds the page wide enough.
    QHBoxLayout* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    m_mainColumn = new QWidget(this);
    rootLayout->addWidget(m_mainColumn, FILL_STRETCH);

    // Filled in by setQuickSettingSources().
    m_quickSettings = new QuickSettingsPanel(m_manager, this);
    m_quickSettings->setVisible(false);
    rootLayout->addWidget(m_quickSettings);
    connect(m_quickSettings, &QuickSettingsPanel::quickSettingsChanged, this, &VpnPage::updateQuickSettings);

    QVBoxLayout* outerLayout = new QVBoxLayout(m_mainColumn);
    m_outerLayout = outerLayout;
    outerLayout->setSpacing(0);
    // No left margin here - logo and power button span the full column width so they
    // are visually centered.  The COLLAPSED_DRAWER_WIDTH offset is applied only to the scroll
    // area via m_scrollOffsetWidget, keeping content clear of the drawer overlay.
    outerLayout->setContentsMargins(0, 0, 0, 0);

    //  Logo row - always at the top, full width
    m_logoRow = new QWidget(this);
    m_logoRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_logoRowLayout = new QHBoxLayout(m_logoRow);
    m_logoRowLayout->setContentsMargins(PAGE_H_MARGIN, LOGO_TOP_MARGIN, PAGE_H_MARGIN, 0);

    // Proton VPN logo banner
    m_logo = new SvgBanner(QStringLiteral(":/assets/proton-vpn-logo.svg"), LOGO_SCALE, m_logoRow);
    m_logo->setLightResource(QStringLiteral(":/assets/proton-vpn-logo-light.svg"));
    m_logo->setMaxWidth(LOGO_MAX_WIDTH);
    m_logoRowLayout->addWidget(m_logo, 0, Qt::AlignCenter);

    outerLayout->addWidget(m_logoRow);

    //  Fixed top section: power button + status label
    m_topContentWidget = new QWidget(this);
    m_topContentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_topLayout = new QVBoxLayout(m_topContentWidget);
    QVBoxLayout* topLayout = m_topLayout;
    topLayout->setSpacing(TOP_SECTION_SPACING);
    topLayout->setContentsMargins(PAGE_H_MARGIN, TOP_SECTION_TOP_MARGIN, PAGE_H_MARGIN, TOP_SECTION_BTM_MARGIN);

    // Power button
    m_powerBtn = new PowerButton(m_topContentWidget);
    connect(m_powerBtn, &PowerButton::clicked, this, [this]()
    {
        // Using the button is what the tip asks for, so it has done its job.
        if (m_powerButtonHint != nullptr)
        {
            m_powerButtonHint->dismiss();
        }

        if (m_currentState == VpnState::Connected)
        {
            emit disconnectRequested();
        }
        else if (m_currentState == VpnState::Disconnected || m_currentState == VpnState::Error)
        {
            if (m_isFreeUser == true)
            {
                // Free users: let protonvpn pick the best free server automatically.
                emit connectRequested(QString(), QString());
            }
            else
            {
                m_activeCity = m_locationPicker->selectedCity();
                emit connectRequested(m_localCountryCode, m_activeCity);
            }
        }
    });
    topLayout->addWidget(m_powerBtn, 0, Qt::AlignCenter);
    m_globe->setAnchor(m_powerBtn);

    // Status text
    m_statusLabel = new QLabel(tr("Checking\u2026"), m_topContentWidget);
    m_statusLabel->setObjectName(QStringLiteral("vpnStatusLabel"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    rememberBaseFont(m_statusLabel);
    topLayout->addWidget(m_statusLabel, 0, Qt::AlignCenter);

    // Location picker + Recent picker
    const QString localCountryName = m_localCountryCode.isEmpty()
        ? QString()
        : GeoUtils::countryCodeToName(m_localCountryCode);
    m_locationPicker = new LocationPicker(m_localCountryCode, localCountryName, this);
    connect(m_locationPicker, &LocationPicker::changeCountryRequested,
            this, &VpnPage::changeCountryRequested);

    // Recent and favorites pickers behave identically once a row is chosen
    // (see onSavedConnectionSelected()).
    m_recentPicker = new RecentPicker(this);
    connect(m_recentPicker, &RecentPicker::connectionSelected, this, &VpnPage::onSavedConnectionSelected);

    // Favorites picker
    m_favoritesPicker = new FavoritesPicker(this);
    connect(m_favoritesPicker, &FavoritesPicker::connectionSelected, this, &VpnPage::onSavedConnectionSelected);

    // Right-clicking a location in any of them offers to favorite it or show
    // its properties.
    for (PickerBase* picker : std::initializer_list<PickerBase*>{m_locationPicker, m_recentPicker, m_favoritesPicker})
    {
        connect(picker, &PickerBase::rowContextMenuRequested, this,
                [this, picker](const QString& code, const QString& name, const QString& city, const QPoint& globalPos)
        {
            ServerContextMenu::exec(m_manager, code, name, city, globalPos, window(), picker->popupWindow());
        });
    }

    // Auto-refresh favorites picker when favorites change
    connect(&FavoritesManager::instance(), &FavoritesManager::changed,
            this, [this]()
            {
                if (m_favoritesPicker != nullptr)
                {
                    m_favoritesPicker->refresh();
                }
                relayoutPickers();
            });

    // Note: pickers are NOT added to topLayout here.
    // They live inside m_drawer (an absolutely-positioned overlay) created below.

    // Apply persisted visibility preference for the location picker.
    setLocationPickerVisible(AppConfig::instance().showLocationPicker());


    //  Scrollable section: timer, info, hint, button
    QWidget* scrollContent = new QWidget();
    scrollContent->setObjectName(QStringLiteral("vpnScrollContent"));
    scrollContent->installEventFilter(this); // see eventFilter()
    scrollContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(SCROLL_SECTION_SPACING);
    scrollLayout->setContentsMargins(PAGE_H_MARGIN, SCROLL_SECTION_TOP_MARGIN, PAGE_H_MARGIN, SCROLL_SECTION_BTM_MARGIN);

    // Timer label
    m_timerLabel = new QLabel(scrollContent);
    m_timerLabel->setObjectName(QStringLiteral("timerLabel"));
    m_timerLabel->setAlignment(Qt::AlignCenter);
    rememberBaseFont(m_timerLabel);
    m_timerLabel->setVisible(false);
    scrollLayout->addWidget(m_timerLabel, 0, Qt::AlignCenter);

    // Info label (IP / error details)
    m_infoLabel = new QLabel(scrollContent);
    m_infoLabel->setObjectName(QStringLiteral("infoLabel"));
    m_infoLabel->setAlignment(Qt::AlignCenter);
    rememberBaseFont(m_infoLabel);
    m_infoLabel->setWordWrap(true);
    // Sized to its text and centered, so its backdrop over the globe is a pill
    // around the words rather than a band across the whole page.
    m_infoLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    scrollLayout->addWidget(m_infoLabel, 0, Qt::AlignHCenter);

    // Sign-out hint - shown only when a CLI error is detected
    m_signOutHintLabel = new QLabel(scrollContent);
    m_signOutHintLabel->setObjectName(QStringLiteral("signOutHintLabel"));
    m_signOutHintLabel->setAlignment(Qt::AlignCenter);
    m_signOutHintLabel->setWordWrap(true);
    m_signOutHintLabel->setTextFormat(Qt::RichText);
    m_signOutHintLabel->setOpenExternalLinks(false);
    m_signOutHintLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_signOutHintLabel->setVisible(false);
    connect(m_signOutHintLabel, &QLabel::linkActivated, this, [this](const QString& link)
    {
        if (link == QLatin1String("action://signout"))
        {
            emit signOutRequested();
        }
    });
    scrollLayout->addWidget(m_signOutHintLabel);

    // "View Details" button – shown only on error
    m_errorDetailsBtn = new QPushButton(tr("View Details"), scrollContent);
    m_errorDetailsBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_errorDetailsBtn->setCursor(Qt::PointingHandCursor);
    m_errorDetailsBtn->setVisible(false);
    m_errorDetailsBtn->setFixedWidth(ERROR_DETAILS_BTN_W);
    connect(m_errorDetailsBtn, &QPushButton::clicked, this, &VpnPage::showErrorDetails);
    scrollLayout->addWidget(m_errorDetailsBtn, 0, Qt::AlignCenter);

    buildPortRow(scrollContent, scrollLayout);

    scrollLayout->addStretch(FILL_STRETCH);

    buildBannerArea(scrollLayout);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("vpnScrollArea"));
    m_scrollArea->setWidget(scrollContent);
    m_scrollArea->setWidgetResizable(true);
    // The info label is sized to the viewport (see syncInfoBackdrop()), which
    // a switch between the narrow and wide layouts resizes after the fact.
    m_scrollArea->viewport()->installEventFilter(this);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea#vpnScrollArea { background: transparent; }"
        "QScrollArea#vpnScrollArea > QWidget > QWidget { background: transparent; }"));
    scrollContent->setAutoFillBackground(false);

    //  Narrow mode wrapper (default)
    m_narrowContent = new QWidget(this);
    m_narrowContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_narrowContentLayout = new QVBoxLayout(m_narrowContent);
    m_narrowContentLayout->setContentsMargins(0, 0, 0, 0);
    m_narrowContentLayout->setSpacing(0);
    m_narrowContentLayout->addWidget(m_topContentWidget);

    // Scroll offset wrapper: gives the scroll area a COLLAPSED_DRAWER_WIDTH left margin so
    // it sits to the right of the drawer overlay.  The logo row and power-button
    // section are NOT wrapped here, so they remain visually centered on the page.
    m_scrollOffsetWidget = new QWidget(m_narrowContent);
    m_scrollOffsetWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_scrollOffsetLayout = new QVBoxLayout(m_scrollOffsetWidget);
    m_scrollOffsetLayout->setContentsMargins(PickerDrawer::COLLAPSED_DRAWER_WIDTH, 0, PickerDrawer::COLLAPSED_DRAWER_WIDTH, 0);
    m_scrollOffsetLayout->setSpacing(0);
    m_scrollOffsetLayout->addWidget(m_scrollArea, FILL_STRETCH);

    m_narrowContentLayout->addWidget(m_scrollOffsetWidget, FILL_STRETCH);
    outerLayout->addWidget(m_narrowContent, FILL_STRETCH);

    //  Wide mode wrapper (two-column layout, initially hidden)
    m_wideContent = new QWidget(this);
    m_wideContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_wideContent->setVisible(false);

    // A grid rather than a row, so the content can either sit in a column beside
    // the dropdown sidebar or span the full width above it when the page is tall
    // enough (see updateArrangement()).
    m_wideGrid = new QGridLayout(m_wideContent);
    m_wideGrid->setContentsMargins(0, 0, 0, 0);
    m_wideGrid->setSpacing(0);
    m_wideGrid->setColumnStretch(GRID_CONTENT_COLUMN, FILL_STRETCH);
    m_wideGrid->setRowStretch(GRID_TOP_ROW, FILL_STRETCH);

    // Left column: picker sidebar (fixed width, pickers at bottom)
    m_pickerSidebar = new QWidget(m_wideContent);
    m_pickerSidebar->setFixedWidth(WIDE_SIDEBAR_WIDTH);
    m_pickerSidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_pickerSidebarLayout = new QVBoxLayout(m_pickerSidebar);
    m_pickerSidebarLayout->setContentsMargins(SIDEBAR_L_MARGIN, 0, 0, SIDEBAR_B_MARGIN);
    m_pickerSidebarLayout->setSpacing(SIDEBAR_SPACING);
    m_pickerSidebarLayout->addStretch(FILL_STRETCH); // pushes pickers to bottom

    // Right column: power button, status, scrollable content
    m_rightContent = new QWidget(m_wideContent);
    m_rightContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_rightContentLayout = new QVBoxLayout(m_rightContent);
    m_rightContentLayout->setContentsMargins(0, 0, 0, 0);
    m_rightContentLayout->setSpacing(0);
    // topContentWidget and scrollArea are added here in applyWideMode(true)

    m_wideGrid->addWidget(m_pickerSidebar, GRID_TOP_ROW, GRID_SIDEBAR_COLUMN);
    m_wideGrid->addWidget(m_rightContent, GRID_TOP_ROW, GRID_CONTENT_COLUMN);

    // Content and dropdown height changes (a new status line, an error box, a
    // warning banner, a picker appearing) arrive as LayoutRequest events.
    m_topContentWidget->installEventFilter(this);
    m_pickerSidebar->installEventFilter(this);
    outerLayout->addWidget(m_wideContent, FILL_STRETCH);

    // Elapsed timer
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(ELAPSED_TIMER_INTERVAL_MS);
    connect(m_elapsedTimer, &QTimer::timeout, this, [this]()
    {
        // Read the elapsed time from a monotonic clock rather than counting
        // ticks: timer events that the event loop delays or coalesces (a busy
        // UI, a blocked CLI call) would otherwise make the displayed duration
        // drift permanently behind the real one.
        const qint64 seconds = m_connectedSince.elapsed() / MS_PER_SECOND;
        const qint64 h = seconds / SECONDS_PER_HOUR;
        const qint64 m = (seconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
        const qint64 s = seconds % SECONDS_PER_MINUTE;
        m_timerLabel->setText(QString::asprintf("%02lld:%02lld:%02lld", h, m, s));
    });

    // Checking spinner - animates the status label while connection state is unknown
    m_checkingSpinnerTimer = new QTimer(this);
    m_checkingSpinnerTimer->setInterval(CHECKING_SPINNER_INTERVAL_MS);
    connect(m_checkingSpinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_checkingSpinnerFrame = (m_checkingSpinnerFrame + 1) % kSpinnerFrameCount;
        m_statusLabel->setText(
            tr("%1 Checking\u2026").arg(QString::fromUtf8(kSpinnerFrames[m_checkingSpinnerFrame])));
    });

    // Start in Unknown - spinner runs until the status monitor's first snapshot arrives
    updateUi(VpnState::Unknown, QString());
    m_checkingSpinnerTimer->start();

    // Populate city combo from the detected local country (if any)
    connect(m_manager, &VpnManager::citiesReady, this, &VpnPage::onCitiesReady);
    if (m_localCountryCode.isEmpty() == false)
    {
        m_manager->fetchCities(m_localCountryCode);
    }
    else
    {
        populateLocationPicker({}); // No local country detected - stop spinner immediately
    }

    // Check the installed CLI version against the tested version
    connect(m_manager, &VpnManager::cliVersionReady, this, &VpnPage::onCliVersionReady);
    m_manager->fetchCliVersion();

    // If the user enables/disables port forwarding while already connected,
    // start or stop the natpmpc loop immediately without waiting for reconnect.
    // The manager re-reads the settings after every change, so this follows
    // what the CLI actually has rather than what was asked for.
    m_portForwardingOn = m_manager->portForwardingEnabled();
    connect(m_manager, &VpnManager::settingsReady, this, [this](const QMap<QString, QString>&)
    {
        const bool on = m_manager->portForwardingEnabled();
        if (on == m_portForwardingOn) return;
        m_portForwardingOn = on;
        if (m_currentState != VpnState::Connected) return;

        if (on == true)
        {
            startNatPmpLoop();
            // Not looked up while the setting was off: the note below needs
            // to know whether this server forwards ports.
            fetchConnectedCityFeatures();
        }
        else
        {
            // Not stopNatPmpLoop(): that is disconnect cleanup and would also
            // forget this connection's info text.
            stopPortForwarding();
            dismissNatpmpcBanner();
        }
        // Adds or removes the "Port forwarding is active" line.
        refreshConnectedInfoLabel();
    });

    // The country of the connected server, as `protonvpn status` reports it.
    // For a connection found already up it comes just before the Connected
    // state; for one the app made, about a second after it. Either way the
    // globe turns to it, and the city's features can be looked up in it.
    connect(m_manager, &VpnManager::connectionCountryKnown, this, [this](const QString& cc)
    {
        m_connectedCountryCode = cc;
        updateGlobeTarget();
        fetchConnectedCityFeatures();
    });

    connect(&MotionPreference::instance(), &MotionPreference::changed, this, &VpnPage::applyGlobeSettings);
    applyGlobeSettings();

    //  NatPmpManager
    m_natPmpManager = new NatPmpManager(this);

    connect(m_natPmpManager, &NatPmpManager::portAcquired, this, [this](int port)
    {
        if (m_portLabel != nullptr)
        {
            m_portLabel->setText(QString::number(port));
        }
        if (m_portRow != nullptr)
        {
            m_portRow->setVisible(true);
        }
    });

    connect(m_natPmpManager, &NatPmpManager::portLost, this, [this]()
    {
        if (m_portRow != nullptr)
        {
            m_portRow->setVisible(false);
        }
    });

    connect(m_natPmpManager, &NatPmpManager::natpmpcMissing, this, [this]()
    {
        showNatpmpcBanner();
    });
    // Show a banner if this is a pre-release build
    checkPrereleaseBanner();

    // React to plan type (Free vs Plus) - affects picker visibility and connect behavior.
    connect(m_manager, &VpnManager::accountTypeReady, this, [this](AccountType type)
    {
        m_isFreeUser = (type == AccountType::Free);
        relayoutPickers();
    });
    // Apply immediately if already known (e.g. app restart with cached state).
    if (m_manager->accountType() != AccountType::Unknown)
    {
        m_isFreeUser = (m_manager->accountType() == AccountType::Free);
        relayoutPickers();
    }

    // When the user changes location while connected/connecting, ask what to do
    connect(m_locationPicker, &LocationPicker::selectionChanged,
            this, &VpnPage::onLocationSelectionChanged);

    buildDrawer();

    // Sync drawer with current config
    m_showFavoritesDropdown = AppConfig::instance().showFavoritesDropdown();
    m_favoritesEnabled      = AppConfig::instance().favoritesEnabled();
    relayoutPickers();
}

// Builds the "Forwarded Port: <n> [copy]" row shown while port forwarding is
// active. Hidden until natpmpc reports a port.
void VpnPage::buildPortRow(QWidget* parent, QVBoxLayout* layout)
{
    //  Port forwarding row
    // Hidden by default; appears when natpmpc successfully allocates a port.
    m_portRow = new QWidget(parent);
    QHBoxLayout* portRowLayout = new QHBoxLayout(m_portRow);
    portRowLayout->setContentsMargins(0, PORT_ROW_V_MARGIN, 0, PORT_ROW_V_MARGIN);
    portRowLayout->setSpacing(PORT_ROW_SPACING);

    QLabel* portTitleLabel = new QLabel(tr("Forwarded Port:"), m_portRow);
    portTitleLabel->setObjectName(QStringLiteral("infoLabel"));
    portRowLayout->addWidget(portTitleLabel, 0, Qt::AlignVCenter);

    //  Button-group container
    // Left segment : port number label
    // Right segment: clipboard icon button
    // Styled to look like a Bootstrap input-group / btn-group.
    QWidget* btnGroup = new QWidget(m_portRow);
    btnGroup->setObjectName(QStringLiteral("portBtnGroup"));
    QHBoxLayout* btnGroupLayout = new QHBoxLayout(btnGroup);
    btnGroupLayout->setContentsMargins(0, 0, 0, 0);
    btnGroupLayout->setSpacing(0);

    // Left segment - port number
    m_portLabel = new QLabel(QStringLiteral("-"), btnGroup);
    m_portLabel->setObjectName(QStringLiteral("portValueLabel"));
    m_portLabel->setAlignment(Qt::AlignCenter);
    {
        QFont f = m_portLabel->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + PORT_LABEL_POINT_BUMP);
        m_portLabel->setFont(f);
    }
    btnGroupLayout->addWidget(m_portLabel);

    // Right segment - clipboard icon button
    // Build a white-tinted icon from the SVG asset.
    const QPixmap clipPix = GeoUtils::svgPixmap(QStringLiteral(":/assets/clipboard2-plus.svg"),
                                                FEATURE_ICON_PIX, QColor(Qt::white));

    QPushButton* portCopyBtn = new QPushButton(btnGroup);
    portCopyBtn->setObjectName(QStringLiteral("portCopyBtn"));
    portCopyBtn->setIcon(QIcon(clipPix));
    portCopyBtn->setIconSize({FEATURE_ICON_PIX, FEATURE_ICON_PIX});
    portCopyBtn->setFixedSize(PORT_COPY_BTN_W, m_portLabel->sizeHint().height() > 0
                                               ? m_portLabel->sizeHint().height()
                                               : PORT_COPY_BTN_MIN_H);
    portCopyBtn->setCursor(Qt::PointingHandCursor);
    portCopyBtn->setToolTip(tr("Copy to Clipboard"));
    connect(portCopyBtn, &QPushButton::clicked, this, [this]()
    {
        if (m_natPmpManager != nullptr && m_natPmpManager->forwardedPort() > 0)
        {
            QGuiApplication::clipboard()->setText(
                QString::number(m_natPmpManager->forwardedPort()));
        }
    });
    btnGroupLayout->addWidget(portCopyBtn);

    portRowLayout->addWidget(btnGroup, 0, Qt::AlignVCenter);

    m_portRow->setVisible(false);
    layout->addWidget(m_portRow, 0, Qt::AlignCenter);
}

// Builds the warnings area (the pre-release banner) plus its
// header and "Clear All" button. Lives at the bottom of the scroll content in
// narrow mode; applyWideMode() moves it into the picker sidebar.
void VpnPage::buildBannerArea(QVBoxLayout* scrollLayout)
{
    // Banner area: starts at the bottom of the scroll content (narrow mode).
    // applyWideMode() moves it above the picker dropdowns in the sidebar.
    m_vpnBannerArea = new QWidget();
    QVBoxLayout* bannerAreaLayout = new QVBoxLayout(m_vpnBannerArea);
    bannerAreaLayout->setContentsMargins(0, SCROLL_SECTION_SPACING, 0, 0);
    bannerAreaLayout->setSpacing(SIDEBAR_SPACING);

    // Header: "Warnings" label + "Clear All" button
    QWidget* bannerHeader = new QWidget(m_vpnBannerArea);
    QHBoxLayout* bannerHeaderLayout = new QHBoxLayout(bannerHeader);
    bannerHeaderLayout->setContentsMargins(0, 0, 0, 0);
    bannerHeaderLayout->setSpacing(SIDEBAR_SPACING);
    m_warningsHeaderLabel = new QLabel(tr("Warnings"), bannerHeader);
    m_warningsHeaderLabel->setObjectName(QStringLiteral("appSectionHeader"));
    m_clearAllBannersBtn = new QPushButton(tr("Clear All"), bannerHeader);
    m_clearAllBannersBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_clearAllBannersBtn->setCursor(Qt::PointingHandCursor);
    connect(m_clearAllBannersBtn, &QPushButton::clicked, this, [this]()
    {
        // A copy: each dismissal takes its banner out of the list.
        const QList<InfoBanner*> warnings = m_warningBanners;
        for (InfoBanner* warning : warnings)
        {
            warning->dismiss();
        }
    });
    bannerHeaderLayout->addWidget(m_warningsHeaderLabel);
    bannerHeaderLayout->addStretch();
    bannerHeaderLayout->addWidget(m_clearAllBannersBtn);
    bannerAreaLayout->addWidget(bannerHeader);

    // Banner content - added directly in narrow mode (outer scroll area handles
    // overflow).  applyWideMode() wraps it in m_vpnBannerScroll for wide mode.
    m_vpnBannerContent = new QWidget();
    m_vpnBannerLayout = new QVBoxLayout(m_vpnBannerContent);
    m_vpnBannerLayout->setContentsMargins(0, 0, 0, 0);
    m_vpnBannerLayout->setSpacing(0);
    bannerAreaLayout->addWidget(m_vpnBannerContent);

    m_vpnBannerArea->setVisible(false);
    // Insert before the final stretch so banners sit below the status content.
    scrollLayout->insertWidget(scrollLayout->count() - 1, m_vpnBannerArea);
}

// Builds the sliding picker drawer overlay and the notch that toggles it.
// Not part of the page layout - it is positioned manually over the content.
void VpnPage::buildDrawer()
{
    //  Sliding picker drawer (overlay - not part of outerLayout)
    // The drawer sits on the left edge of the VpnPage and overlays the main content.
    m_drawer = new PickerDrawer(m_locationPicker, m_recentPicker, m_favoritesPicker, this);
    m_drawer->setGeometry(0, 0, PickerDrawer::COLLAPSED_DRAWER_WIDTH, m_drawer->sizeHint().height());
    m_drawer->raise();

    // Notch toggle button - protrudes from the drawer's right edge
    m_drawerNotch = new QFrame(this);
    m_drawerNotch->setObjectName(QStringLiteral("drawerNotch"));
    m_drawerNotch->setFixedSize(DRAWER_NOTCH_W, DRAWER_NOTCH_H);
    m_drawerNotch->setCursor(Qt::PointingHandCursor);
    m_drawerNotch->setAttribute(Qt::WA_StyledBackground);

    QVBoxLayout* notchLayout = new QVBoxLayout(m_drawerNotch);
    notchLayout->setContentsMargins(DRAWER_NOTCH_H_MARGIN, 0, DRAWER_NOTCH_H_MARGIN, 0);
    m_drawerNotchIcon = new QLabel(m_drawerNotch);
    m_drawerNotchIcon->setAlignment(Qt::AlignCenter);
    notchLayout->addWidget(m_drawerNotchIcon, 0, Qt::AlignCenter);
    updateDrawerNotchIcon();

    // Transparent click overlay so the whole notch frame is clickable
    QPushButton* notchBtn = new QPushButton(m_drawerNotch);
    notchBtn->setFlat(true);
    notchBtn->setGeometry(0, 0, DRAWER_NOTCH_W, DRAWER_NOTCH_BTN_H);
    notchBtn->setCursor(Qt::PointingHandCursor);
    notchBtn->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    connect(notchBtn, &QPushButton::clicked, this, [this]()
    {
        m_drawer->toggle();
        updateDrawerNotchIcon();
    });

    connect(m_drawer, &PickerDrawer::drawerWidthChanged, this, [this](int w)
    {
        repositionDrawerNotch(w);
    });

    connect(m_drawer, &PickerDrawer::pickerAvailabilityChanged, this, [this](bool hasAny)
    {
        // A dropdown appearing or going can decide whether they all fit
        // expanded below the content.
        scheduleArrangement();
        if (m_wideMode == true)
        {
            updatePickerSidebarVisibility();
        }
        else
        {
            m_drawer->setVisible(hasAny);
            m_drawerNotch->setVisible(hasAny);
            // Only offset the scroll area; logo/power rows stay full-width.
            const int leftMargin = hasAny ? PickerDrawer::COLLAPSED_DRAWER_WIDTH : 0;
            m_scrollOffsetLayout->setContentsMargins(leftMargin, 0, leftMargin, 0);
            repositionDrawer();
        }
    });

    repositionDrawerNotch(PickerDrawer::COLLAPSED_DRAWER_WIDTH);
    m_drawerNotch->raise();
}

void VpnPage::notifyExternalConnect(const QString& countryCode, const QString& city)
{
    m_activeCity = city;
    // Only a city of the local country belongs in the Selected Location
    // dropdown. Another country's would stay there as the selection, and the
    // power button would later ask for that city in the local country.
    if (isLocalCountry(countryCode) == true)
    {
        m_locationPicker->setSelectedCity(city);
    }
    if (m_recentPicker != nullptr)
    {
        m_recentPicker->refresh();
    }
}

bool VpnPage::isLocalCountry(const QString& countryCode) const
{
    return m_localCountryCode.isEmpty() == false
        && countryCode.compare(m_localCountryCode, Qt::CaseInsensitive) == 0;
}

QString VpnPage::connectionCountry() const
{
    return m_connectedCountryCode.isEmpty() == true ? m_manager->lastConnectCountry() : m_connectedCountryCode;
}

void VpnPage::forgetStatusLocation()
{
    m_connectedCountryCode.clear();
    m_pendingStatusCity.clear();
}

void VpnPage::onSavedConnectionSelected(const QString& countryCode, const QString& city)
{
    // With no connection up or on its way there is nothing to ask about.
    if (m_currentState != VpnState::Connected && m_currentState != VpnState::Connecting)
    {
        m_activeCity = city;
        emit connectRequested(countryCode, city);
        return;
    }

    // A location of the local country goes through the Selected Location
    // dropdown, which asks whether to switch now or on the next reconnect
    // (see onLocationSelectionChanged()).
    if (isLocalCountry(countryCode) == true)
    {
        m_locationPicker->selectCity(city);
        return;
    }

    // Any other country's cannot: the dropdown lists the local country's
    // cities only, so it has nowhere to keep this one for a later reconnect,
    // and sent through it the connection would go to the local country. It
    // is asked about here, and connects to its own country or not at all.
    const bool sameAsConnection = m_hadUnknownConnection == false && city == m_activeCity
                               && countryCode.compare(connectionCountry(), Qt::CaseInsensitive) == 0;
    if (sameAsConnection == true) return;

    const QString countryName = GeoUtils::countryCodeToName(countryCode);
    const QString locationName = city.isEmpty() == true
        ? tr("Fastest server in %1").arg(countryName)
        : connectionLabel(countryCode, countryName, city);
    if (confirmLocationChange(locationName, false) == true)
    {
        m_activeCity = city;
        emit connectRequested(countryCode, city);
    }
}

void VpnPage::onLocationSelectionChanged(const QString& city)
{
    if (m_currentState != VpnState::Connected && m_currentState != VpnState::Connecting) return;

    // Nothing to ask when the connection already goes there. Only one in the
    // local country can: connected anywhere else, the same city name, or the
    // fastest server, is another place.
    const bool sameAsConnection = m_hadUnknownConnection == false && city == m_activeCity
                               && isLocalCountry(connectionCountry()) == true;
    if (sameAsConnection == true) return;

    m_hadUnknownConnection = false;

    const QString locationName = city.isEmpty() == true
        ? tr("Fastest server")
        : GeoUtils::cityWithRegion(m_localCountryCode, city);
    if (confirmLocationChange(locationName, true) == true)
    {
        m_activeCity = city;
        emit connectRequested(m_localCountryCode, city);
    }
}

bool VpnPage::confirmLocationChange(const QString& locationName, const bool canDefer)
{
    // Over the window: the page's stylesheet clears the background of
    // every widget inside the page, so a dialog parented here showed black.
    QDialog* dlg = new QDialog(window());
    dlg->setWindowTitle(tr("Change Location?"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModal(true);
    dlg->setMinimumWidth(CHANGE_LOCATION_DLG_MIN_W);

    QVBoxLayout* layout = new QVBoxLayout(dlg);
    layout->setSpacing(DIALOG_SPACING);
    layout->setContentsMargins(DIALOG_H_MARGIN, DIALOG_H_MARGIN, DIALOG_H_MARGIN, DIALOG_BTM_MARGIN);

    // "On the next reconnect" is only offered where the selection is kept
    // until then, which is the Selected Location dropdown.
    const QString question = canDefer == true
        ? tr("You selected <b>%1</b>.<br>"
             "Would you like to connect to this location now, "
             "or use it on the next reconnect?")
        : tr("You selected <b>%1</b>.<br>"
             "Would you like to switch the connection to this location now?");
    QLabel* msgLabel = new QLabel(question.arg(locationName.toHtmlEscaped()), dlg);
    msgLabel->setWordWrap(true);
    msgLabel->setTextFormat(Qt::RichText);
    layout->addWidget(msgLabel);

    QHBoxLayout* btnRow = new QHBoxLayout();
    btnRow->setSpacing(DIALOG_BTN_SPACING);

    QPushButton* laterBtn = new QPushButton(canDefer == true ? tr("On next reconnect") : tr("Cancel"), dlg);
    laterBtn->setObjectName(QStringLiteral("secondaryButton"));

    QPushButton* nowBtn = new QPushButton(tr("Connect now"), dlg);
    nowBtn->setObjectName(QStringLiteral("primaryButton"));
    nowBtn->setDefault(true);

    const int btnH = nowBtn->sizeHint().height();
    laterBtn->setFixedHeight(btnH);
    nowBtn->setFixedHeight(btnH);

    btnRow->addWidget(laterBtn, FILL_STRETCH);
    btnRow->addWidget(nowBtn, FILL_STRETCH);
    layout->addLayout(btnRow);

    connect(laterBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(nowBtn,   &QPushButton::clicked, dlg, &QDialog::accept);

    return dlg->exec() == QDialog::Accepted;
}

void VpnPage::onStatusCityKnown(const QString& city)
{
    // Kept for the state change that follows this (see updateUi()): it
    // skips the "Active connection" fallback when a city is known, and has
    // applyPendingStatusCity() select the city once the connection's country
    // is known too and the list is populated.
    m_pendingStatusCity = city;
    m_activeCity        = city;
}

void VpnPage::applyPendingStatusCity()
{
    // Before the list has loaded no city can be selected in it; this runs
    // again when it has (see populateLocationPicker()).
    if (m_pendingStatusCity.isEmpty() == true || m_citiesPopulated == false) return;

    // A connection to another country says nothing about the local
    // selection, also when that country has a city of the same name. In the
    // local country, a city that is not listed leaves the dropdown unchanged.
    if (isLocalCountry(connectionCountry()) == true)
    {
        m_locationPicker->trySelectCity(m_pendingStatusCity);
    }
    m_pendingStatusCity.clear();
}

void VpnPage::populateLocationPicker(const QList<QPair<QString, QString>>& cities)
{
    m_locationPicker->populate(cities);
    m_citiesPopulated = true;
    applyPendingStatusCity();
}

void VpnPage::refreshRecentPicker() const
{
    if (m_recentPicker != nullptr)
    {
        m_recentPicker->refresh();
    }
    relayoutPickers();
}

void VpnPage::refreshFavoritesPicker() const
{
    if (m_favoritesPicker != nullptr)
    {
        m_favoritesPicker->refresh();
    }
    relayoutPickers();
}

void VpnPage::setFavoritesDropdownVisible(const bool visible)
{
    m_showFavoritesDropdown = visible;
    relayoutPickers();
}

void VpnPage::setFavoritesEnabled(const bool enabled)
{
    m_favoritesEnabled = enabled;
    relayoutPickers();
}

void VpnPage::setQuickSettingSources(const QList<QuickSettingSource>& sources)
{
    m_quickSettings->setSources(sources);
    updateLayoutForSize();
}

void VpnPage::updateQuickSettings()
{
    m_quickSettings->rebuild();
    updateLayoutForSize();
}

void VpnPage::setLocationPickerVisible(const bool visible)
{
    m_showLocationPicker = visible;
    relayoutPickers();
}

void VpnPage::relayoutPickers() const
{
    // A free account cannot choose a location: Proton picks its server.
    m_locationPicker->setVisible(m_showLocationPicker == true && m_isFreeUser == false);
    if (m_drawer != nullptr)
    {
        m_drawer->syncVisibility(m_isFreeUser, m_showFavoritesDropdown, m_favoritesEnabled);
    }
}

int VpnPage::mainWidth() const
{
    return m_quickSettings->isHidden() == true ? width() : width() - QuickSettingsPanel::PANEL_WIDTH;
}

void VpnPage::repositionDrawer()
{
    if (m_drawer == nullptr) return;
    const int drawerH = m_drawer->sizeHint().height();
    m_drawer->setFixedHeight(drawerH);
    m_drawer->move(0, height() - drawerH);
    repositionDrawerNotch(m_drawer->width());
}

void VpnPage::applyWideMode(bool wide)
{
    if (m_wideMode == wide) return;
    m_wideMode = wide;

    if (wide == true)
    {
        //  Switch to wide mode
        // 1. Release pickers from the drawer and place them in the sidebar.
        //    Banners move to the sidebar above the pickers.
        m_drawer->releasePickers();
        if (m_vpnBannerArea != nullptr)
        {
            QVBoxLayout* sl = qobject_cast<QVBoxLayout*>(m_scrollArea->widget()->layout());
            if (sl != nullptr)
            {
                sl->removeWidget(m_vpnBannerArea);
            }
            // Wrap banner content in a scroll area for the sidebar (created once).
            if (m_vpnBannerContent != nullptr)
            {
                QVBoxLayout* bal = qobject_cast<QVBoxLayout*>(m_vpnBannerArea->layout());
                if (bal != nullptr)
                {
                    bal->removeWidget(m_vpnBannerContent);
                }
                if (m_vpnBannerScroll == nullptr)
                {
                    m_vpnBannerScroll = new QScrollArea(m_vpnBannerArea);
                    m_vpnBannerScroll->setWidgetResizable(true);
                    m_vpnBannerScroll->setFrameShape(QFrame::NoFrame);
                    m_vpnBannerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                    m_vpnBannerScroll->setMaximumHeight(BANNER_SCROLL_MAX_HEIGHT_WIDE);
                }
                m_vpnBannerScroll->setWidget(m_vpnBannerContent);
                if (bal != nullptr)
                {
                    bal->addWidget(m_vpnBannerScroll);
                }
                updateBannerScrollHeight();
            }
            // After the stretch at the top, before the pickers.
            m_pickerSidebarLayout->insertWidget(SIDEBAR_BANNER_INDEX, m_vpnBannerArea);
        }
        m_pickerSidebarLayout->addWidget(m_locationPicker, 0, Qt::AlignLeft);
        m_pickerSidebarLayout->addWidget(m_recentPicker, 0, Qt::AlignLeft);
        if (m_favoritesPicker != nullptr)
        {
            m_pickerSidebarLayout->addWidget(m_favoritesPicker, 0, Qt::AlignLeft);
        }
        // Ensure all pickers are in their expanded (full-size) form.
        // Note: isVisible() cannot be used here because the parent hierarchy
        // (m_wideContent) is still hidden at this point - it would always return
        // false, causing recent/favorites to stay collapsed.  Instead, expand
        // every picker unconditionally; hidden pickers will be given setCollapsed
        // again if they are ever re-shown via syncVisibility in narrow mode.
        m_locationPicker->setCollapsed(false);
        m_recentPicker->setCollapsed(false);
        if (m_favoritesPicker != nullptr)
        {
            m_favoritesPicker->setCollapsed(false);
        }

        // 2. Move topContentWidget and scrollArea into the right column.
        m_narrowContentLayout->removeWidget(m_topContentWidget);
        m_scrollOffsetLayout->removeWidget(m_scrollArea);
        m_rightContentLayout->addWidget(m_topContentWidget);
        m_rightContentLayout->addWidget(m_scrollArea, FILL_STRETCH);

        // 3. Swap visible containers.
        m_narrowContent->setVisible(false);
        m_wideContent->setVisible(true);

        // 4. Hide the drawer overlay and notch.
        m_drawer->setVisible(false);
        m_drawerNotch->setVisible(false);

        // 5. Sidebar is shown only when it has dropdowns or warnings to show.
        updatePickerSidebarVisibility();
    }
    else
    {
        //  Switch to narrow mode
        // 1. Remove pickers from sidebar and reclaim them into the drawer.
        //    Banners move back to the bottom of the scroll content.
        m_pickerSidebarLayout->removeWidget(m_locationPicker);
        m_pickerSidebarLayout->removeWidget(m_recentPicker);
        if (m_favoritesPicker != nullptr)
        {
            m_pickerSidebarLayout->removeWidget(m_favoritesPicker);
        }
        m_drawer->reclaimPickers();
        // Restore collapsed state to match the current drawer state.
        // isVisible() must not be used here: the pickers were just reparented
        // to the hidden drawer, so isVisible() returns false for all of them.
        const bool collapsed = (m_drawer->isExpanded() == false);
        m_locationPicker->setCollapsed(collapsed);
        m_recentPicker->setCollapsed(collapsed);
        if (m_favoritesPicker != nullptr)
        {
            m_favoritesPicker->setCollapsed(collapsed);
        }

        // 2. Move topContentWidget and scrollArea back to the narrow wrapper.
        //    Return the banner area to the scroll content (before the stretch).
        m_rightContentLayout->removeWidget(m_topContentWidget);
        m_rightContentLayout->removeWidget(m_scrollArea);
        m_narrowContentLayout->insertWidget(0, m_topContentWidget);
        m_scrollOffsetLayout->addWidget(m_scrollArea, FILL_STRETCH);
        if (m_vpnBannerArea != nullptr)
        {
            m_pickerSidebarLayout->removeWidget(m_vpnBannerArea);
            // Unwrap banner content from the scroll area back to direct placement.
            if (m_vpnBannerScroll != nullptr && m_vpnBannerContent != nullptr)
            {
                QVBoxLayout* bal = qobject_cast<QVBoxLayout*>(m_vpnBannerArea->layout());
                if (bal != nullptr)
                {
                    bal->removeWidget(m_vpnBannerScroll);
                }
                m_vpnBannerScroll->takeWidget();  // releases m_vpnBannerContent
                if (bal != nullptr)
                {
                    bal->addWidget(m_vpnBannerContent);
                }
            }
            QVBoxLayout* sl = qobject_cast<QVBoxLayout*>(m_scrollArea->widget()->layout());
            if (sl != nullptr)
            {
                sl->insertWidget(sl->count() - 1, m_vpnBannerArea);
            }
        }
        // Restore the scroll offset: only push right when the drawer is present.
        const int leftMargin = m_drawer->hasAnyVisiblePicker() ? PickerDrawer::COLLAPSED_DRAWER_WIDTH : 0;
        m_scrollOffsetLayout->setContentsMargins(leftMargin, 0, leftMargin, 0);

        // 3. Swap visible containers.
        m_wideContent->setVisible(false);
        m_narrowContent->setVisible(true);

        // 4. Restore drawer/notch/margin via the availability signal.
        m_drawer->notifyAvailability();
        repositionDrawer();
    }
}

void VpnPage::repositionDrawerNotch(int drawerW)
{
    if (m_drawerNotch == nullptr || m_drawer == nullptr) return;
    const int notchY = m_drawer->y() + (m_drawer->height() - m_drawerNotch->height()) / 2;
    m_drawerNotch->move(drawerW, notchY);
}

void VpnPage::updateDrawerNotchIcon()
{
    if (m_drawerNotchIcon == nullptr || m_drawer == nullptr) return;
    const QString path = m_drawer->isExpanded() == true
        ? QStringLiteral(":/assets/arrow-bar-left.svg")
        : QStringLiteral(":/assets/arrow-bar-right.svg");
    const QColor tintColor = ThemeManager::isDark() == true
        ? NOTCH_ICON_COLOR_DARK_BG
        : NOTCH_ICON_COLOR_LIGHT_BG;
    m_drawerNotchIcon->setPixmap(GeoUtils::svgPixmap(path, SMALL_ICON_PIX, tintColor));
}

void VpnPage::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        updateDrawerNotchIcon();
    }
}

void VpnPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayoutForSize();
}

void VpnPage::setSplitView(const bool split)
{
    if (m_splitView == split) return;
    m_splitView = split;
    updateLayoutForSize();
}

void VpnPage::updateLayoutForSize()
{
    // The quick settings take their width from the main column. On its own
    // the page shows them only once the column stays wide enough for wide
    // mode beside them; beside the Countries list they stay down to a narrow
    // column, which then uses the narrow layout.
    const int mainNeeded = m_splitView == true ? SPLIT_MIN_MAIN_WIDTH : WIDE_THRESHOLD;
    const bool wanted = AppConfig::instance().showQuickSettings() == true && m_quickSettings->isEmpty() == false;
    m_quickSettings->setVisible(wanted == true && width() >= mainNeeded + QuickSettingsPanel::PANEL_WIDTH);
    m_globe->setGeometry(rect());
    updateArrangement();
    updateContentScale();
    // Again once the new size and scale are laid out.
    scheduleArrangement();
    syncInfoBackdrop();
    if (m_wideMode == false && m_drawer != nullptr)
    {
        repositionDrawer();
    }
}

bool VpnPage::eventFilter(QObject* obj, QEvent* ev)
{
    if (m_scrollArea != nullptr && obj == m_scrollArea->viewport())
    {
        if (ev->type() == QEvent::Resize)
        {
            syncInfoBackdrop();
        }
        return QWidget::eventFilter(obj, ev);
    }
    // Installed on the content and the dropdown sidebar: any change in their
    // height can decide whether the content still fits above the dropdowns.
    if (ev->type() == QEvent::LayoutRequest)
    {
        scheduleArrangement();
    }
    return QWidget::eventFilter(obj, ev);
}

void VpnPage::scheduleArrangement()
{
    // Deferred so the sizes it measures come from the finished layout pass, and
    // coalesced so a burst of layout requests costs one check.
    if (m_arrangementPending == true) return;
    m_arrangementPending = true;
    QTimer::singleShot(0, this, [this]()
    {
        m_arrangementPending = false;
        updateArrangement();
    });
}

// The dropdowns sit at the bottom left, expanded, whenever the page is tall
// enough for the power button, status and info text to fit above them: the
// content then spans the main column's full width and is centered on it,
// like the logo. A page that is not that tall but wide enough keeps the
// dropdowns expanded in a column beside the content, so the two can never
// overlap; a narrow one collapses them into the drawer.
void VpnPage::updateArrangement()
{
    const bool centered = stackedArrangementFits();
    applyWideMode(centered == true || mainWidth() >= WIDE_THRESHOLD);
    if (m_wideMode == false) return;
    if (centered == m_wideCentered) return;
    m_wideCentered = centered;
    m_globe->anchorMoved();

    m_wideGrid->removeWidget(m_pickerSidebar);
    m_wideGrid->removeWidget(m_rightContent);
    if (centered == true)
    {
        m_wideGrid->addWidget(m_rightContent, GRID_TOP_ROW, GRID_SIDEBAR_COLUMN, GRID_ONE_ROW, GRID_BOTH_COLUMNS);
        m_wideGrid->addWidget(m_pickerSidebar, GRID_BOTTOM_ROW, GRID_SIDEBAR_COLUMN);
    }
    else
    {
        m_wideGrid->addWidget(m_pickerSidebar, GRID_TOP_ROW, GRID_SIDEBAR_COLUMN);
        m_wideGrid->addWidget(m_rightContent, GRID_TOP_ROW, GRID_CONTENT_COLUMN);
    }
}

bool VpnPage::stackedArrangementFits() const
{
    // Measured at the column width beside the dropdowns whenever the page is
    // wide enough for that arrangement too: across the full width the text
    // wraps onto fewer lines, and judging by that height could flip the
    // arrangement straight back after switching.
    const int measureW = mainWidth() >= WIDE_THRESHOLD ? mainWidth() - WIDE_SIDEBAR_WIDTH : mainWidth();
    const QLayout* scrollLayout = m_scrollArea->widget()->layout();
    const int scrollH = scrollLayout->hasHeightForWidth()
        ? scrollLayout->heightForWidth(measureW)
        : scrollLayout->sizeHint().height();
    const int contentH = m_topContentWidget->sizeHint().height() + scrollH;
    const int available = height() - m_logoRow->sizeHint().height() - stackedSidebarHeight();
    return contentH + CENTERED_CONTENT_GAP <= available;
}

int VpnPage::stackedSidebarHeight() const
{
    if (m_wideMode == true)
    {
        return m_pickerSidebar->isHidden() == true ? 0 : m_pickerSidebar->sizeHint().height();
    }

    // Narrow, the dropdowns are collapsed in the drawer, so their expanded
    // heights stand in. The warnings, which join them in the sidebar, are
    // still part of the content's height; at least as tall there as in the
    // sidebar (which caps them), so this never finds room the stacked
    // arrangement would not have, and the page cannot flip back and forth.
    int height = 0;
    int shown = 0;
    for (const PickerBase* picker : std::initializer_list<const PickerBase*>{m_locationPicker, m_recentPicker,
                                                                             m_favoritesPicker})
    {
        if (picker == nullptr || picker->isHidden() == true) continue;
        height += picker->expandedHeight();
        ++shown;
    }
    const bool hasWarnings = m_vpnBannerArea->isHidden() == false;
    if (shown == 0 && hasWarnings == false)
    {
        return 0;
    }
    if (shown > 0)
    {
        height += (shown - 1) * SIDEBAR_SPACING;
    }
    if (shown > 0 && hasWarnings == true)
    {
        height += SIDEBAR_SPACING;
    }
    return height + SIDEBAR_B_MARGIN;
}

void VpnPage::updatePickerSidebarVisibility() const
{
    if (m_wideMode == false || m_drawer == nullptr) return;
    m_pickerSidebar->setVisible(m_drawer->hasAnyVisiblePicker() == true || m_vpnBannerArea->isHidden() == false);
}

// Scales the logo, power button, status text and the spacing between them with
// the page, so a large or full-screen window is not a small cluster of controls
// in a sea of empty space. The fixed sizes are designed for the default window,
// where the scale is 1; it grows with whichever of the column width and page
// height grew less, so the column keeps its proportions.
void VpnPage::updateContentScale()
{
    // By the column beside the dropdowns wherever they can sit there, in
    // either wide arrangement; below that width, by the whole main column in
    // both the narrow and the stacked layout.
    const bool sidebarShown = m_wideMode == true && m_pickerSidebar->isHidden() == false
                           && mainWidth() >= WIDE_THRESHOLD;
    const int columnW = mainWidth() - (sidebarShown ? WIDE_SIDEBAR_WIDTH : 0);
    const qreal byWidth  = static_cast<qreal>(columnW) / REFERENCE_PAGE_WIDTH;
    const qreal byHeight = static_cast<qreal>(height()) / REFERENCE_PAGE_HEIGHT;
    const qreal clamped  = std::clamp(std::min(byWidth, byHeight), BASE_CONTENT_SCALE, MAX_CONTENT_SCALE);
    const qreal scale    = std::round(clamped / CONTENT_SCALE_STEP) * CONTENT_SCALE_STEP;
    if (qFuzzyCompare(scale, m_contentScale)) return;
    m_contentScale = scale;

    m_logo->setMaxWidth(qRound(LOGO_MAX_WIDTH * scale));
    m_logoRowLayout->setContentsMargins(PAGE_H_MARGIN, qRound(LOGO_TOP_MARGIN * scale), PAGE_H_MARGIN, 0);
    m_powerBtn->setScale(scale);
    m_topLayout->setSpacing(qRound(TOP_SECTION_SPACING * scale));
    m_topLayout->setContentsMargins(PAGE_H_MARGIN, qRound(TOP_SECTION_TOP_MARGIN * scale),
                                    PAGE_H_MARGIN, qRound(TOP_SECTION_BTM_MARGIN * scale));
    scaleLabelFont(m_statusLabel, scale);
    scaleLabelFont(m_timerLabel, scale);
    scaleLabelFont(m_infoLabel, scale);
    syncInfoBackdrop();
    m_globe->anchorMoved();
}

void VpnPage::onCitiesReady(const QString& countryCode,
                            const QList<QPair<QString, QString>>& cities)
{
    if (countryCode.compare(m_localCountryCode, Qt::CaseInsensitive) != 0) return;

    if (m_stateKnown == false)
    {
        m_pendingCities = cities; // wraps in optional - even an empty list is "received"
        return;
    }

    populateLocationPicker(cities);

    // Update the tracked features for the currently active city so
    // refreshConnectedInfoLabel() can show "Port forwarding is active" when
    // the app starts with the VPN already connected to a P2P server. These
    // are the local country's cities, so only a connection there is looked
    // up in them: another country's city of the same name is another server.
    if (m_activeCity.isEmpty() == false && isLocalCountry(connectionCountry()) == true)
    {
        const auto it = std::ranges::find_if(cities,
            [this](const QPair<QString, QString>& pair)
            {
                return pair.first.compare(m_activeCity, Qt::CaseInsensitive) == 0;
            });
        if (it != cities.end())
        {
            m_currentCityFeatures = it->second;
        }
    }
    if (m_currentState == VpnState::Connected)
    {
        refreshConnectedInfoLabel();
    }
}

void VpnPage::checkPrereleaseBanner()
{
    const AppVersion::Info& version = AppVersion::embedded();
    if (version.prerelease == false) return;

    const QString msg = tr(
        "You are running a <b>pre-release</b> version of this app (<b>v%1</b>). "
        "It may contain bugs or incomplete features. Use with caution.")
        .arg(version.appVersion.toHtmlEscaped());

    m_prereleaseBanner = addWarning(msg);
    connect(m_prereleaseBanner, &InfoBanner::dismissed, this, [this]()
    {
        m_prereleaseBanner = nullptr;
    });
}

InfoBanner* VpnPage::addWarning(const QString& htmlMessage)
{
    InfoBanner* warning = new InfoBanner(htmlMessage, this);
    m_warningBanners.append(warning);
    connect(warning, &InfoBanner::dismissed, this, [this, warning]()
    {
        // Hidden now rather than once this returns, so the warnings' height
        // below leaves it out.
        warning->hide();
        m_warningBanners.removeOne(warning);
        updateBannerAreaVisibility();
    });
    m_vpnBannerLayout->addWidget(warning);
    updateBannerAreaVisibility();
    return warning;
}

void VpnPage::updateBannerAreaVisibility()
{
    const int count = static_cast<int>(m_warningBanners.size());
    const bool hasAny = count > 0;
    m_vpnBannerArea->setVisible(hasAny);
    updateBannerScrollHeight();
    updatePickerSidebarVisibility();
    if (m_warningsHeaderLabel != nullptr)
    {
        m_warningsHeaderLabel->setText(tr("Warnings (%1)").arg(count));
    }
}

void VpnPage::updateBannerScrollHeight() const
{
    if (m_vpnBannerScroll == nullptr) return;
    // A scroll area only ever asks for the size it already has, so it would
    // never grow to show a warning added later; it is sized to its warnings
    // instead, as they wrap in the sidebar, up to its cap.
    const QLayout* bannerLayout = m_vpnBannerContent->layout();
    const int bannerW = WIDE_SIDEBAR_WIDTH - SIDEBAR_L_MARGIN;
    const int contentH = bannerLayout->hasHeightForWidth() == true
        ? bannerLayout->heightForWidth(bannerW)
        : bannerLayout->sizeHint().height();
    m_vpnBannerScroll->setFixedHeight(std::min(contentH, BANNER_SCROLL_MAX_HEIGHT_WIDE));
}

void VpnPage::onCliVersionReady(const QString& version)
{
    const QString cliVersionMin = AppVersion::embedded().cliTestedMin;
    const QString cliVersionMax = AppVersion::embedded().cliTestedMax;

    if (version.isEmpty() == true || (cliVersionMin.isEmpty() == true && cliVersionMax.isEmpty() == true)) return;

    const QVersionNumber installed = QVersionNumber::fromString(version);
    const QVersionNumber verMin    = QVersionNumber::fromString(cliVersionMin);
    const QVersionNumber verMax    = QVersionNumber::fromString(cliVersionMax);

    const bool tooOld = installed.isNull() == false && verMin.isNull() == false
                        && installed < verMin;
    const bool tooNew = installed.isNull() == false && verMax.isNull() == false
                        && installed > verMax;

    if (tooOld == false && tooNew == false) return;

    const QString rangeStr = (cliVersionMin.isEmpty() == false && cliVersionMax.isEmpty() == false)
        ? (cliVersionMin + QStringLiteral("-") + cliVersionMax)
        : (cliVersionMin.isEmpty() == true ? cliVersionMax : cliVersionMin);

    const QString msg = tooNew == true
        ? tr("Your Proton VPN CLI (<b>v%1</b>) is newer than the tested range (<b>%2</b>). "
             "Things may work fine, but you could encounter unexpected behavior.")
              .arg(version, rangeStr)
        : tr("Your Proton VPN CLI (<b>v%1</b>) is older than the tested range (<b>%2</b>). "
             "Some features may not work correctly. Consider upgrading the CLI.")
              .arg(version, rangeStr);

    const QScrollArea* scrollArea = findChild<QScrollArea*>(QStringLiteral("vpnScrollArea"));
    if (scrollArea == nullptr || scrollArea->widget() == nullptr) return;
    QVBoxLayout* scrollLayout = qobject_cast<QVBoxLayout*>(scrollArea->widget()->layout());
    if (scrollLayout == nullptr) return;

    m_versionBanner = new InfoBanner(msg, this);
    connect(m_versionBanner, &InfoBanner::dismissed, this, [this]()
    {
        m_versionBanner = nullptr;
    });
    int pos = 0;
    if (m_prereleaseBanner != nullptr)
    {
        ++pos;
    }
    scrollLayout->insertWidget(pos, m_versionBanner);
}

void VpnPage::onStateChanged(const VpnState state, const QString& info)
{
    updateUi(state, info);
    updateGlobeTarget();
}

void VpnPage::applyGlobeSettings()
{
    m_globe->setPauseWhenInactive(AppConfig::instance().globePauseWhenUnfocused());
    m_globe->setVisible(MotionPreference::instance().animationEnabled(AppConfig::instance().globeAnimation()));
}

void VpnPage::showPowerButtonHint()
{
    if (AppConfig::instance().powerButtonHintShown() == true || m_powerButtonHint != nullptr) return;

    m_powerButtonHint = new HintBubble(
        tr("Turn the VPN On or Off"),
        tr("Click the power button to connect, and click it again to disconnect. "
           "Its ring is green while you're connected and red while you're not."),
        m_powerBtn, this);
    // Beside the button where there is room, so the status below it stays in
    // view, and clear of the quick settings.
    m_powerButtonHint->setSides({HintBubble::Side::Right, HintBubble::Side::Below, HintBubble::Side::Above});
    m_powerButtonHint->setBounds(m_mainColumn);
    connect(m_powerButtonHint, &HintBubble::dismissed, this, []()
    {
        AppConfig::instance().setPowerButtonHintShown(true);
    });
}

// Faces the country being connected to, or connected to; spins otherwise.
void VpnPage::updateGlobeTarget()
{
    QString country;
    switch (m_currentState)
    {
        case VpnState::Connecting:
        case VpnState::Connected:
        case VpnState::Disconnecting:
            // `protonvpn status` names the server actually in use; until it
            // has, this is the requested country, which is empty for the
            // fastest server anywhere: that destination is not known yet.
            country = connectionCountry();
            break;

        default:
            break;
    }
    m_globe->setTarget(country.isEmpty() ? std::nullopt : CountryCenters::find(country));
}

void VpnPage::updateUi(const VpnState state, const QString& info)
{
    const VpnState prevState = m_currentState;
    m_currentState = state;

    if (state != VpnState::Unknown)
    {
        m_checkingSpinnerTimer->stop();
    }

    const bool justBecameKnown = m_stateKnown == false && state != VpnState::Unknown;
    if (justBecameKnown == true)
    {
        m_stateKnown = true;
    }

    // Cities that arrived before the first state snapshot are drained here, for
    // every state.  Doing it inside the Connected/Disconnected branches meant a
    // first known state of Error or Connecting left the location picker stuck
    // on "Loading locations..." with no second citiesReady to rescue it.
    if (justBecameKnown == true && m_pendingCities.has_value() == true)
    {
        populateLocationPicker(*m_pendingCities);
        m_pendingCities.reset();
    }

    m_errorDetailsBtn->setVisible(false);
    m_signOutHintLabel->setVisible(false);
    m_infoLabel->setObjectName(QStringLiteral("infoLabel"));
    m_infoLabel->style()->unpolish(m_infoLabel);
    m_infoLabel->style()->polish(m_infoLabel);
    m_infoLabel->setTextFormat(Qt::AutoText);
    m_infoLabel->setOpenExternalLinks(false);

    switch (state)
    {
        case VpnState::Connected:
            m_powerBtn->setState(PowerButton::RingState::Connected);
            m_powerBtn->setEnabled(true);
            m_statusLabel->setText(tr("Connected"));
            setStyleProperty(m_statusLabel, "vpnState", QStringLiteral("connected"));
            // "... in Secaucus, United States" -> "... in Secaucus, New Jersey,
            // United States". No-op for every other country.
            m_lastConnectedInfo = GeoUtils::withUsStates(info);
            refreshConnectedInfoLabel();
            startNatPmpLoop();

            // On app startup with VPN already connected, the CLI connect output is
            // not available, so "Port forwarding is active on this server." won't be
            // in `info`.  Fetch the city features explicitly and update the label once
            // we know whether it's a P2P server. After a connect the app made, the
            // country is not known yet; this runs again when it arrives.
            fetchConnectedCityFeatures();
            if (prevState == VpnState::Connecting)
            {
                startElapsedTimer();
                // Update the picker immediately using the city known at connect
                // time: m_activeCity is always set at the moment connectRequested
                // was emitted. Only for a connection to the local country, though:
                // one made elsewhere, from a recent or a favorite or the Countries
                // list, leaves the local selection as it was.
                if (isLocalCountry(connectionCountry()) == true)
                {
                    m_locationPicker->trySelectCity(m_activeCity);
                }
                if (m_isFreeUser == false && m_recentPicker != nullptr)
                {
                    m_recentPicker->refresh();
                    relayoutPickers();
                }
            }
            else if (prevState == VpnState::Connected)
            {
                // Server changed while staying connected (external CLI switch).
                // Restart the elapsed timer for the new connection and update the
                // location picker to reflect the new city.
                startElapsedTimer();
                m_hadUnknownConnection = false;
                m_locationPicker->setUnknownConnection(false);
                applyPendingStatusCity();
            }
            else
            {
                stopElapsedTimer();
                // Only fall back to "Active connection" when we have no city at
                // all.  If onStatusCityKnown() was called first, m_activeCity is
                // already set and we skip this so the picker can show the real city.
                if (prevState == VpnState::Unknown && m_activeCity.isEmpty() == true)
                {
                    m_hadUnknownConnection = true;
                    m_locationPicker->setUnknownConnection(true);
                }
                // A connection the status monitor found, at startup or made
                // outside the app: its city is applied here, in this state
                // change, so that it cannot be left over for a later connection.
                applyPendingStatusCity();
            }
            break;

        case VpnState::Disconnected:
            stopNatPmpLoop();
            m_powerBtn->setState(PowerButton::RingState::Disconnected);
            m_powerBtn->setEnabled(true);
            m_statusLabel->setText(tr("Disconnected"));
            setStyleProperty(m_statusLabel, "vpnState", QStringLiteral("disconnected"));
            m_infoLabel->setText(info);
            m_activeCity.clear();
            forgetStatusLocation();
            m_hadUnknownConnection = false;
            m_locationPicker->setUnknownConnection(false);
            stopElapsedTimer();
            break;

        case VpnState::Connecting:
            stopNatPmpLoop();
            m_powerBtn->setState(PowerButton::RingState::Spinning);
            m_powerBtn->setEnabled(false);
            m_statusLabel->setText(tr("Connecting\u2026"));
            setStyleProperty(m_statusLabel, "vpnState", QStringLiteral("transitioning"));
            m_infoLabel->setText(QString());
            // A new connection may go elsewhere, so what the status said
            // about the last one no longer holds, and neither does "Active
            // connection" for one found at startup: this one the page knows.
            forgetStatusLocation();
            // The city comes from the manager, which also starts connections
            // the page did not ask for: the reconnect after a setting change
            // and the startup auto-connect. The page's own copy was cleared
            // by the disconnect in between, which reset the picker to
            // "Fastest" once such a connection was up.
            m_activeCity = m_manager->lastConnectCity();
            m_hadUnknownConnection = false;
            m_locationPicker->setUnknownConnection(false);
            stopElapsedTimer();
            break;

        case VpnState::Disconnecting:
            stopNatPmpLoop();
            m_powerBtn->setState(PowerButton::RingState::Spinning);
            m_powerBtn->setEnabled(false);
            m_statusLabel->setText(tr("Disconnecting\u2026"));
            setStyleProperty(m_statusLabel, "vpnState", QStringLiteral("transitioning"));
            stopElapsedTimer();
            break;

        case VpnState::Error:
        {
            stopNatPmpLoop();
            m_powerBtn->setState(PowerButton::RingState::Disconnected);
            m_powerBtn->setEnabled(true);
            m_statusLabel->setText(tr("Error"));
            setStyleProperty(m_statusLabel, "vpnState", QStringLiteral("error"));
            stopElapsedTimer();

            forgetStatusLocation();

            m_rawError = info;

            // A Python traceback is the CLI itself breaking, which is worth a
            // bug report to its authors. Anything else is the CLI saying why
            // it could not do what was asked (no such server, no network, a
            // value it rejects), and its own words say that best.
            const bool isCliError = info.contains(QLatin1String("Traceback (most recent call last)"))
                                 || info.contains(QLatin1String("File \"/usr/bin/protonvpn\""))
                                 || info.contains(QLatin1String("File \"/usr/lib/python"));
            if (isCliError == true)
            {
                // Rich text, for the link, so the line break is a <br>: a
                // newline there is only white space.
                m_infoLabel->setTextFormat(Qt::RichText);
                m_infoLabel->setOpenExternalLinks(true);
                m_infoLabel->setText(tr(
                    "An error occurred in the Proton VPN CLI.<br>"
                    "Please file a bug report at "
                    "<a href='https://github.com/ProtonVPN/proton-vpn-cli/issues'>github.com/ProtonVPN/proton-vpn-cli/</a>."));
                m_signOutHintLabel->setText(tr(
                    "<span style='color:#f5a623;'>&#9888;</span>"
                    " CLI errors can sometimes be resolved by "
                    "<a href='action://signout' style='color:#ab8fff;'>signing out and signing back in</a>."));
                m_signOutHintLabel->setVisible(true);
            }
            else
            {
                // Plain text: nothing the CLI printed is taken for markup.
                const QString summary = CliErrorSummary::summarize(info);
                m_infoLabel->setTextFormat(Qt::PlainText);
                m_infoLabel->setText(summary.isEmpty() == true
                    ? tr("The Proton VPN CLI reported an error without saying what went wrong.")
                    : summary);
            }
            m_infoLabel->setObjectName(QStringLiteral("errorLabel"));
            m_infoLabel->style()->unpolish(m_infoLabel);
            m_infoLabel->style()->polish(m_infoLabel);
            m_errorDetailsBtn->setVisible(info.trimmed().isEmpty() == false);
            break;
        }

        default: // Unknown
            m_powerBtn->setState(PowerButton::RingState::Unknown);
            m_powerBtn->setEnabled(false);
            m_statusLabel->setText(QStringLiteral("\u280b Checking\u2026"));
            setStyleProperty(m_statusLabel, "vpnState", QStringLiteral("unknown"));
            m_infoLabel->setText(QString());
            stopElapsedTimer();
            break;
    }
    syncInfoBackdrop();
}

// The info label's backdrop over the globe (style.qss) would show as an empty
// pill whenever the label has no text, so it is switched off then. The label
// is also sized to its text: a word-wrapped QLabel left to pick its own width
// squeezes short text into a narrow column, and stretched across the page its
// backdrop would be a band rather than a pill.
void VpnPage::syncInfoBackdrop() const
{
    const int available = std::max(0, m_scrollArea->viewport()->width() - 2 * PAGE_H_MARGIN);

    // Most calls come from resize and layout steps that change nothing the
    // width depends on; restyling and measuring the text again is skipped then.
    const QString state = QStringList{m_infoLabel->text(), m_infoLabel->objectName(),
                                      QString::number(m_infoLabel->textFormat()),
                                      QString::number(available),
                                      m_infoLabel->font().toString()}.join(QChar::LineFeed);
    if (state == m_infoBackdropState) return;
    m_infoBackdropState = state;

    setStyleProperty(m_infoLabel, "empty", m_infoLabel->text().isEmpty());

    QTextDocument doc;
    doc.setDefaultFont(m_infoLabel->font());
    if (m_infoLabel->textFormat() == Qt::RichText
        || (m_infoLabel->textFormat() == Qt::AutoText && Qt::mightBeRichText(m_infoLabel->text())))
    {
        doc.setHtml(m_infoLabel->text());
    }
    else
    {
        doc.setPlainText(m_infoLabel->text());
    }
    const QMargins frame = m_infoLabel->contentsMargins();
    const int chrome = frame.left() + frame.right() + INFO_LABEL_SLACK;
    m_infoLabel->setFixedWidth(std::min(available, static_cast<int>(std::ceil(doc.idealWidth())) + chrome));
}

void VpnPage::startElapsedTimer()
{
    m_connectedSince.start();
    m_timerLabel->setText(QStringLiteral("00:00:00"));
    m_timerLabel->setVisible(true);
    m_elapsedTimer->start();
}

void VpnPage::stopElapsedTimer() const
{
    m_elapsedTimer->stop();
    m_timerLabel->setVisible(false);
}

void VpnPage::showErrorDetails()
{
    // Over the window, for the same reason as the Change Location dialog.
    ErrorDetailsDialog* dlg = new ErrorDetailsDialog(m_rawError, window());
    dlg->exec();
}

// ---------------------------------------------------------------------------
// Port forwarding - NatPmpManager integration
// ---------------------------------------------------------------------------

void VpnPage::refreshConnectedInfoLabel() const
{
    QString text = m_lastConnectedInfo;

    // Append the port-forwarding notice if:
    //   1. The setting is enabled in the VPN configuration, AND
    //   2. The active city's server features include P2P, AND
    //   3. The text doesn't already contain the notice (avoids duplicates from
    //      the live CLI connect path, which already includes it).
    const bool pfEnabled = m_manager->portForwardingEnabled();
    const bool isP2P     = m_currentCityFeatures.contains(
                               QLatin1String("p2p"), Qt::CaseInsensitive);
    const QString pfNote = tr("Port forwarding is active on this server.");
    if (pfEnabled == true && isP2P == true && text.contains(pfNote) == false)
    {
        if (text.isEmpty() == false)
        {
            text += QLatin1Char('\n');
        }
        text += pfNote;
    }

    m_infoLabel->setText(text);
    syncInfoBackdrop();
}

void VpnPage::startNatPmpLoop()
{
    // The keep-alive asks the server for a forwarded port. With the setting
    // off there is none to ask for, and the loop would run natpmpc every 45
    // seconds for nothing and count as active port forwarding when quitting.
    if (m_manager->portForwardingEnabled() == false) return;

    if (NatPmpManager::isInstalled() == false)
    {
        showNatpmpcBanner();
        return;
    }

    // natpmpc is available - dismiss any stale "not installed" banner that
    // may have been shown before the user installed the package at runtime.
    dismissNatpmpcBanner();

    // refresh() fires an immediate port-mapping request whether or not the
    // keep-alive loop was already running, preventing a 45-second wait.
    m_natPmpManager->refresh();
}

void VpnPage::showNatpmpcBanner()
{
    // Port forwarding is off - don't nag the user.
    if (m_manager->portForwardingEnabled() == false) return;
    // Already showing.
    if (m_natpmpcBanner != nullptr) return;

    const QScrollArea* scrollArea = findChild<QScrollArea*>(QStringLiteral("vpnScrollArea"));
    if (scrollArea == nullptr || scrollArea->widget() == nullptr) return;
    QVBoxLayout* scrollLayout = qobject_cast<QVBoxLayout*>(scrollArea->widget()->layout());
    if (scrollLayout == nullptr) return;

    m_natpmpcBanner = new InfoBanner(
        tr("<b>natpmpc is not installed.</b> "
           "The forwarded port cannot be displayed or kept alive automatically. "
           "Install it to use port forwarding."),
        this);
    connect(m_natpmpcBanner, &InfoBanner::dismissed, this, [this]()
    {
        m_natpmpcBanner = nullptr;
    });
    int pos = 0;
    if (m_prereleaseBanner != nullptr)
    {
        ++pos;
    }
    if (m_versionBanner != nullptr)
    {
        ++pos;
    }
    scrollLayout->insertWidget(pos, m_natpmpcBanner);
}

void VpnPage::stopNatPmpLoop()
{
    stopPortForwarding();

    m_currentCityFeatures.clear();
    m_cityFeaturesRequest.clear();
    m_lastConnectedInfo.clear();
}

void VpnPage::fetchConnectedCityFeatures()
{
    if (m_currentState != VpnState::Connected || m_manager->portForwardingEnabled() == false) return;
    if (m_connectedCountryCode.isEmpty() == true || m_activeCity.isEmpty() == true) return;

    // Asked once per server: the country arriving and the state change that
    // follows it both come through here.
    const QString cc      = m_connectedCountryCode;
    const QString city    = m_activeCity;
    const QString request = cc + QLatin1Char('/') + city;
    if (request == m_cityFeaturesRequest) return;
    m_cityFeaturesRequest = request;

    m_manager->fetchCityFeatures(cc, city, [this, cc, city](const QString& features)
    {
        // Guard: still connected to the same server when the result arrives.
        const bool sameServer = m_currentState == VpnState::Connected
                             && m_connectedCountryCode == cc && m_activeCity == city;
        if (sameServer == false) return;
        m_currentCityFeatures = features;
        refreshConnectedInfoLabel();
    });
}

void VpnPage::stopPortForwarding()
{
    m_natPmpManager->stop();
    if (m_portRow != nullptr)
    {
        m_portRow->setVisible(false);
    }
}

void VpnPage::dismissNatpmpcBanner()
{
    if (m_natpmpcBanner != nullptr)
    {
        m_natpmpcBanner->deleteLater();
        m_natpmpcBanner = nullptr;
    }
}

