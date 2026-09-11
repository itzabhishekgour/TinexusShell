// ============================================================================
// LauncherBridge.hpp — C++20 QObject Bridge for tinexus-launcher
// Ref: 05_UI_UX_GUIDELINES.md §4.2, §5.4, §5.5, §6.1, §6.4, §10.2
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <vector>
#include <string>

namespace tinexus::launcher {

struct LauncherItem {
    QString name;
    QString exec;
    QString description;
    QString icon;
    QString kind; // "App", "System", "Calculator", "Store"
    bool    isTerminal{false};
};

class LauncherBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantList results READ resultsList NOTIFY resultsChanged)
    Q_PROPERTY(bool hasResults READ hasResults NOTIFY resultsChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)

public:
    explicit LauncherBridge(QObject* parent = nullptr);
    ~LauncherBridge() override = default;

    [[nodiscard]] QString query() const { return m_query; }
    [[nodiscard]] int selectedIndex() const { return m_selectedIndex; }
    [[nodiscard]] QVariantList resultsList() const;
    [[nodiscard]] bool hasResults() const { return !m_results.empty(); }
    [[nodiscard]] bool reducedMotion() const { return m_reducedMotion; }

    void setQuery(const QString& q);
    void setSelectedIndex(int idx);
    void setReducedMotion(bool rm);

    // QML Invokables
    Q_INVOKABLE void selectNext();
    Q_INVOKABLE void selectPrev();
    Q_INVOKABLE void launchSelected();
    Q_INVOKABLE void launchIndex(int idx);
    Q_INVOKABLE void tabComplete();
    Q_INVOKABLE void closeLauncher();

    // Test Harness Support
    void setAllApps(const std::vector<LauncherItem>& apps);
    const std::vector<LauncherItem>& rawResults() const { return m_results; }

    static bool tryEvalCalc(const QString& q, double& outVal);

signals:
    void queryChanged();
    void selectedIndexChanged();
    void resultsChanged();
    void reducedMotionChanged();
    void closeRequested();

private:
    void refreshResults();
    void scanDesktopApps();
    void spawnApp(const QString& execCmd, bool inTerminal);

    QString m_query;
    int     m_selectedIndex{0};
    bool    m_reducedMotion{false};

    std::vector<LauncherItem> m_allApps;
    std::vector<LauncherItem> m_results;
};

} // namespace tinexus::launcher
