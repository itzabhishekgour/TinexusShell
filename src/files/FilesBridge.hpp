// ============================================================================
// FilesBridge.hpp — Qt6 Bridge for tinexus-files with Real Filesystem Backend
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QStringList>
#include <vector>

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
    Q_PROPERTY(QString selectedPath READ selectedPath WRITE selectItem NOTIFY selectedPathChanged)
    Q_PROPERTY(QVariantMap selectedItemDetails READ selectedItemDetails NOTIFY selectedPathChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY clipboardChanged)

public:
    explicit FilesBridge(const QString& initialPath = QString(), QObject* parent = nullptr);
    ~FilesBridge() override = default;

    QString currentPath() const { return m_currentPath; }
    QString currentDirName() const;
    QString homePath() const;
    QVariantList sidebarLocations() const;
    QVariantList fileList() const { return m_fileList; }
    QVariantList columnsData() const { return m_columnsData; }
    int columnsVersion() const { return m_columnsVersion; }
    bool canGoBack() const { return m_historyIndex > 0; }
    bool canGoForward() const { return m_historyIndex + 1 < static_cast<int>(m_history.size()); }
    int itemCount() const { return static_cast<int>(m_fileList.size()); }
    int currentViewMode() const { return m_viewMode; }
    void setViewMode(int mode);
    QString selectedPath() const { return m_selectedPath; }
    void selectItem(const QString& path);
    QVariantMap selectedItemDetails() const;
    bool hasClipboard() const { return !m_clipboardPath.isEmpty(); }

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
    Q_INVOKABLE void copyItem(const QString& path);
    Q_INVOKABLE void cutItem(const QString& path);
    Q_INVOKABLE bool pasteItem(const QString& targetDir = QString());
    Q_INVOKABLE QVariantList getFolderContents(const QString& folderPath);
    Q_INVOKABLE QVariantMap getFileDetails(const QString& path);

signals:
    void currentPathChanged();
    void fileListChanged();
    void columnsDataChanged();
    void navigationChanged();
    void viewModeChanged();
    void selectedPathChanged();
    void clipboardChanged();

private:
    void loadDirectory(const QString& path, bool addToHistory = true);
    void rebuildColumnsData();

    QString m_currentPath;
    QVariantList m_fileList;
    QVariantList m_columnsData;
    std::vector<QString> m_history;
    int m_historyIndex{-1};
    int m_viewMode{0}; // 0=Icons, 1=List, 2=Columns, 3=Gallery
    QString m_selectedPath;
    QString m_clipboardPath;
    bool m_clipboardIsCut{false};
    int m_columnsVersion{0};
};

} // namespace tinexus::files
