// ============================================================================
// StacksModel.cpp — Folder Aggregator Model for tinexus-dock (Slice 7)
// Ref: Architecture Blueprint §8, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#include "dock/StacksModel.hpp"
#include <common/logger.hpp>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QUrl>
#include <algorithm>

namespace tinexus::dock {

StacksModel::StacksModel(QObject* parent)
    : StacksModel(QDir::homePath() + QStringLiteral("/Downloads"), parent)
{
}

StacksModel::StacksModel(const QString& initialPath, QObject* parent)
    : QAbstractListModel(parent)
{
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(60); // 60ms debounce to avoid inotify storm
    connect(&m_debounceTimer, &QTimer::timeout, this, &StacksModel::reloadFiles);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &StacksModel::onDirectoryChanged);

    setDirectoryPath(initialPath);
}

int StacksModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_items.size());
}

QVariant StacksModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return {};
    }

    const auto& item = m_items[static_cast<size_t>(index.row())];

    switch (role) {
    case Qt::DisplayRole:
    case StackRoles::FileNameRole:
        return item.fileName;
    case StackRoles::FilePathRole:
        return item.filePath;
    case StackRoles::FileSizeRole:
        return item.fileSizeFormatted;
    case StackRoles::FileDateRole:
        return item.fileDateFormatted;
    case StackRoles::FileTimeRole:
        return item.modifiedTime.toMSecsSinceEpoch();
    case StackRoles::IconTypeRole:
        return item.iconType;
    case StackRoles::IsDirectoryRole:
        return item.isDirectory;
    case StackRoles::FileUrlRole:
        return QUrl::fromLocalFile(item.filePath);
    default:
        return {};
    }
}

QHash<int, QByteArray> StacksModel::roleNames() const {
    return {
        { StackRoles::FileNameRole,    "fileName"    },
        { StackRoles::FilePathRole,    "filePath"    },
        { StackRoles::FileSizeRole,    "fileSize"    },
        { StackRoles::FileDateRole,    "fileDate"    },
        { StackRoles::FileTimeRole,    "fileTime"    },
        { StackRoles::IconTypeRole,    "iconType"    },
        { StackRoles::IsDirectoryRole, "isDirectory" },
        { StackRoles::FileUrlRole,     "fileUrl"     }
    };
}

void StacksModel::setDirectoryPath(const QString& path) {
    QString expanded = path;
    if (expanded.startsWith(QStringLiteral("~/"))) {
        expanded = QDir::homePath() + expanded.mid(1);
    } else if (expanded == QStringLiteral("~")) {
        expanded = QDir::homePath();
    }

    if (m_dirPath == expanded) return;

    m_dirPath = expanded;
    setupWatcher();
    reloadFiles();
    emit directoryPathChanged(m_dirPath);
}

QString StacksModel::folderName() const {
    if (m_dirPath.isEmpty()) return QStringLiteral("Stack");
    QDir dir(m_dirPath);
    return dir.dirName();
}

void StacksModel::setSortMode(StackSortMode mode) {
    if (m_sortMode == mode) return;
    m_sortMode = mode;
    beginResetModel();
    sortItems();
    endResetModel();
    emit sortModeChanged(static_cast<int>(m_sortMode));
}

void StacksModel::setSortModeInt(int mode) {
    setSortMode(static_cast<StackSortMode>(mode));
}

void StacksModel::refresh() {
    reloadFiles();
}

void StacksModel::openFile(int index) {
    if (index < 0 || index >= static_cast<int>(m_items.size())) return;
    openPath(m_items[static_cast<size_t>(index)].filePath);
}

void StacksModel::openPath(const QString& path) {
    tinexus::log::info("[StacksModel] Opening file: '{}'", path.toStdString());

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"));
    proc.setProcessEnvironment(env);

    QFileInfo fi(path);
    if (fi.isDir()) {
        proc.startDetached(QStringLiteral("tinexus-files"), {path});
    } else {
        proc.startDetached(QStringLiteral("xdg-open"), {path});
    }

    emit fileOpened(path);
}

void StacksModel::openFolder() {
    tinexus::log::info("[StacksModel] Opening folder: '{}'", m_dirPath.toStdString());
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"));
    proc.setProcessEnvironment(env);
    proc.startDetached(QStringLiteral("tinexus-files"), {m_dirPath});
}

void StacksModel::setupWatcher() {
    const QStringList watched = m_watcher.directories();
    if (!watched.isEmpty()) {
        m_watcher.removePaths(watched);
    }

    if (!m_dirPath.isEmpty() && QDir(m_dirPath).exists()) {
        m_watcher.addPath(m_dirPath);
        tinexus::log::info("[StacksModel] inotify watcher active on '{}'", m_dirPath.toStdString());
    }
}

void StacksModel::onDirectoryChanged(const QString&) {
    m_debounceTimer.start();
}

