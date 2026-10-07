#pragma once

#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include "../cli/signin/signinFlow.h"
#ifdef QT_DEBUG
#include <QLineEdit>
#endif

// ---------------------------------------------------------------------------
// DebugPage – only included in debug builds (QT_DEBUG).
// Provides developer-facing tools such as triggering dialogs manually and
// inspecting / resetting saved application settings.
// ---------------------------------------------------------------------------
class DebugPage : public QWidget
{
    Q_OBJECT

public:
    explicit DebugPage(QWidget* parent = nullptr);
    void setLeftPadding(int px);

signals:
    // Ask MainWindow to show one of its startup failure pages without having
    // to reproduce the failure: a missing CLI, or a login check that timed out.
    void notInstalledPageRequested();
    void cliNotRespondingPageRequested();
    // Show the login page on one of its second-factor screens.
    void signinScreenRequested(SigninPrompt prompt, const QString& error);
    // Add a sample warning to the home screen, or put it into its error state
    // with sample CLI output, for checking how its layout copes.
    void homeWarningRequested(const QString& message);
    void homeErrorRequested(const QString& details);
    // Put the home screen back the way the connection state has it.
    void homeErrorHidden();

private slots:
    void refreshValues() const;

private:
    QVBoxLayout* m_outerLayout = nullptr;

    // Value labels in the per-key settings grid (kept for live refresh)
    QLabel* m_valAutoConnect        = nullptr;
    QLabel* m_valNotifications      = nullptr;
    QLabel* m_valRecentCount        = nullptr;
    QLabel* m_valStartHidden        = nullptr;
    QLabel* m_valShowPicker         = nullptr;
    QLabel* m_valTheme              = nullptr;
    QLabel* m_valLastSeenVersion    = nullptr;
    QLabel* m_valCheckForUpdates    = nullptr;
    QLabel* m_valLogToFile          = nullptr;

#ifdef QT_DEBUG
    QLineEdit* m_migrationVersionInput = nullptr;
#endif
    // Sample warnings added so far, to number the next one.
    int m_homeWarningCount = 0;
    // Whether the sample error is on the home screen (its button hides it).
    bool m_homeErrorShown = false;
};
