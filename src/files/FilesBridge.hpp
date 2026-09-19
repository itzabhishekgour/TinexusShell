// ============================================================================
// FilesBridge.hpp — Qt6 Bridge for tinexus-files with Real Filesystem Backend
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QStringList>
#include <QtCore/QSocketNotifier>
#include <sys/inotify.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <files/file_model.hpp>
#include <files/file_operations.hpp>

namespace tinexus::files {

class FilesBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString currentDirName READ currentDirName NOTIFY currentPathChanged)
    Q_PROPERTY(QString homePath READ homePath CONSTANT)
    Q_PROPERTY(QVariantList sidebarLocations READ sidebarLocations CONSTANT)
    Q_PROPERTY(QVariantList fileList READ fileList NOTIFY fileListChanged)
    Q_PROPERTY(QVariantList columnsData READ columnsData NOTIFY columnsDataChanged)
    Q_PROPERTY(int columnsVersion READ columnsVersion NOTIFY columnsDataChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY navigationChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY fileListChanged)
    Q_PROPERTY(int currentViewMode READ currentViewMode WRITE setViewMode NOTIFY viewModeChanged)
    // Phase 5: Multi-Select Properties
    Q_PROPERTY(QStringList selectedPaths READ selectedPaths WRITE setSelectedPaths NOTIFY selectedPathsChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedPathsChanged)
    Q_PROPERTY(QString selectedPath READ selectedPath WRITE selectItem NOTIFY selectedPathChanged)
    Q_PROPERTY(QVariantMap selectedItemDetails READ selectedItemDetails NOTIFY selectedPathChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY clipboardChanged)
    Q_PROPERTY(int clipboardCount READ clipboardCount NOTIFY clipboardChanged)
    Q_PROPERTY(QString sortColumn READ sortColumn NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)
    // Phase 1: Tag revision counter — incremented on every tag mutation
    Q_PROPERTY(int tagRevision READ tagRevision NOTIFY tagDataChanged)

    // Phase 2A: Non-blocking Async File Operations & Telemetry
    Q_PROPERTY(bool isOperating READ isOperating NOTIFY operationStateChanged)
    Q_PROPERTY(double operationProgress READ operationProgress NOTIFY operationProgressChanged)
    Q_PROPERTY(qint64 bytesTransferred READ bytesTransferred NOTIFY operationProgressChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY operationProgressChanged)
    Q_PROPERTY(QString operationSpeedStr READ operationSpeedStr NOTIFY operationProgressChanged)
    Q_PROPERTY(QString currentOperationName READ currentOperationName NOTIFY operationStateChanged)

    // Phase 2B: Conflict Dialog Properties
    Q_PROPERTY(bool conflictDialogVisible READ conflictDialogVisible NOTIFY conflictDialogVisibleChanged)
    Q_PROPERTY(QVariantMap conflictDetails READ conflictDetails NOTIFY conflictDetailsChanged)
    Q_PROPERTY(bool applyToAll READ applyToAll WRITE setApplyToAll NOTIFY applyToAllChanged)

    // Phase 3: Instant Search & Interactive Address Bar
    Q_PROPERTY(bool isSearching READ isSearching NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(int searchResultCount READ searchResultCount NOTIFY searchResultsChanged)
    Q_PROPERTY(bool isSearchFinished READ isSearchFinished NOTIFY searchFinishedChanged)

    // Phase 4: Multi-Tab Navigation
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(int activeTabIndex READ activeTabIndex NOTIFY activeTabIndexChanged)
    Q_PROPERTY(int tabCount READ tabCount NOTIFY tabsChanged)

public:
    explicit FilesBridge(const QString& initialPath = QString(), QObject* parent = nullptr);
    ~FilesBridge() override;

    QString currentPath() const { return m_currentPath; }
    QString currentDirName() const;
    QString homePath() const;
    QVariantList sidebarLocations() const;
    QVariantList fileList() const { return m_isSearching ? m_searchResults : m_fileList; }
    QVariantList columnsData() const { return m_columnsData; }
    int columnsVersion() const { return m_columnsVersion; }
    bool canGoBack() const { return m_historyIndex > 0; }
    bool canGoForward() const { return m_historyIndex + 1 < static_cast<int>(m_history.size()); }
    int itemCount() const { return static_cast<int>((m_isSearching ? m_searchResults : m_fileList).size()); }
    int currentViewMode() const { return m_viewMode; }
    void setViewMode(int mode);

    // Phase 5: Multi-Select getters & setters
    QString selectedPath() const { return m_selectedPaths.isEmpty() ? QString() : m_selectedPaths.first(); }
    QStringList selectedPaths() const { return m_selectedPaths; }
    int selectedCount() const { return static_cast<int>(m_selectedPaths.size()); }
    void selectItem(const QString& path);
    void setSelectedPaths(const QStringList& paths);
    QVariantMap selectedItemDetails() const;
    bool hasClipboard() const { return !m_clipboardPaths.isEmpty(); }
    int clipboardCount() const { return static_cast<int>(m_clipboardPaths.size()); }
    QString sortColumn() const { return m_sortColumn; }
    bool sortAscending() const { return m_sortAscending; }
    int tagRevision() const { return m_tagRevision; }

    // Phase 2A: Async telemetry getters
    bool isOperating() const { return m_isOperating; }
    double operationProgress() const { return m_operationProgress; }
    qint64 bytesTransferred() const { return m_bytesTransferred; }
    qint64 totalBytes() const { return m_totalBytes; }
    QString operationSpeedStr() const { return m_operationSpeedStr; }
    QString currentOperationName() const { return m_currentOperationName; }

    // Phase 2B: Conflict dialog getters
    bool conflictDialogVisible() const { return m_conflictDialogVisible; }
    QVariantMap conflictDetails() const { return m_conflictDetails; }
    bool applyToAll() const { return m_applyToAll; }
    void setApplyToAll(bool val);

    // Phase 3: Instant Search & Address Bar getters
    bool isSearching() const { return m_isSearching; }
    QString searchQuery() const { return m_searchQuery; }
    void setSearchQuery(const QString& q);
    int searchResultCount() const { return static_cast<int>(m_searchResults.size()); }
    bool isSearchFinished() const { return m_searchFinished; }

    Q_INVOKABLE void startSearch(const QString& query, bool recursive = true);
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE bool navigateToPath(const QString& rawPath);

    // Phase 4: Multi-Tab Navigation
    QVariantList tabs() const;
    int activeTabIndex() const { return m_activeTabIndex; }
    int tabCount() const { return static_cast<int>(m_tabs.size()); }

    Q_INVOKABLE int createTab(const QString& path = QString());
    Q_INVOKABLE bool closeTab(int index);
    Q_INVOKABLE void switchTab(int index);
    Q_INVOKABLE void duplicateTab(int index);

    Q_INVOKABLE void cancelCurrentOperation();

    // Phase 2B: Conflict resolution from UI or test harness
    Q_INVOKABLE void resolveConflict(int resolution, bool applyToAll = false);
    Q_INVOKABLE void setBatchConflictPolicy(int policy);

    Q_INVOKABLE void cd(const QString& path);
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void openItem(const QString& path, bool isDir);
    Q_INVOKABLE void refresh();

    // Column View Hierarchy
    Q_INVOKABLE void selectColumnItem(int colIdx, int itemIdx);
    Q_INVOKABLE void openColumnItem(int colIdx, int itemIdx);
    Q_INVOKABLE int getColumnSelectedIndex(int colIdx) const;

    // File operations & context actions
    Q_INVOKABLE bool createNewFolder(const QString& folderName);
    Q_INVOKABLE bool createNewFile(const QString& fileName);
    Q_INVOKABLE bool renameItem(const QString& oldPath, const QString& newName);
    Q_INVOKABLE bool moveItem(const QString& srcPath, const QString& destDir);
    Q_INVOKABLE bool deleteItem(const QString& path);
    Q_INVOKABLE bool deleteSelected();
    Q_INVOKABLE void copyItem(const QString& path);
    Q_INVOKABLE void copyItems(const QStringList& paths);
    Q_INVOKABLE void cutItem(const QString& path);
    Q_INVOKABLE void cutItems(const QStringList& paths);
    Q_INVOKABLE void copySelected();
    Q_INVOKABLE void cutSelected();
    Q_INVOKABLE bool pasteItem(const QString& targetDir = QString());
    Q_INVOKABLE QVariantList getFolderContents(const QString& folderPath);
    Q_INVOKABLE QVariantMap getFileDetails(const QString& path);

    // Phase 5: Multi-Select Operations
    Q_INVOKABLE void toggleSelectItem(const QString& path);
    Q_INVOKABLE void selectRange(const QString& targetPath);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE bool isSelected(const QString& path) const;
    Q_INVOKABLE void setTagOnSelected(const QString& tagName);

    // Phase 1: Sort-by-column
    Q_INVOKABLE void sortBy(const QString& column, bool ascending);

    // Phase 1: Tag apply
    Q_INVOKABLE void setTagOnItem(const QString& path, const QString& tagName);
    Q_INVOKABLE QString getTagForItem(const QString& path) const;

signals:
    void currentPathChanged();
    void fileListChanged();
    void columnsDataChanged();
    void navigationChanged();
    void viewModeChanged();
    void selectedPathChanged();
    void selectedPathsChanged();
    void clipboardChanged();
    void sortChanged();
    void tagDataChanged(); // fires on every tag mutation so QML bindings re-evaluate:

    // Phase 2A signals
    void operationStateChanged();
    void operationProgressChanged();
    void operationCompleted(bool success, const QString& error);

    // Phase 2B signals
    void conflictDialogVisibleChanged();
    void conflictDetailsChanged();
    void applyToAllChanged();

    // Phase 3 signals
    void searchStateChanged();
    void searchQueryChanged();
    void searchResultsChanged();
    void searchFinishedChanged();
    void searchFinished(int totalFound);

    // Phase 4 signals
    void tabsChanged();
    void activeTabIndexChanged();

private:
    void loadDirectory(const QString& path, bool addToHistory = true);
    void rebuildColumnsData();

    // Phase 6: inotify Live File Watcher
    void watchDirectory(const QString& path);
    void unwatchDirectory();
    void onInotifyEvent();

    int m_inotifyFd{-1};
    int m_watchDescriptor{-1};
    QSocketNotifier* m_inotifyNotifier{nullptr};

    QString m_currentPath;
    QVariantList m_fileList;
    QVariantList m_columnsData;
    std::vector<QString> m_history;
    int m_historyIndex{-1};
    int m_viewMode{0}; // 0=Icons, 1=List, 2=Columns, 3=Gallery
    QStringList m_selectedPaths;
    QStringList m_clipboardPaths;
    bool m_clipboardIsCut{false};
    int m_columnsVersion{0};

    // Sort state (Phase 1)
    QString m_sortColumn{QStringLiteral("name")};
    bool m_sortAscending{true};
    SortCriteria m_sortCriteria{SortCriteria::Name};
    SortDirection m_sortDirection{SortDirection::Ascending};

    // Tag revision counter (Phase 1) — incremented on every tag mutation
    int m_tagRevision{0};

    // Phase 2A: Async worker thread & telemetry state
    std::jthread m_workerThread;
    bool m_isOperating{false};
    double m_operationProgress{0.0};
    qint64 m_bytesTransferred{0};
    qint64 m_totalBytes{0};
    QString m_operationSpeedStr;
    QString m_currentOperationName;

    // Phase 2B: Conflict resolution synchronization
    std::condition_variable_any m_conflictCv;
    std::mutex m_conflictMutex;
    bool m_conflictResolved{false};
    ConflictResolution m_chosenResolution{ConflictResolution::Skip};
    BatchConflictPolicy m_batchConflictPolicy{BatchConflictPolicy::AskEach};
    bool m_conflictDialogVisible{false};
    QVariantMap m_conflictDetails;
    bool m_applyToAll{false};

    // Phase 3: Background search worker & state
    std::jthread m_searchThread;
    bool m_isSearching{false};
    bool m_searchFinished{true};
    QString m_searchQuery;
    QVariantList m_searchResults;
    uint64_t m_searchGeneration{0};

    // Phase 4: Multi-Tab State
    struct TabInfo {
        int id{0};
        QString title;
        QString currentPath;
        std::vector<QString> history;
        int historyIndex{-1};
        int viewMode{0};
        QStringList selectedPaths;
        QString sortColumn{QStringLiteral("name")};
        bool sortAscending{true};
    };

    std::vector<TabInfo> m_tabs;
    int m_activeTabIndex{0};
    int m_nextTabId{0};
};


} // namespace tinexus::files
