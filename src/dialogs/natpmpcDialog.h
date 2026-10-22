#pragma once

#include <QDialog>

// Shown when port forwarding is turned on without natpmpc installed: explains
// that the forwarded port cannot be shown or kept alive without it, and gives
// the install command for each distribution, with a copy button.
class NatpmpcDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NatpmpcDialog(QWidget* parent = nullptr);
};
