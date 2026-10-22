#include "globeAnimationCombo.h"
#include "../appConfig.h"
#include "../motionPreference.h"

#include <QSignalBlocker>

GlobeAnimationCombo::GlobeAnimationCombo(QWidget* parent)
    : StyledComboBox(parent)
{
    reload();
    connect(this, &QComboBox::currentIndexChanged, this, [this](int)
    {
        AppConfig::instance().setGlobeAnimation(AppConfig::globeAnimationFromName(currentData().toString()));
        emit changed();
    });
    // Detection is asynchronous: "Auto" appears once the desktop answers.
    connect(&MotionPreference::instance(), &MotionPreference::changed, this, &GlobeAnimationCombo::reload);
}

void GlobeAnimationCombo::reload()
{
    // Rebuilding is not a user choice; nothing should be saved for it.
    const QSignalBlocker blocker(this);
    clear();

    const bool autoAvailable = MotionPreference::instance().support() == MotionPreference::Support::Available;
    if (autoAvailable == true)
    {
        addItem(tr("Auto"), AppConfig::globeAnimationName(AppConfig::GlobeAnimation::Auto));
        setItemData(0, tr("Follows your desktop's reduce-motion setting."), Qt::ToolTipRole);
    }
    addItem(tr("On"),  AppConfig::globeAnimationName(AppConfig::GlobeAnimation::On));
    addItem(tr("Off"), AppConfig::globeAnimationName(AppConfig::GlobeAnimation::Off));

    // A saved "auto" shows as On when the desktop's preference cannot be
    // read, since that is what it does then.
    AppConfig::GlobeAnimation shown = AppConfig::instance().globeAnimation();
    if (shown == AppConfig::GlobeAnimation::Auto && autoAvailable == false)
    {
        shown = AppConfig::GlobeAnimation::On;
    }
    setCurrentIndex(findData(AppConfig::globeAnimationName(shown)));
}
