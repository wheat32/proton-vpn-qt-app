#include <QApplication>
#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>
#include "themeManager.h"

namespace ThemeManager
{

namespace
{
// QColor::lightness() runs 0-255; a Window color below the middle is dark.
constexpr int LIGHTNESS_MIDPOINT = 128;

// The Proton purple, the same in both themes.
constexpr QColor ACCENT(0x6d, 0x4a, 0xff);

// Dark theme
constexpr QColor DARK_BACKGROUND(0x1a, 0x1a, 0x2e);
constexpr QColor DARK_SURFACE(0x25, 0x25, 0x3d);
constexpr QColor DARK_BORDER(0x3a, 0x3a, 0x55);
constexpr QColor DARK_TEXT_PRIMARY(0xea, 0xea, 0xea);
constexpr QColor DARK_TEXT_SECONDARY(0x99, 0x99, 0xbb);
constexpr QColor DARK_SHADOW(0x00, 0x00, 0x00, 0x80);

// Light theme
constexpr QColor LIGHT_BACKGROUND(0xf0, 0xf0, 0xf5);
constexpr QColor LIGHT_SURFACE(0xff, 0xff, 0xff);
constexpr QColor LIGHT_BORDER(0xc8, 0xc8, 0xd8);
constexpr QColor LIGHT_TEXT_PRIMARY(0x1a, 0x1a, 0x2e);
constexpr QColor LIGHT_TEXT_SECONDARY(0x5a, 0x5a, 0x7a);
constexpr QColor LIGHT_MIDLIGHT(0xe8, 0xe8, 0xf4);
constexpr QColor LIGHT_SHADOW(0x00, 0x00, 0x00, 0x30);

void applyDark()
{
    QPalette palette;
    palette.setColor(QPalette::Window,          DARK_BACKGROUND);
    palette.setColor(QPalette::WindowText,      DARK_TEXT_PRIMARY);
    palette.setColor(QPalette::Base,            DARK_SURFACE);
    palette.setColor(QPalette::AlternateBase,   DARK_BACKGROUND);
    palette.setColor(QPalette::ToolTipBase,     DARK_SURFACE);
    palette.setColor(QPalette::ToolTipText,     DARK_TEXT_PRIMARY);
    palette.setColor(QPalette::Text,            DARK_TEXT_PRIMARY);
    palette.setColor(QPalette::BrightText,      Qt::white);
    palette.setColor(QPalette::Button,          DARK_SURFACE);
    palette.setColor(QPalette::ButtonText,      DARK_TEXT_PRIMARY);
    palette.setColor(QPalette::Link,            ACCENT);
    palette.setColor(QPalette::Highlight,       ACCENT);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, DARK_TEXT_SECONDARY);
    palette.setColor(QPalette::Mid,             DARK_BORDER);
    palette.setColor(QPalette::Dark,            DARK_BORDER);
    palette.setColor(QPalette::Midlight,        DARK_SURFACE);
    palette.setColor(QPalette::Shadow,          DARK_SHADOW);
    QApplication::setPalette(palette);

    QFile f(QStringLiteral(":/style.qss"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text) == true)
    {
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
    }
}

void applyLight()
{
    QPalette palette;
    palette.setColor(QPalette::Window,          LIGHT_BACKGROUND);
    palette.setColor(QPalette::WindowText,      LIGHT_TEXT_PRIMARY);
    palette.setColor(QPalette::Base,            LIGHT_SURFACE);
    palette.setColor(QPalette::AlternateBase,   LIGHT_BACKGROUND);
    palette.setColor(QPalette::ToolTipBase,     LIGHT_SURFACE);
    palette.setColor(QPalette::ToolTipText,     LIGHT_TEXT_PRIMARY);
    palette.setColor(QPalette::Text,            LIGHT_TEXT_PRIMARY);
    palette.setColor(QPalette::BrightText,      Qt::black);
    palette.setColor(QPalette::Button,          LIGHT_SURFACE);
    palette.setColor(QPalette::ButtonText,      LIGHT_TEXT_PRIMARY);
    palette.setColor(QPalette::Link,            ACCENT);
    palette.setColor(QPalette::Highlight,       ACCENT);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, LIGHT_TEXT_SECONDARY);
    palette.setColor(QPalette::Mid,             LIGHT_BORDER);
    palette.setColor(QPalette::Dark,            LIGHT_BORDER);
    palette.setColor(QPalette::Midlight,        LIGHT_MIDLIGHT);
    palette.setColor(QPalette::Shadow,          LIGHT_SHADOW);
    QApplication::setPalette(palette);

    QFile f(QStringLiteral(":/style_light.qss"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text) == true)
    {
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
    }
}

// Detect whether the system is using a dark color scheme.
bool systemIsDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    // Fall back: check the system palette window background luminance.
    return isDark();
#endif
}

} // anonymous namespace

bool isDark()
{
    return QGuiApplication::palette().color(QPalette::Window).lightness() < LIGHTNESS_MIDPOINT;
}

void apply(const AppConfig::Theme theme)
{
    bool useDark = false;
    switch (theme)
    {
        case AppConfig::Theme::Dark:
            useDark = true;
            break;

        case AppConfig::Theme::Light:
            useDark = false;
            break;

        default:
            useDark = systemIsDark();
            break;
    }

    if (useDark == true)
    {
        applyDark();
    }
    else
    {
        applyLight();
    }
}

} // namespace ThemeManager
