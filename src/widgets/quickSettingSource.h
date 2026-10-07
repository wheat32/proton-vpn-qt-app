#pragma once

#include <QString>

class QLineEdit;
class QPushButton;
class QWidget;

// A setting on the Settings page that the VPN page's quick settings can show.
// The quick settings show a compact copy of its control and hand every change
// to the original, so a change made from either place runs the Settings
// page's own logic (see QuickSettingsPanel).
struct QuickSettingSource
{
    // The quick settings group their settings by the Settings tab they are on.
    enum class Section { Vpn, Appearance, App };

    // What `control` is, which decides the copy the quick settings show.
    enum class Kind
    {
        Toggle,    // ToggleWithStatus
        Combo,     // QComboBox
        Number,    // NumberSpinner
        Button,    // QPushButton
        CustomDns, // ToggleWithStatus, plus the servers field and its Apply button
    };

    QString id;    // see quickSettingIds.h
    QString label; // as the quick settings show it
    Section section = Section::App;
    Kind kind = Kind::Toggle;
    // The row on the Settings page and the control in it. While the row is
    // hidden the setting does not apply (auto-connect without launch on
    // startup), and the quick settings show it disabled.
    QWidget* row = nullptr;
    QWidget* control = nullptr;
    // Custom DNS only.
    QLineEdit* dnsEdit = nullptr;
    QPushButton* dnsApply = nullptr;
    // Only Plus accounts can change it; the quick settings say so to free ones.
    bool plusOnly = false;
};