void StacksModel::reloadFiles() {
    QDir dir(m_dirPath);
    if (!dir.exists()) {
        tinexus::log::warn("[StacksModel] Directory '{}' does not exist", m_dirPath.toStdString());
        beginResetModel();
        m_items.clear();
        endResetModel();
        emit countChanged(0);
        return;
    }

    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::NoSort
    );

    std::vector<StackFileItem> newItems;
    newItems.reserve(entries.size());

    for (const auto& fi : entries) {
        StackFileItem item;
        item.fileName          = fi.fileName();
        item.filePath          = fi.absoluteFilePath();
        item.fileSizeBytes     = fi.size();
        item.fileSizeFormatted = item.isDirectory ? QStringLiteral("Folder") : formatFileSize(fi.size());
        item.modifiedTime      = fi.lastModified();
        item.fileDateFormatted = fi.lastModified().toString(QStringLiteral("MMM d, yyyy"));
        item.iconType          = detectIconType(fi);
        item.isDirectory       = fi.isDir();
        newItems.push_back(std::move(item));
    }

    beginResetModel();
    m_items = std::move(newItems);
    sortItems();
    endResetModel();

    emit countChanged(static_cast<int>(m_items.size()));
    tinexus::log::debug("[StacksModel] Loaded {} items from '{}'", m_items.size(), m_dirPath.toStdString());
}

void StacksModel::sortItems() {
    switch (m_sortMode) {
    case StackSortMode::ByDateDesc:
        std::stable_sort(m_items.begin(), m_items.end(), [](const StackFileItem& a, const StackFileItem& b) {
            return a.modifiedTime > b.modifiedTime;
        });
        break;
    case StackSortMode::ByDateAsc:
        std::stable_sort(m_items.begin(), m_items.end(), [](const StackFileItem& a, const StackFileItem& b) {
            return a.modifiedTime < b.modifiedTime;
        });
        break;
    case StackSortMode::ByNameAsc:
        std::stable_sort(m_items.begin(), m_items.end(), [](const StackFileItem& a, const StackFileItem& b) {
            return a.fileName.localeAwareCompare(b.fileName) < 0;
        });
        break;
    case StackSortMode::ByNameDesc:
        std::stable_sort(m_items.begin(), m_items.end(), [](const StackFileItem& a, const StackFileItem& b) {
            return a.fileName.localeAwareCompare(b.fileName) > 0;
        });
        break;
    case StackSortMode::ByKind:
        std::stable_sort(m_items.begin(), m_items.end(), [](const StackFileItem& a, const StackFileItem& b) {
            if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
            if (a.iconType != b.iconType) return a.iconType < b.iconType;
            return a.fileName.localeAwareCompare(b.fileName) < 0;
        });
        break;
    }
}

QString StacksModel::detectIconType(const QFileInfo& info) {
    if (info.isDir()) return QStringLiteral("folder");

    const QString ext = info.suffix().toLower();
    if (ext == QStringLiteral("pdf")) {
        return QStringLiteral("pdf");
    }
    if (ext == QStringLiteral("png") || ext == QStringLiteral("jpg") || ext == QStringLiteral("jpeg") ||
        ext == QStringLiteral("svg") || ext == QStringLiteral("gif") || ext == QStringLiteral("webp") ||
        ext == QStringLiteral("bmp") || ext == QStringLiteral("tiff")) {
        return QStringLiteral("image");
    }
    if (ext == QStringLiteral("mp3") || ext == QStringLiteral("wav") || ext == QStringLiteral("flac") ||
        ext == QStringLiteral("ogg") || ext == QStringLiteral("m4a") || ext == QStringLiteral("aac")) {
        return QStringLiteral("audio");
    }
    if (ext == QStringLiteral("mp4") || ext == QStringLiteral("mkv") || ext == QStringLiteral("avi") ||
        ext == QStringLiteral("mov") || ext == QStringLiteral("webm")) {
        return QStringLiteral("video");
    }
    if (ext == QStringLiteral("zip") || ext == QStringLiteral("tar") || ext == QStringLiteral("gz") ||
        ext == QStringLiteral("bz2") || ext == QStringLiteral("xz")  || ext == QStringLiteral("7z") ||
        ext == QStringLiteral("rar") || ext == QStringLiteral("zst")) {
        return QStringLiteral("archive");
    }
    if (ext == QStringLiteral("cpp") || ext == QStringLiteral("hpp") || ext == QStringLiteral("c") ||
        ext == QStringLiteral("h")   || ext == QStringLiteral("py")  || ext == QStringLiteral("js") ||
        ext == QStringLiteral("ts")  || ext == QStringLiteral("qml") || ext == QStringLiteral("json") ||
        ext == QStringLiteral("toml")|| ext == QStringLiteral("yaml")|| ext == QStringLiteral("sh") ||
        ext == QStringLiteral("rs")  || ext == QStringLiteral("go")) {
        return QStringLiteral("code");
    }
    if (ext == QStringLiteral("txt") || ext == QStringLiteral("md")  || ext == QStringLiteral("rtf") ||
        ext == QStringLiteral("log") || ext == QStringLiteral("csv")) {
        return QStringLiteral("text");
    }

    return QStringLiteral("generic");
}

QString StacksModel::formatFileSize(qint64 bytes) {
    if (bytes < 1024) {
        return QString::number(bytes) + QStringLiteral(" B");
    }
    if (bytes < 1024 * 1024) {
        return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    }
    if (bytes < 1024 * 1024 * 1024) {
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    }
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + QStringLiteral(" GB");
}

} // namespace tinexus::dock

#include "moc_StacksModel.cpp"
