// ============================================================================
// StacksModel.hpp — Folder Aggregator Model for tinexus-dock (Slice 7)
// Ref: Architecture Blueprint §8, docs/05_UI_UX_GUIDELINES.md
// Inotify-backed QFileSystemWatcher monitoring, dynamic sorting, and file opening.
// ============================================================================
#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QString>
#include <QtCore/QFileInfo>
#include <QtCore/QDateTime>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QTimer>
#include <vector>

namespace tinexus::dock {

enum class StackSortMode : int {
    ByDateDesc = 0, // Most recent first (default)
    ByDateAsc  = 1,
    ByNameAsc  = 2,
    ByNameDesc = 3,
    ByKind     = 4
};

struct StackFileItem {
    QString   fileName;
    QString   filePath;
    QString   fileSizeFormatted;
    QString   fileDateFormatted;
    qint64    fileSizeBytes{0};
    QDateTime modifiedTime;
    QString   iconType; // "pdf", "image", "audio", "video", "archive", "code", "text", "folder", "generic"
    bool      isDirectory{false};
};

class StacksModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(QString directoryPath READ directoryPath WRITE setDirectoryPath NOTIFY directoryPathChanged)
    Q_PROPERTY(QString folderName READ folderName NOTIFY directoryPathChanged)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY countChanged)
    Q_PROPERTY(int sortMode READ sortModeInt WRITE setSortModeInt NOTIFY sortModeChanged)

public:
    enum StackRoles {
        FileNameRole = Qt::UserRole + 1,
        FilePathRole,
        FileSizeRole,
        FileDateRole,
        FileTimeRole,
        IconTypeRole,
        IsDirectoryRole,
        FileUrlRole
    };
    Q_ENUM(StackRoles)

    explicit StacksModel(QObject* parent = nullptr);
    explicit StacksModel(const QString& initialPath, QObject* parent = nullptr);
    ~StacksModel() override = default;

    // QAbstractListModel interface
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Properties
    [[nodiscard]] QString directoryPath() const { return m_dirPath; }
    void setDirectoryPath(const QString& path);

    [[nodiscard]] QString folderName() const;
    [[nodiscard]] int fileCount() const { return static_cast<int>(m_items.size()); }
    [[nodiscard]] StackSortMode sortMode() const { return m_sortMode; }
    [[nodiscard]] int sortModeInt() const { return static_cast<int>(m_sortMode); }
    void setSortMode(StackSortMode mode);
    void setSortModeInt(int mode);

    // QML Invokables
    Q_INVOKABLE void openFile(int index);
    Q_INVOKABLE void openPath(const QString& path);
    Q_INVOKABLE void openFolder();
    Q_INVOKABLE void refresh();

signals:
    void directoryPathChanged(const QString& path);
    void countChanged(int count);
    void sortModeChanged(int mode);
    void fileOpened(const QString& path);

private slots:
    void onDirectoryChanged(const QString& path);
    void reloadFiles();

private:
    void setupWatcher();
    void sortItems();
    static QString detectIconType(const QFileInfo& info);
    static QString formatFileSize(qint64 bytes);

    QString                     m_dirPath;
    StackSortMode               m_sortMode{StackSortMode::ByDateDesc};
    std::vector<StackFileItem>  m_items;
    QFileSystemWatcher          m_watcher;
    QTimer                      m_debounceTimer;
};

} // namespace tinexus::dock
