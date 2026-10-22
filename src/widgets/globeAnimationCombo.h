#pragma once

#include "styledComboBox.h"

// The Globe Animation setting as a combo box, for the Settings page and the
// VPN page's quick settings. "Auto" is listed only when the desktop's
// reduce-motion preference can be read (MotionPreference::Available), and the
// list follows that as it is detected. Choosing an item saves it to AppConfig.
class GlobeAnimationCombo : public StyledComboBox
{
    Q_OBJECT

public:
    explicit GlobeAnimationCombo(QWidget* parent = nullptr);

    // Refills the list and selects the saved setting, for when it changed
    // somewhere else. Emits nothing.
    void reload();

signals:
    // The user chose an item; it is already saved.
    void changed();
};
