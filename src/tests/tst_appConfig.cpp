#include <QtTest/QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "appConfig.h"
#include "quickSettingIds.h"

// AppConfig is a singleton, so all test slots share the same instance.
// We use QStandardPaths::setTestModeEnabled(true) to redirect file I/O to a
// throwaway location so we never touch the real ~/.config/ProtonVPN-Qt/.

class TstAppConfig : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Redirect all standard paths to a temporary location so the tests
        // never read from (or write to) the user's real config directory.
        QStandardPaths::setTestModeEnabled(true);
    }

    void cleanupTestCase()
    {
        QStandardPaths::setTestModeEnabled(false);
    }

    //  Default values
    // With no pre-existing config file the singleton must expose the defaults
    // documented in appConfig.h.
    void defaults_autoConnectIsFalse()
    {
        QCOMPARE(AppConfig::instance().autoConnect(), false);
    }

    void defaults_notificationsIsTrue()
    {
        QCOMPARE(AppConfig::instance().notifications(), true);
    }

    void defaults_recentConnectionsCountIsFive()
    {
        QCOMPARE(AppConfig::instance().recentConnectionsCount(), 5);
    }

    void defaults_startHiddenIsFalse()
    {
        QCOMPARE(AppConfig::instance().startHidden(), false);
    }

    void defaults_themeIsSystem()
    {
        QCOMPARE(AppConfig::instance().theme(), AppConfig::Theme::System);
    }

    //  Setters round-trip
    void setAutoConnect_storesValue()
    {
        AppConfig::instance().setAutoConnect(true);
        QCOMPARE(AppConfig::instance().autoConnect(), true);
        // Restore
        AppConfig::instance().setAutoConnect(false);
    }

    void setNotifications_storesValue()
    {
        AppConfig::instance().setNotifications(false);
        QCOMPARE(AppConfig::instance().notifications(), false);
        // Restore
        AppConfig::instance().setNotifications(true);
    }

    void setStartHidden_storesValue()
    {
        AppConfig::instance().setStartHidden(true);
        QCOMPARE(AppConfig::instance().startHidden(), true);
        // Restore
        AppConfig::instance().setStartHidden(false);
    }

    //  Theme round-trip
    void setTheme_dark_storesValue()
    {
        AppConfig::instance().setTheme(AppConfig::Theme::Dark);
        QCOMPARE(AppConfig::instance().theme(), AppConfig::Theme::Dark);
    }

    void setTheme_light_storesValue()
    {
        AppConfig::instance().setTheme(AppConfig::Theme::Light);
        QCOMPARE(AppConfig::instance().theme(), AppConfig::Theme::Light);
    }

    void setTheme_system_storesValue()
    {
        AppConfig::instance().setTheme(AppConfig::Theme::System);
        QCOMPARE(AppConfig::instance().theme(), AppConfig::Theme::System);
    }

    //  recentConnectionsCount boundary
    void setRecentConnectionsCount_negativeValue_clampsToZero()
    {
        AppConfig::instance().setRecentConnectionsCount(-1);
        QCOMPARE(AppConfig::instance().recentConnectionsCount(), 0);
        // Restore to default
        AppConfig::instance().setRecentConnectionsCount(5);
    }

    void setRecentConnectionsCount_zero_allowed()
    {
        AppConfig::instance().setRecentConnectionsCount(0);
        QCOMPARE(AppConfig::instance().recentConnectionsCount(), 0);
        // Restore
        AppConfig::instance().setRecentConnectionsCount(5);
    }

    void setRecentConnectionsCount_positiveValue_storesValue()
    {
        AppConfig::instance().setRecentConnectionsCount(10);
        QCOMPARE(AppConfig::instance().recentConnectionsCount(), 10);
        // Restore
        AppConfig::instance().setRecentConnectionsCount(5);
    }

    void defaults_globeAnimationIsAuto()
    {
        QVERIFY(AppConfig::instance().globeAnimation() == AppConfig::GlobeAnimation::Auto);
    }

    void setGlobeAnimation_storesValue()
    {
        AppConfig::instance().setGlobeAnimation(AppConfig::GlobeAnimation::Off);
        QVERIFY(AppConfig::instance().globeAnimation() == AppConfig::GlobeAnimation::Off);
        // Restore
        AppConfig::instance().setGlobeAnimation(AppConfig::GlobeAnimation::Auto);
    }

    void defaults_globePauseWhenUnfocusedIsFalse()
    {
        QCOMPARE(AppConfig::instance().globePauseWhenUnfocused(), false);
    }

    void setGlobePauseWhenUnfocused_storesValue()
    {
        AppConfig::instance().setGlobePauseWhenUnfocused(true);
        QCOMPARE(AppConfig::instance().globePauseWhenUnfocused(), true);
        // Restore
        AppConfig::instance().setGlobePauseWhenUnfocused(false);
    }

    void globeAnimationName_eachValue_isDistinct()
    {
        QCOMPARE(AppConfig::globeAnimationName(AppConfig::GlobeAnimation::Auto), QStringLiteral("auto"));
        QCOMPARE(AppConfig::globeAnimationName(AppConfig::GlobeAnimation::On),   QStringLiteral("on"));
        QCOMPARE(AppConfig::globeAnimationName(AppConfig::GlobeAnimation::Off),  QStringLiteral("off"));
    }

    void globeAnimationFromName_roundTripsAndDefaultsToAuto()
    {
        for (const AppConfig::GlobeAnimation value : {AppConfig::GlobeAnimation::Auto,
                                                      AppConfig::GlobeAnimation::On,
                                                      AppConfig::GlobeAnimation::Off})
        {
            QCOMPARE(AppConfig::globeAnimationFromName(AppConfig::globeAnimationName(value)), value);
        }
        QCOMPARE(AppConfig::globeAnimationFromName(QStringLiteral("sometimes")), AppConfig::GlobeAnimation::Auto);
        QCOMPARE(AppConfig::globeAnimationFromName(QString()), AppConfig::GlobeAnimation::Auto);
    }

    void defaults_splitViewCountriesRatioIsDefault()
    {
        QCOMPARE(AppConfig::instance().splitViewCountriesRatio(), AppConfig::SPLIT_RATIO_DEFAULT);
    }

    void setSplitViewCountriesRatio_inRange_storesValue()
    {
        AppConfig::instance().setSplitViewCountriesRatio(0.25);
        QCOMPARE(AppConfig::instance().splitViewCountriesRatio(), 0.25);
        // Restore
        AppConfig::instance().setSplitViewCountriesRatio(AppConfig::SPLIT_RATIO_DEFAULT);
    }

    void setSplitViewCountriesRatio_outOfRange_clamps()
    {
        AppConfig::instance().setSplitViewCountriesRatio(0.0);
        QCOMPARE(AppConfig::instance().splitViewCountriesRatio(), AppConfig::SPLIT_RATIO_MIN);
        AppConfig::instance().setSplitViewCountriesRatio(1.0);
        QCOMPARE(AppConfig::instance().splitViewCountriesRatio(), AppConfig::SPLIT_RATIO_MAX);
        // Restore
        AppConfig::instance().setSplitViewCountriesRatio(AppConfig::SPLIT_RATIO_DEFAULT);
    }

    void splitRatioMax_isAThird()
    {
        QVERIFY(qFuzzyCompare(AppConfig::SPLIT_RATIO_MAX * 3.0, 1.0));
        QVERIFY(AppConfig::SPLIT_RATIO_DEFAULT <= AppConfig::SPLIT_RATIO_MAX);
    }

    //  Main page: countries and quick settings
    void defaults_showCountriesSidebarAndQuickSettingsAreTrue()
    {
        QCOMPARE(AppConfig::instance().showCountriesSidebar(), true);
        QCOMPARE(AppConfig::instance().showQuickSettings(), true);
    }

    void setShowCountriesSidebar_storesValue()
    {
        AppConfig::instance().setShowCountriesSidebar(false);
        QCOMPARE(AppConfig::instance().showCountriesSidebar(), false);
        // Restore
        AppConfig::instance().setShowCountriesSidebar(true);
    }

    void setShowQuickSettings_storesValue()
    {
        AppConfig::instance().setShowQuickSettings(false);
        QCOMPARE(AppConfig::instance().showQuickSettings(), false);
        // Restore
        AppConfig::instance().setShowQuickSettings(true);
    }

    void defaults_quickSettingsAreTheDefaults()
    {
        QCOMPARE(AppConfig::instance().quickSettings(), AppConfig::defaultQuickSettings());
    }

    void defaultQuickSettings_fitAndIncludeShowCountries()
    {
        const QStringList ids = AppConfig::defaultQuickSettings();
        QVERIFY(ids.size() <= AppConfig::QUICK_SETTINGS_MAX);
        QVERIFY(ids.contains(QuickSettingIds::SHOW_COUNTRIES));
        QStringList unique = ids;
        unique.removeDuplicates();
        QCOMPARE(unique, ids);
    }

    void setQuickSettings_dropsRepeats()
    {
        AppConfig::instance().setQuickSettings({QuickSettingIds::IPV6, QuickSettingIds::THEME, QuickSettingIds::IPV6});
        QCOMPARE(AppConfig::instance().quickSettings(), QStringList({QuickSettingIds::IPV6, QuickSettingIds::THEME}));
        // Restore
        AppConfig::instance().setQuickSettings(AppConfig::defaultQuickSettings());
    }

    void setQuickSettings_tooMany_keepsTheFirstMax()
    {
        QStringList ids;
        for (int i = 0; i <= AppConfig::QUICK_SETTINGS_MAX; ++i)
        {
            ids << QStringLiteral("setting-%1").arg(i);
        }
        AppConfig::instance().setQuickSettings(ids);
        QCOMPARE(AppConfig::instance().quickSettings(), ids.mid(0, AppConfig::QUICK_SETTINGS_MAX));
        // Restore
        AppConfig::instance().setQuickSettings(AppConfig::defaultQuickSettings());
    }

    void setQuickSettings_empty_isKept()
    {
        AppConfig::instance().setQuickSettings({});
        QVERIFY(AppConfig::instance().quickSettings().isEmpty());
        // Restore
        AppConfig::instance().setQuickSettings(AppConfig::defaultQuickSettings());
    }
};

QTEST_MAIN(TstAppConfig)
#include "tst_appConfig.moc"

