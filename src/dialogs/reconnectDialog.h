#pragma once

#include <QDialog>

// Asked before changing a VPN setting that only takes effect on a new
// connection while the VPN is not disconnected: "Apply & Reconnect" accepts,
// "Dismiss" rejects.
class ReconnectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReconnectDialog(const QString& settingLabel, QWidget* parent = nullptr);

    // Shows the dialog and returns true if the user chose Apply & Reconnect.
    static bool confirm(const QString& settingLabel, QWidget* parent);
};
