#pragma once

#include <QList>
#include <QWidget>
#include "quickSettingSource.h"

class QHBoxLayout;
class QLabel;
class QScrollArea;
class VpnManager;

// The VPN page's quick settings: the settings the user picked on the Settings
// page (by right-clicking them there), shown beside the page's content when
// it is wide enough. Each row is a compact copy of a Settings page control
// that hands every change to the original and follows whatever state the
// original is in, so the two always agree and a change behaves the same from
// either place.
class QuickSettingsPanel : public QWidget
{
    Q_OBJECT

public:
    // The width the panel takes from the page, margins included.
    static constexpr int PANEL_WIDTH = 320;

    explicit QuickSettingsPanel(VpnManager* manager, QWidget* parent = nullptr);

    // The settings it can show; see rebuild().
    void setSources(const QList<QuickSettingSource>& sources);
    // Shows the settings chosen in AppConfig::quickSettings(), grouped and
    // ordered as the Settings page lists them.
    void rebuild();
    // True while none of the chosen settings is shown.
    [[nodiscard]] bool isEmpty() const { return m_rows.isEmpty(); }

signals:
    // The user took a setting out with its right-click menu.
    void quickSettingsChanged();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    struct Row
    {
        QuickSettingSource source;
        QWidget* widget = nullptr;
        QHBoxLayout* layout = nullptr;
        QLabel* label = nullptr;
        QWidget* control = nullptr; // the copy of source.control
    };

    VpnManager* m_manager;
    QScrollArea* m_scroll = nullptr;
    QList<QuickSettingSource> m_sources;
    QList<Row> m_rows;
    bool m_syncPending = false;
    // Above and below each row's control (see fitRows()).
    int m_rowPadding;

    QWidget* makeRow(QWidget* parent, const QuickSettingSource& source);
    // Gives the rows as much padding as the panel's height leaves room for,
    // up to the full amount.
    void fitRows();
    // The copy of the source's control, passing the user's changes on to it.
    QWidget* makeControl(QWidget* parent, const QuickSettingSource& source);
    // Custom DNS asks for the servers here, then sets them as the Settings
    // page's field and Apply button would.
    void turnCustomDns(const QuickSettingSource& source, bool on);
    // Copies every original's value and state to its copy. scheduleSync()
    // does so once, after the event being handled.
    void sync();
    void scheduleSync();
};
