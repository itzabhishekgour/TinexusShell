// ============================================================================
// FilesBridge.cpp â€” Qt6 Bridge for tinexus-files with Real Filesystem Backend
// ============================================================================
#include "FilesBridge.hpp"
#include <common/logger.hpp>
#include <files/TagManager.hpp>
#include <files/file_operations.hpp>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QDateTime>
#include <algorithm>

namespace tinexus::files {

FilesBridge::FilesBridge(const QString& initialPath, QObject* parent)
    : QObject(parent)
{
    QString home = homePath();
    for (const auto& sub : {QStringLiteral("/Desktop"), QStringLiteral("/Documents"), QStringLiteral("/Downloads"), QStringLiteral("/Pictures"), QStringLiteral("/Videos"), QStringLiteral("/.local/share/Trash/files")}) {
        QDir().mkpath(home + sub);
    }

    QString start = initialPath;
    if (start.isEmpty() || !QDir(start).exists()) {
        if (!home.isEmpty() && QDir(home).exists()) {
            start = home;
        } else {
            start = QDir::currentPath();
        }
    }
    cd(start);

    // Phase 6: Initialize inotify watcher
    m_inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_inotifyFd >= 0) {
        m_inotifyNotifier = new QSocketNotifier(m_inotifyFd, QSocketNotifier::Read, this);
        connect(m_inotifyNotifier, &QSocketNotifier::activated, this, &FilesBridge::onInotifyEvent);
        watchDirectory(m_currentPath);
    }

    // Phase 4: Initialize Tab 0
    TabInfo initialTab;
    initialTab.id = ++m_nextTabId;
    initialTab.currentPath = m_currentPath;
    initialTab.title = currentDirName();
    initialTab.history = m_history;
    initialTab.historyIndex = m_historyIndex;
    initialTab.viewMode = m_viewMode;
    initialTab.selectedPaths = m_selectedPaths;
    initialTab.sortColumn = m_sortColumn;
    initialTab.sortAscending = m_sortAscending;
    m_tabs.push_back(initialTab);
    m_activeTabIndex = 0;
}

FilesBridge::~FilesBridge() {
    unwatchDirectory();
    if (m_inotifyFd >= 0) {
        ::close(m_inotifyFd);
        m_inotifyFd = -1;
    }
    clearSearch();
    cancelCurrentOperation();
}

QString FilesBridge::currentDirName() const {
    if (m_currentPath == QStringLiteral("/")) return QStringLiteral("Root");
    return QFileInfo(m_currentPath).fileName();
}

QString FilesBridge::homePath() const {
    const char* h = getenv("HOME");
    if (h && QDir(QString::fromUtf8(h)).exists()) {
        return QString::fromUtf8(h);
    }
    return QDir::homePath();
}

QVariantList FilesBridge::sidebarLocations() const {
    QVariantList locations;
    QString home = homePath();

    auto addLoc = [&](const QString& label, const QString& path, const QString& iconKind, bool isHeader) {
        QVariantMap m;
        m[QStringLiteral("label")] = label;
        m[QStringLiteral("path")] = path;
        m[QStringLiteral("iconKind")] = iconKind;
        m[QStringLiteral("isHeader")] = isHeader;
        locations.append(m);
    };

    // FAVORITES
    addLoc(QStringLiteral("FAVORITES"), QString(), QString(), true);
    addLoc(QStringLiteral("Home"), home, QStringLiteral("folder"), false);
    addLoc(QStringLiteral("Desktop"), home + QStringLiteral("/Desktop"), QStringLiteral("folder"), false);
    addLoc(QStringLiteral("Documents"), home + QStringLiteral("/Documents"), QStringLiteral("folder"), false);
    addLoc(QStringLiteral("Downloads"), home + QStringLiteral("/Downloads"), QStringLiteral("folder"), false);
    addLoc(QStringLiteral("Pictures"), home + QStringLiteral("/Pictures"), QStringLiteral("image"), false);
    addLoc(QStringLiteral("Videos"), home + QStringLiteral("/Videos"), QStringLiteral("video"), false);
    addLoc(QStringLiteral("Apps"), QStringLiteral("/opt/tinexus-apps"), QStringLiteral("code"), false);

    // LOCATIONS
    addLoc(QStringLiteral("LOCATIONS"), QString(), QString(), true);
    addLoc(QStringLiteral("File System"), QStringLiteral("/"), QStringLiteral("folder"), false);

    for (const auto& rootLoc : {QStringLiteral("/media"), QStringLiteral("/mnt")}) {
        QDir rootDir(rootLoc);
        if (rootDir.exists()) {
            const auto list = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto& d : list) {
                addLoc(d.fileName(), d.absoluteFilePath(), QStringLiteral("archive"), false);
            }
        }
    }

    // TRASH
    addLoc(QStringLiteral("TRASH"), QString(), QString(), true);
    addLoc(QStringLiteral("Trash"), home + QStringLiteral("/.local/share/Trash/files"), QStringLiteral("file"), false);

    return locations;
}

void FilesBridge::rebuildColumnsData() {
    m_columnsData.clear();
    QVariantMap rootCol;
    rootCol[QStringLiteral("columnIndex")] = 0;
    rootCol[QStringLiteral("path")] = m_currentPath;
    rootCol[QStringLiteral("dirName")] = m_isSearching ? QStringLiteral("Search Results") : currentDirName();
    rootCol[QStringLiteral("selectedIndex")] = -1;
    rootCol[QStringLiteral("items")] = fileList();
    m_columnsData.append(rootCol);
    m_columnsVersion++;
    emit columnsDataChanged();
}

int FilesBridge::getColumnSelectedIndex(int colIdx) const {
    if (colIdx >= 0 && colIdx < m_columnsData.size()) {
        return m_columnsData[colIdx].toMap().value(QStringLiteral("selectedIndex"), -1).toInt();
    }
    return -1;
}

void FilesBridge::selectColumnItem(int colIdx, int itemIdx) {
    if (colIdx < 0 || colIdx >= m_columnsData.size()) return;

    QVariantList newCols = m_columnsData;
    QVariantMap col = newCols[colIdx].toMap();
    QVariantList items = col[QStringLiteral("items")].toList();
    if (itemIdx < 0 || itemIdx >= items.size()) return;

    col[QStringLiteral("selectedIndex")] = itemIdx;
    newCols[colIdx] = col;

    // Pop any columns to the right of colIdx
    while (newCols.size() > colIdx + 1) {
        newCols.removeLast();
    }

    QVariantMap item = items[itemIdx].toMap();
    QString targetPath = item[QStringLiteral("path")].toString();
    bool isDir = item[QStringLiteral("isDir")].toBool();

    selectItem(targetPath);

    if (isDir) {
        QVariantList subItems = getFolderContents(targetPath);
        QVariantMap nextCol;
        nextCol[QStringLiteral("columnIndex")] = colIdx + 1;
        nextCol[QStringLiteral("path")] = targetPath;
        nextCol[QStringLiteral("dirName")] = item[QStringLiteral("name")].toString();
        nextCol[QStringLiteral("selectedIndex")] = -1;
        nextCol[QStringLiteral("items")] = subItems;
        newCols.append(nextCol);
    }

    m_columnsData = newCols;
    m_columnsVersion++;
    emit columnsDataChanged();
}

void FilesBridge::openColumnItem(int colIdx, int itemIdx) {
    if (colIdx < 0 || colIdx >= m_columnsData.size()) return;
    QVariantMap col = m_columnsData[colIdx].toMap();
    QVariantList items = col[QStringLiteral("items")].toList();
    if (itemIdx < 0 || itemIdx >= items.size()) return;
    QVariantMap item = items[itemIdx].toMap();
    openItem(item[QStringLiteral("path")].toString(), item[QStringLiteral("isDir")].toBool());
}

void FilesBridge::setViewMode(int mode) {
    if (m_viewMode != mode) {
        m_viewMode = mode;
        if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
            m_tabs[static_cast<size_t>(m_activeTabIndex)].viewMode = m_viewMode;
            emit tabsChanged();
        }
        if (m_viewMode == 2) {
            rebuildColumnsData();
        }
        emit viewModeChanged();
    }
}

void FilesBridge::selectItem(const QString& path) {
    if (path.isEmpty()) {
        clearSelection();
        return;
    }
    if (m_selectedPaths.size() == 1 && m_selectedPaths.first() == path) return;
    m_selectedPaths = QStringList{path};
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        m_tabs[static_cast<size_t>(m_activeTabIndex)].selectedPaths = m_selectedPaths;
    }
    emit selectedPathsChanged();
    emit selectedPathChanged();
}

void FilesBridge::setSelectedPaths(const QStringList& paths) {
    if (m_selectedPaths != paths) {
        m_selectedPaths = paths;
        if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
            m_tabs[static_cast<size_t>(m_activeTabIndex)].selectedPaths = m_selectedPaths;
        }
        emit selectedPathsChanged();
        emit selectedPathChanged();
    }
}

void FilesBridge::toggleSelectItem(const QString& path) {
    if (path.isEmpty()) return;
    if (m_selectedPaths.contains(path)) {
        m_selectedPaths.removeAll(path);
    } else {
        m_selectedPaths.append(path);
    }
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        m_tabs[static_cast<size_t>(m_activeTabIndex)].selectedPaths = m_selectedPaths;
    }
    emit selectedPathsChanged();
    emit selectedPathChanged();
}

void FilesBridge::selectRange(const QString& targetPath) {
    if (targetPath.isEmpty()) return;
    const auto& items = fileList();
    if (items.isEmpty()) return;

    int targetIdx = -1;
    int anchorIdx = -1;
    QString anchorPath = m_selectedPaths.isEmpty() ? QString() : m_selectedPaths.last();

    for (int i = 0; i < items.size(); ++i) {
        QString p = items[i].toMap()[QStringLiteral("path")].toString();
        if (p == targetPath) targetIdx = i;
        if (!anchorPath.isEmpty() && p == anchorPath) anchorIdx = i;
    }

    if (targetIdx == -1) return;
    if (anchorIdx == -1) anchorIdx = 0;

    int start = std::min(anchorIdx, targetIdx);
    int end = std::max(anchorIdx, targetIdx);

    m_selectedPaths.clear();
    for (int i = start; i <= end; ++i) {
        m_selectedPaths.append(items[i].toMap()[QStringLiteral("path")].toString());
    }
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        m_tabs[static_cast<size_t>(m_activeTabIndex)].selectedPaths = m_selectedPaths;
    }
    emit selectedPathsChanged();
    emit selectedPathChanged();
}

void FilesBridge::selectAll() {
    m_selectedPaths.clear();
    for (const auto& item : fileList()) {
        m_selectedPaths.append(item.toMap()[QStringLiteral("path")].toString());
    }
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        m_tabs[static_cast<size_t>(m_activeTabIndex)].selectedPaths = m_selectedPaths;
    }
    emit selectedPathsChanged();
    emit selectedPathChanged();
}

void FilesBridge::clearSelection() {
    if (!m_selectedPaths.isEmpty()) {
        m_selectedPaths.clear();
        if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
            m_tabs[static_cast<size_t>(m_activeTabIndex)].selectedPaths = m_selectedPaths;
        }
        emit selectedPathsChanged();
        emit selectedPathChanged();
    }
}

bool FilesBridge::isSelected(const QString& path) const {
    return m_selectedPaths.contains(path);
}

QVariantMap FilesBridge::selectedItemDetails() const {
    if (m_selectedPaths.isEmpty()) return QVariantMap();
    return const_cast<FilesBridge*>(this)->getFileDetails(m_selectedPaths.first());
}

void FilesBridge::setTagOnSelected(const QString& tagName) {
    for (const auto& p : m_selectedPaths) {
        setTagOnItem(p, tagName);
    }
}

void FilesBridge::loadDirectory(const QString& path, bool addToHistory) {
    if (m_isSearching) {
        clearSearch();
    }
    QDir dir(path);
    if (!dir.exists()) {
        QDir().mkpath(path);
    }
    if (!dir.exists()) {
        tinexus::log::warn("[files] Path does not exist and could not be created: {}", path.toStdString());
        return;
    }

    QString canonical = dir.canonicalPath();
    if (canonical.isEmpty()) canonical = path;

    m_currentPath = canonical;

    if (addToHistory) {
        if (m_historyIndex >= 0 && m_historyIndex + 1 < static_cast<int>(m_history.size())) {
            m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());
        }
        m_history.push_back(canonical);
        m_historyIndex = static_cast<int>(m_history.size()) - 1;
        emit navigationChanged();
    }

    m_fileList.clear();

    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const auto& info : entries) {
        QVariantMap item;
        item[QStringLiteral("name")] = info.fileName();
        item[QStringLiteral("path")] = info.absoluteFilePath();
        item[QStringLiteral("isDir")] = info.isDir();
        item[QStringLiteral("isSymLink")] = info.isSymLink();
        item[QStringLiteral("isHidden")] = info.isHidden();

        if (info.isDir()) {
            QDir subDir(info.absoluteFilePath());
            int count = static_cast<int>(subDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count());
            item[QStringLiteral("sizeStr")] = QStringLiteral("%1 items").arg(count);
        } else {
            qint64 bytes = info.size();
            if (bytes < 1024) {
                item[QStringLiteral("sizeStr")] = QStringLiteral("%1 B").arg(bytes);
            } else if (bytes < 1024 * 1024) {
                item[QStringLiteral("sizeStr")] = QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 1);
            } else if (bytes < 1024 * 1024 * 1024) {
                item[QStringLiteral("sizeStr")] = QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
            } else {
                item[QStringLiteral("sizeStr")] = QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
            }
        }

        item[QStringLiteral("modified")] = info.lastModified().toString(QStringLiteral("MMM d, yyyy h:mm AP"));

        // Icon hint based on extension
        QString ext = info.suffix().toLower();
        QString iconKind = QStringLiteral("file");
        if (info.isDir()) {
            iconKind = QStringLiteral("folder");
        } else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "svg" || ext == "bmp") {
            iconKind = QStringLiteral("image");
        } else if (ext == "mp4" || ext == "mkv" || ext == "mov" || ext == "avi") {
            iconKind = QStringLiteral("video");
        } else if (ext == "mp3" || ext == "wav" || ext == "flac" || ext == "ogg") {
            iconKind = QStringLiteral("audio");
        } else if (ext == "pdf") {
            iconKind = QStringLiteral("pdf");
        } else if (ext == "txt" || ext == "md" || ext == "log" || ext == "cpp" || ext == "hpp" || ext == "py" || ext == "sh" || ext == "json") {
            iconKind = QStringLiteral("code");
        } else if (ext == "zip" || ext == "tar" || ext == "gz" || ext == "xz" || ext == "7z") {
            iconKind = QStringLiteral("archive");
        }
        item[QStringLiteral("iconKind")] = iconKind;

        // Phase 1: embed current tag name so model is self-contained after restart
        {
            try {
                auto tag = tinexus::files::TagManager::instance().get_tag(
                    std::filesystem::path(info.absoluteFilePath().toStdString()));
                item[QStringLiteral("tag")] = tag.has_value()
                    ? QString::fromStdString(tag.value())
                    : QString();
            } catch (...) {
                item[QStringLiteral("tag")] = QString();
            }
        }

        m_fileList.append(item);
    }

    if (!m_fileList.isEmpty() && m_selectedPaths.isEmpty()) {
        m_selectedPaths = QStringList{m_fileList.first().toMap()[QStringLiteral("path")].toString()};
        emit selectedPathsChanged();
        emit selectedPathChanged();
    }

    // ── Phase 1: Apply current sort state instead of always Name-ASC ──
    // Convert QVariantList to FileItem vector, sort, convert back
    {
        using tinexus::files::FileModel;
        using tinexus::files::SortCriteria;
        using tinexus::files::SortDirection;
        using tinexus::files::FileItem;
        using tinexus::files::FileType;

        // Build a lightweight key vector for sorting (avoids re-stating files)
        struct SortKey {
            int original_index;
            QString name;
            qint64 size_bytes;
            QDateTime modified;
            QString mime_kind; // iconKind used as Kind column proxy
        };

        QVariantList sorted = m_fileList;
        std::stable_sort(sorted.begin(), sorted.end(),
            [this](const QVariant& a, const QVariant& b) -> bool {
                const QVariantMap ma = a.toMap();
                const QVariantMap mb = b.toMap();

                // Dirs always first, regardless of sort column
                bool aDir = ma[QStringLiteral("isDir")].toBool();
                bool bDir = mb[QStringLiteral("isDir")].toBool();
                if (aDir != bDir) return aDir;

                bool result = false;
                if (m_sortColumn == QStringLiteral("name")) {
                    result = ma[QStringLiteral("name")].toString().compare(
                                 mb[QStringLiteral("name")].toString(),
                                 Qt::CaseInsensitive) < 0;
                } else if (m_sortColumn == QStringLiteral("modified")) {
                    result = ma[QStringLiteral("modified")].toString() <
                             mb[QStringLiteral("modified")].toString();
                } else if (m_sortColumn == QStringLiteral("size")) {
                    // sizeStr is human-readable; compare raw bytes via QFileInfo
                    // We stored sizeStr not bytes, so fall back to lexicographic
                    // on the sizeStr. For a proper fix, store size_bytes in the map.
                    result = ma[QStringLiteral("sizeStr")].toString() <
                             mb[QStringLiteral("sizeStr")].toString();
                } else if (m_sortColumn == QStringLiteral("kind")) {
                    const QString& ika = ma[QStringLiteral("iconKind")].toString();
                    const QString& ikb = mb[QStringLiteral("iconKind")].toString();
                    if (ika != ikb) result = ika < ikb;
                    else result = ma[QStringLiteral("name")].toString().compare(
                                      mb[QStringLiteral("name")].toString(),
                                      Qt::CaseInsensitive) < 0;
                }

                return m_sortAscending ? result : !result;
            });
        m_fileList = sorted;
    }

    emit currentPathChanged();
    emit fileListChanged();
    rebuildColumnsData();

    // Phase 6: Ensure active directory is watched by inotify
    watchDirectory(m_currentPath);

    // Phase 4: Sync active tab state
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        auto& active = m_tabs[static_cast<size_t>(m_activeTabIndex)];
        active.currentPath = m_currentPath;
        active.title = currentDirName();
        active.history = m_history;
        active.historyIndex = m_historyIndex;
        active.selectedPaths = m_selectedPaths;
        emit tabsChanged();
    }
}

void FilesBridge::cd(const QString& path) {
    loadDirectory(path, true);
}

void FilesBridge::goUp() {
    QDir dir(m_currentPath);
    if (dir.cdUp()) {
        loadDirectory(dir.absolutePath(), true);
    }
}

void FilesBridge::goBack() {
    if (canGoBack()) {
        m_historyIndex--;
        emit navigationChanged();
        loadDirectory(m_history[static_cast<size_t>(m_historyIndex)], false);
    }
}

void FilesBridge::goForward() {
    if (canGoForward()) {
        m_historyIndex++;
        emit navigationChanged();
        loadDirectory(m_history[static_cast<size_t>(m_historyIndex)], false);
    }
}

void FilesBridge::openItem(const QString& path, bool isDir) {
    if (isDir) {
        cd(path);
    } else {
        tinexus::log::info("[files] Opening file: {}", path.toStdString());
        if (!QProcess::startDetached(QStringLiteral("xdg-open"), {path})) {
            QProcess::startDetached(QStringLiteral("gio"), {QStringLiteral("open"), path});
        }
    }
}

void FilesBridge::refresh() {
    loadDirectory(m_currentPath, false);
}

bool FilesBridge::createNewFolder(const QString& folderName) {
    if (folderName.trimmed().isEmpty()) return false;
    QDir dir(m_currentPath);
    bool ok = dir.mkdir(folderName.trimmed());
    if (ok) refresh();
    return ok;
}

bool FilesBridge::createNewFile(const QString& fileName) {
    if (fileName.trimmed().isEmpty()) return false;
    QString target = m_currentPath + "/" + fileName.trimmed();
    QFile file(target);
    if (file.open(QIODevice::WriteOnly)) {
        file.close();
        refresh();
        return true;
    }
    return false;
}

bool FilesBridge::renameItem(const QString& oldPath, const QString& newName) {
    if (newName.trimmed().isEmpty()) return false;
    QFileInfo fi(oldPath);
    if (!fi.exists()) return false;
    QString target = fi.dir().absoluteFilePath(newName.trimmed());
    bool ok = QFile::rename(oldPath, target);
    if (ok) {
        int idx = m_selectedPaths.indexOf(oldPath);
        if (idx >= 0) {
            m_selectedPaths[idx] = target;
            if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
                m_tabs[static_cast<size_t>(m_activeTabIndex)].selectedPaths = m_selectedPaths;
            }
            emit selectedPathsChanged();
            emit selectedPathChanged();
        }
        refresh();
    }
    return ok;
}

bool FilesBridge::deleteItem(const QString& path) {
    if (path.isEmpty()) return false;
    auto res = FileOperations::trash_path(path.toStdString());
    if (res.success) {
        if (m_selectedPaths.contains(path)) {
            m_selectedPaths.removeAll(path);
            if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
                m_tabs[static_cast<size_t>(m_activeTabIndex)].selectedPaths = m_selectedPaths;
            }
            emit selectedPathsChanged();
            emit selectedPathChanged();
        }
        refresh();
        return true;
    }
    tinexus::log::warn("[files] Failed to trash item: {}", res.error_message);
    return false;
}

bool FilesBridge::deleteSelected() {
    if (m_selectedPaths.isEmpty()) return false;
    QStringList toDelete = m_selectedPaths;
    bool allOk = true;
    for (const auto& p : toDelete) {
        auto res = FileOperations::trash_path(p.toStdString());
        if (!res.success) {
            allOk = false;
            tinexus::log::warn("[files] Failed to trash '{}': {}", p.toStdString(), res.error_message);
        }
    }
    clearSelection();
    refresh();
    return allOk;
}

bool FilesBridge::moveItem(const QString& srcPath, const QString& destDir) {
    if (srcPath.isEmpty() || destDir.isEmpty()) return false;
    auto res = FileOperations::move_path(srcPath.toStdString(), destDir.toStdString());
    if (res.success) {
        refresh();
        return true;
    }
    tinexus::log::warn("[files] Failed to move item: {}", res.error_message);
    return false;
}

void FilesBridge::copyItem(const QString& path) {
    if (path.isEmpty()) return;
    copyItems(QStringList{path});
}

void FilesBridge::copyItems(const QStringList& paths) {
    m_clipboardPaths = paths;
    m_clipboardIsCut = false;
    emit clipboardChanged();
}

void FilesBridge::cutItem(const QString& path) {
    if (path.isEmpty()) return;
    cutItems(QStringList{path});
}

void FilesBridge::cutItems(const QStringList& paths) {
    m_clipboardPaths = paths;
    m_clipboardIsCut = true;
    emit clipboardChanged();
}

void FilesBridge::copySelected() {
    if (!m_selectedPaths.isEmpty()) {
        copyItems(m_selectedPaths);
    }
}

void FilesBridge::cutSelected() {
    if (!m_selectedPaths.isEmpty()) {
        cutItems(m_selectedPaths);
    }
}

void FilesBridge::setApplyToAll(bool val) {
    if (m_applyToAll != val) {
        m_applyToAll = val;
        emit applyToAllChanged();
    }
}

void FilesBridge::resolveConflict(int resolution, bool applyToAll) {
    {
        std::lock_guard<std::mutex> lock(m_conflictMutex);
        m_chosenResolution = static_cast<ConflictResolution>(resolution);
        if (applyToAll) {
            if (m_chosenResolution == ConflictResolution::Skip) {
                m_batchConflictPolicy = BatchConflictPolicy::SkipAll;
            } else if (m_chosenResolution == ConflictResolution::Replace) {
                m_batchConflictPolicy = BatchConflictPolicy::ReplaceAll;
            } else if (m_chosenResolution == ConflictResolution::KeepBoth) {
                m_batchConflictPolicy = BatchConflictPolicy::KeepBothAll;
            }
        }
        m_conflictResolved = true;
        m_conflictCv.notify_all();
    }
    m_conflictDialogVisible = false;
    emit conflictDialogVisibleChanged();
}

void FilesBridge::setBatchConflictPolicy(int policy) {
    std::lock_guard<std::mutex> lock(m_conflictMutex);
    m_batchConflictPolicy = static_cast<BatchConflictPolicy>(policy);
}

void FilesBridge::cancelCurrentOperation() {
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        {
            std::lock_guard<std::mutex> lock(m_conflictMutex);
            m_conflictResolved = true;
            m_chosenResolution = ConflictResolution::Abort;
            m_conflictCv.notify_all();
        }
        m_workerThread.join(); // Cooperative wait until thread exits and partial files are removed
    }
    if (m_isOperating) {
        m_isOperating = false;
        m_operationProgress = 0.0;
        m_bytesTransferred = 0;
        m_totalBytes = 0;
        m_operationSpeedStr.clear();
        m_conflictDialogVisible = false;
        emit operationStateChanged();
        emit operationProgressChanged();
        emit conflictDialogVisibleChanged();
    }
}

bool FilesBridge::pasteItem(const QString& targetDir) {
    if (m_clipboardPaths.isEmpty()) return false;
    if (m_isOperating) {
        tinexus::log::warn("[files] Cannot paste: another operation is in progress");
        return false;
    }

    QString destDir = targetDir.isEmpty() ? m_currentPath : targetDir;
    std::vector<std::filesystem::path> srcPaths;
    for (const auto& cp : m_clipboardPaths) {
        QFileInfo srcInfo(cp);
        if (srcInfo.exists()) {
            srcPaths.push_back(srcInfo.absoluteFilePath().toStdString());
        }
    }
    if (srcPaths.empty()) return false;

    const std::filesystem::path destDirFs = destDir.toStdString();
    const bool isCut = m_clipboardIsCut;

    {
        std::lock_guard<std::mutex> lock(m_conflictMutex);
        m_batchConflictPolicy = BatchConflictPolicy::AskEach;
        m_conflictResolved = false;
        m_conflictDialogVisible = false;
        m_applyToAll = false;
    }

    m_isOperating = true;
    m_operationProgress = 0.0;
    m_bytesTransferred = 0;
    m_totalBytes = 0;
    m_operationSpeedStr.clear();
    m_currentOperationName = isCut
        ? (srcPaths.size() > 1 ? QStringLiteral("Moving %1 items...").arg(srcPaths.size()) : QStringLiteral("Moving item..."))
        : (srcPaths.size() > 1 ? QStringLiteral("Copying %1 items...").arg(srcPaths.size()) : QStringLiteral("Copying item..."));
    emit operationStateChanged();
    emit operationProgressChanged();

    if (isCut) {
        m_clipboardPaths.clear();
        m_clipboardIsCut = false;
        emit clipboardChanged();
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    // Launch background worker thread with cooperative std::stop_token
    m_workerThread = std::jthread([this, srcPaths, destDirFs, isCut](std::stop_token st) {
        auto progress_cb = [this](const CopyProgress& p) {
            // Marshalling to main UI thread
            QMetaObject::invokeMethod(this, [this, p]() {
                m_bytesTransferred = static_cast<qint64>(p.bytes_transferred);
                m_totalBytes = static_cast<qint64>(p.total_bytes);
                m_operationProgress = p.progress_fraction;

                if (p.bytes_per_sec >= 1024.0 * 1024.0 * 1024.0) {
                    m_operationSpeedStr = QStringLiteral("%1 GB/s").arg(p.bytes_per_sec / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
                } else if (p.bytes_per_sec >= 1024.0 * 1024.0) {
                    m_operationSpeedStr = QStringLiteral("%1 MB/s").arg(p.bytes_per_sec / (1024.0 * 1024.0), 0, 'f', 1);
                } else if (p.bytes_per_sec >= 1024.0) {
                    m_operationSpeedStr = QStringLiteral("%1 KB/s").arg(p.bytes_per_sec / 1024.0, 0, 'f', 1);
                } else {
                    m_operationSpeedStr = QStringLiteral("%1 B/s").arg(p.bytes_per_sec, 0, 'f', 0);
                }
                emit operationProgressChanged();
            }, Qt::QueuedConnection);
        };

        auto conflict_cb = [this](const ConflictInfo& info, std::stop_token token) -> ConflictResolution {
            std::unique_lock<std::mutex> lock(m_conflictMutex);

            // Fast-path for batch policy
            if (m_batchConflictPolicy == BatchConflictPolicy::SkipAll) {
                return ConflictResolution::Skip;
            }
            if (m_batchConflictPolicy == BatchConflictPolicy::ReplaceAll) {
                return ConflictResolution::Replace;
            }
            if (m_batchConflictPolicy == BatchConflictPolicy::KeepBothAll) {
                return ConflictResolution::KeepBoth;
            }

            m_conflictResolved = false;

            auto formatSize = [](uint64_t bytes) -> QString {
                if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
                if (bytes < 1024 * 1024) return QStringLiteral("%1 KB").arg(double(bytes) / 1024.0, 0, 'f', 1);
                if (bytes < 1024 * 1024 * 1024) return QStringLiteral("%1 MB").arg(double(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
                return QStringLiteral("%1 GB").arg(double(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
            };

            auto formatTime = [](int64_t sec) -> QString {
                if (sec <= 0) return QStringLiteral("Unknown");
                QDateTime dt = QDateTime::fromSecsSinceEpoch(sec);
                return dt.toString(QStringLiteral("MMM d, yyyy h:mm AP"));
            };

            // Marshal to main GUI thread: zero direct QML mutation from worker thread
            QMetaObject::invokeMethod(this, [this, info, formatSize, formatTime]() {
                QVariantMap map;
                map[QStringLiteral("sourcePath")] = QString::fromStdString(info.source_path.string());
                map[QStringLiteral("destPath")] = QString::fromStdString(info.dest_path.string());
                map[QStringLiteral("fileName")] = QString::fromStdString(info.source_path.filename().string());
                map[QStringLiteral("isDir")] = info.is_directory;
                map[QStringLiteral("sourceSize")] = static_cast<qint64>(info.source_size);
                map[QStringLiteral("destSize")] = static_cast<qint64>(info.dest_size);
                map[QStringLiteral("sourceSizeStr")] = formatSize(info.source_size);
                map[QStringLiteral("destSizeStr")] = formatSize(info.dest_size);
                map[QStringLiteral("sourceMtimeStr")] = formatTime(info.source_mtime);
                map[QStringLiteral("destMtimeStr")] = formatTime(info.dest_mtime);
                map[QStringLiteral("isTypeMismatch")] = info.is_type_mismatch;
                map[QStringLiteral("destDirItemCount")] = static_cast<int>(info.dest_dir_item_count);
                map[QStringLiteral("destDirTotalSizeStr")] = formatSize(info.dest_dir_total_size);

                m_conflictDetails = map;
                m_conflictDialogVisible = true;
                emit conflictDetailsChanged();
                emit conflictDialogVisibleChanged();
            }, Qt::QueuedConnection);

            // Wait with C++20 stop_token awareness: unblocks immediately if operation is cancelled
            m_conflictCv.wait(lock, token, [this] {
                return m_conflictResolved;
            });

            if (token.stop_requested()) {
                return ConflictResolution::Abort;
            }

            return m_chosenResolution;
        };

        bool allSucceeded = true;
        std::string lastError;

        for (size_t i = 0; i < srcPaths.size(); ++i) {
            if (st.stop_requested()) {
                allSucceeded = false;
                break;
            }
            const auto& srcFs = srcPaths[i];
            OperationResult res;
            if (isCut) {
                res = FileOperations::move_path(srcFs, destDirFs, st, progress_cb, conflict_cb);
            } else {
                res = FileOperations::copy_path_streaming(srcFs, destDirFs, st, progress_cb, conflict_cb);
            }
            if (!res.success) {
                allSucceeded = false;
                lastError = res.error_message;
            }
        }

        // Completion dispatched to main UI thread
        QMetaObject::invokeMethod(this, [this, allSucceeded, lastError]() {
            m_isOperating = false;
            m_conflictDialogVisible = false;
            {
                std::lock_guard<std::mutex> lock(m_conflictMutex);
                m_batchConflictPolicy = BatchConflictPolicy::AskEach;
            }
            emit operationStateChanged();
            emit conflictDialogVisibleChanged();
            emit operationCompleted(allSucceeded, QString::fromStdString(lastError));
            refresh();
        }, Qt::QueuedConnection);
    });

    return true;
}

QVariantList FilesBridge::getFolderContents(const QString& folderPath) {
    QVariantList list;
    QDir dir(folderPath);
    if (!dir.exists()) return list;

    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    for (const auto& info : entries) {
        QVariantMap item;
        item[QStringLiteral("name")] = info.fileName();
        item[QStringLiteral("path")] = info.absoluteFilePath();
        item[QStringLiteral("isDir")] = info.isDir();
        QString ext = info.suffix().toLower();
        QString iconKind = info.isDir() ? QStringLiteral("folder") :
                           ((ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp") ? QStringLiteral("image") : QStringLiteral("file"));
        item[QStringLiteral("iconKind")] = iconKind;
        list.append(item);
    }
    return list;
}

QVariantMap FilesBridge::getFileDetails(const QString& path) {
    QVariantMap map;
    QFileInfo fi(path);
    if (!fi.exists()) {
        map[QStringLiteral("name")] = QStringLiteral("No selection");
        map[QStringLiteral("isDir")] = false;
        map[QStringLiteral("kind")] = QStringLiteral("--");
        map[QStringLiteral("sizeStr")] = QStringLiteral("--");
        map[QStringLiteral("modified")] = QStringLiteral("--");
        return map;
    }

    map[QStringLiteral("name")] = fi.fileName();
    map[QStringLiteral("path")] = fi.absoluteFilePath();
    map[QStringLiteral("isDir")] = fi.isDir();
    map[QStringLiteral("extension")] = fi.suffix().toUpper();

    QString ext = fi.suffix().toLower();
    QString kind = QStringLiteral("Document");
    QString iconKind = QStringLiteral("file");

    if (fi.isDir()) {
        kind = QStringLiteral("Folder");
        iconKind = QStringLiteral("folder");
        QDir dir(path);
        int count = static_cast<int>(dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count());
        map[QStringLiteral("sizeStr")] = QStringLiteral("%1 items").arg(count);
    } else {
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "svg") {
            kind = QStringLiteral("Image (%1)").arg(ext.toUpper());
            iconKind = QStringLiteral("image");
        } else if (ext == "mp4" || ext == "mkv" || ext == "mov") {
            kind = QStringLiteral("Video");
            iconKind = QStringLiteral("video");
        } else if (ext == "mp3" || ext == "wav" || ext == "flac") {
            kind = QStringLiteral("Audio");
            iconKind = QStringLiteral("audio");
        } else if (ext == "pdf") {
            kind = QStringLiteral("PDF Document");
            iconKind = QStringLiteral("pdf");
        } else if (ext == "cpp" || ext == "hpp" || ext == "py" || ext == "sh" || ext == "json") {
            kind = QStringLiteral("Source Code");
            iconKind = QStringLiteral("code");
        } else if (ext == "zip" || ext == "tar" || ext == "gz") {
            kind = QStringLiteral("Archive");
            iconKind = QStringLiteral("archive");
        }

        qint64 bytes = fi.size();
        if (bytes < 1024) map[QStringLiteral("sizeStr")] = QStringLiteral("%1 B").arg(bytes);
        else if (bytes < 1024 * 1024) map[QStringLiteral("sizeStr")] = QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 1);
        else if (bytes < 1024 * 1024 * 1024) map[QStringLiteral("sizeStr")] = QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
        else map[QStringLiteral("sizeStr")] = QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }

    map[QStringLiteral("kind")] = kind;
    map[QStringLiteral("iconKind")] = iconKind;
    map[QStringLiteral("modified")] = fi.lastModified().toString(QStringLiteral("MMM d, yyyy h:mm AP"));
    map[QStringLiteral("created")] = fi.birthTime().isValid() ? fi.birthTime().toString(QStringLiteral("MMM d, yyyy h:mm AP")) : fi.lastModified().toString(QStringLiteral("MMM d, yyyy h:mm AP"));
    map[QStringLiteral("owner")] = fi.owner().isEmpty() ? QStringLiteral("user") : fi.owner();
    map[QStringLiteral("group")] = fi.group().isEmpty() ? QStringLiteral("user") : fi.group();
    map[QStringLiteral("fullPath")] = fi.canonicalFilePath().isEmpty() ? fi.absoluteFilePath() : fi.canonicalFilePath();

    QFile::Permissions p = fi.permissions();
    QString permStr;
    permStr += (fi.isDir() ? "d" : "-");
    permStr += (p & QFile::ReadOwner) ? "r" : "-";
    permStr += (p & QFile::WriteOwner) ? "w" : "-";
    permStr += (p & QFile::ExeOwner) ? "x" : "-";
    permStr += (p & QFile::ReadGroup) ? "r" : "-";
    permStr += (p & QFile::WriteGroup) ? "w" : "-";
    permStr += (p & QFile::ExeGroup) ? "x" : "-";
    permStr += (p & QFile::ReadOther) ? "r" : "-";
    permStr += (p & QFile::WriteOther) ? "w" : "-";
    permStr += (p & QFile::ExeOther) ? "x" : "-";
    map[QStringLiteral("permissions")] = permStr;

    return map;
}

} // namespace tinexus::files

// ============================================================================
// Phase 1 additions
// ============================================================================
namespace tinexus::files {

void FilesBridge::sortBy(const QString& column, bool ascending) {
    const QString col = column.toLower();
    // QML caller already computes the correct ascending value:
    //   bridge.sortColumn !== col ? true : !bridge.sortAscending
    // So we just apply what we're told directly.
    m_sortColumn    = col;
    m_sortAscending = ascending;
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        auto& active = m_tabs[static_cast<size_t>(m_activeTabIndex)];
        active.sortColumn = m_sortColumn;
        active.sortAscending = m_sortAscending;
    }

    tinexus::log::info("[files] sortBy column='{}' ascending={}",
                       col.toStdString(), ascending);

    // Re-sort in-place without hitting the filesystem again
    std::stable_sort(m_fileList.begin(), m_fileList.end(),
        [this](const QVariant& a, const QVariant& b) -> bool {
            const QVariantMap ma = a.toMap();
            const QVariantMap mb = b.toMap();

            bool aDir = ma[QStringLiteral("isDir")].toBool();
            bool bDir = mb[QStringLiteral("isDir")].toBool();
            if (aDir != bDir) return aDir;

            bool result = false;
            if (m_sortColumn == QStringLiteral("name")) {
                result = ma[QStringLiteral("name")].toString().compare(
                             mb[QStringLiteral("name")].toString(),
                             Qt::CaseInsensitive) < 0;
            } else if (m_sortColumn == QStringLiteral("modified")) {
                result = ma[QStringLiteral("modified")].toString() <
                         mb[QStringLiteral("modified")].toString();
            } else if (m_sortColumn == QStringLiteral("size")) {
                result = ma[QStringLiteral("sizeStr")].toString() <
                         mb[QStringLiteral("sizeStr")].toString();
            } else if (m_sortColumn == QStringLiteral("kind")) {
                const QString& ika = ma[QStringLiteral("iconKind")].toString();
                const QString& ikb = mb[QStringLiteral("iconKind")].toString();
                if (ika != ikb) result = ika < ikb;
                else result = ma[QStringLiteral("name")].toString().compare(
                                  mb[QStringLiteral("name")].toString(),
                                  Qt::CaseInsensitive) < 0;
            }
            return m_sortAscending ? result : !result;
        });

    emit sortChanged();
    emit fileListChanged();
    if (m_viewMode == 2) rebuildColumnsData();
}

void FilesBridge::setTagOnItem(const QString& path, const QString& tagName) {
    if (path.isEmpty()) return;

    try {
        if (tagName.isEmpty()) {
            TagManager::instance().remove_tag(
                std::filesystem::path(path.toStdString()));
            tinexus::log::info("[files] Removed tag from '{}'", path.toStdString());
        } else {
            TagManager::instance().set_tag(
                std::filesystem::path(path.toStdString()),
                tagName.toStdString());
            tinexus::log::info("[files] Set tag '{}' on '{}'",
                               tagName.toStdString(), path.toStdString());
        }
    } catch (const std::exception& e) {
        tinexus::log::error("[files] setTagOnItem exception: {}", e.what());
    }

    // Patch the tag field in-place inside m_fileList so model is consistent
    // without requiring a full loadDirectory() re-scan.
    for (int i = 0; i < m_fileList.size(); ++i) {
        QVariantMap entry = m_fileList[i].toMap();
        if (entry[QStringLiteral("path")].toString() == path) {
            entry[QStringLiteral("tag")] = tagName;
            m_fileList[i] = entry;
            break;
        }
    }

    if (m_isSearching) {
        for (int i = 0; i < m_searchResults.size(); ++i) {
            QVariantMap entry = m_searchResults[i].toMap();
            if (entry[QStringLiteral("path")].toString() == path) {
                entry[QStringLiteral("tag")] = tagName;
                m_searchResults[i] = entry;
                break;
            }
        }
    }

    // Increment revision BEFORE emitting so QML sees new value in same frame
    ++m_tagRevision;
    emit tagDataChanged();
    emit fileListChanged();
}

QString FilesBridge::getTagForItem(const QString& path) const {
    if (path.isEmpty()) return QString();
    try {
        auto tag = TagManager::instance().get_tag(
            std::filesystem::path(path.toStdString()));
        if (tag.has_value()) {
            return QString::fromStdString(tag.value());
        }
    } catch (...) {}
    return QString();
}

void FilesBridge::setSearchQuery(const QString& q) {
    if (m_searchQuery != q) {
        startSearch(q, true);
    }
}

void FilesBridge::clearSearch() {
    m_searchGeneration++;
    if (m_searchThread.joinable()) {
        m_searchThread.request_stop();
        m_searchThread.join();
    }
    if (m_isSearching || !m_searchResults.isEmpty() || !m_searchQuery.isEmpty()) {
        m_isSearching = false;
        m_searchFinished = true;
        m_searchQuery.clear();
        m_searchResults.clear();
        emit searchStateChanged();
        emit searchQueryChanged();
        emit searchResultsChanged();
        emit searchFinishedChanged();
        emit fileListChanged();
        rebuildColumnsData();
    }
}

void FilesBridge::startSearch(const QString& query, bool recursive) {
    // 1. Cancel and join any ongoing search thread
    m_searchGeneration++;
    if (m_searchThread.joinable()) {
        m_searchThread.request_stop();
        m_searchThread.join();
    }

    QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        clearSearch();
        return;
    }

    uint64_t currentGen = m_searchGeneration;
    m_isSearching = true;
    m_searchFinished = false;
    m_searchQuery = query;
    m_searchResults.clear();
    emit searchStateChanged();
    emit searchQueryChanged();
    emit searchResultsChanged();
    emit searchFinishedChanged();
    emit fileListChanged();
    rebuildColumnsData();

    QString searchDir = m_currentPath;

    // 2. Launch background search worker with cooperative stop_token
    m_searchThread = std::jthread([this, currentGen, trimmed, searchDir, recursive](std::stop_token st) {
        std::filesystem::path searchPath(searchDir.toStdString());
        std::error_code ec;
        if (!std::filesystem::exists(searchPath, ec) || !std::filesystem::is_directory(searchPath, ec)) {
            QMetaObject::invokeMethod(this, [this, currentGen]() {
                if (m_searchGeneration != currentGen) return;
                m_searchFinished = true;
                emit searchFinishedChanged();
                emit searchFinished(0);
            }, Qt::QueuedConnection);
            return;
        }

        QVariantList batch;
        int totalFound = 0;

        auto processEntry = [&](const std::filesystem::directory_entry& entry) {
            if (st.stop_requested()) return false;

            std::string filenameStr = entry.path().filename().string();
            if (filenameStr == "." || filenameStr == "..") return true;

            QString fileName = QString::fromStdString(filenameStr);
            std::filesystem::path rel = std::filesystem::relative(entry.path(), searchPath, ec);
            QString relPathStr = QString::fromStdString(rel.string());

            bool matches = fileName.contains(trimmed, Qt::CaseInsensitive) ||
                           relPathStr.contains(trimmed, Qt::CaseInsensitive);

            if (matches) {
                QVariantMap item;
                item[QStringLiteral("name")] = fileName;
                item[QStringLiteral("relativePath")] = relPathStr;
                item[QStringLiteral("path")] = QString::fromStdString(entry.path().string());
                item[QStringLiteral("isDir")] = entry.is_directory(ec);
                item[QStringLiteral("isSymLink")] = entry.is_symlink(ec);
                item[QStringLiteral("isHidden")] = fileName.startsWith('.');

                if (entry.is_directory(ec)) {
                    int count = 0;
                    std::error_code countEc;
                    for (auto const& sub : std::filesystem::directory_iterator(entry.path(), countEc)) {
                        (void)sub;
                        count++;
                    }
                    item[QStringLiteral("sizeStr")] = QStringLiteral("%1 items").arg(count);
                } else {
                    auto bytes = entry.file_size(ec);
                    if (ec) bytes = 0;
                    if (bytes < 1024) {
                        item[QStringLiteral("sizeStr")] = QStringLiteral("%1 B").arg(bytes);
                    } else if (bytes < 1024 * 1024) {
                        item[QStringLiteral("sizeStr")] = QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 1);
                    } else if (bytes < 1024 * 1024 * 1024) {
                        item[QStringLiteral("sizeStr")] = QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
                    } else {
                        item[QStringLiteral("sizeStr")] = QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
                    }
                }

                // Date modified
                QFileInfo qfi(QString::fromStdString(entry.path().string()));
                item[QStringLiteral("modified")] = qfi.lastModified().toString(QStringLiteral("MMM d, yyyy h:mm AP"));

                // Icon kind
                QString ext = qfi.suffix().toLower();
                QString iconKind = QStringLiteral("file");
                if (entry.is_directory(ec)) {
                    iconKind = QStringLiteral("folder");
                } else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "svg" || ext == "bmp") {
                    iconKind = QStringLiteral("image");
                } else if (ext == "mp4" || ext == "mkv" || ext == "mov" || ext == "avi") {
                    iconKind = QStringLiteral("video");
                } else if (ext == "mp3" || ext == "wav" || ext == "flac" || ext == "ogg") {
                    iconKind = QStringLiteral("audio");
                } else if (ext == "pdf") {
                    iconKind = QStringLiteral("pdf");
                } else if (ext == "txt" || ext == "md" || ext == "log" || ext == "cpp" || ext == "hpp" || ext == "py" || ext == "sh" || ext == "json") {
                    iconKind = QStringLiteral("code");
                } else if (ext == "zip" || ext == "tar" || ext == "gz" || ext == "xz" || ext == "7z") {
                    iconKind = QStringLiteral("archive");
                }
                item[QStringLiteral("iconKind")] = iconKind;

                // Tag
                try {
                    auto tag = TagManager::instance().get_tag(entry.path());
                    item[QStringLiteral("tag")] = tag.has_value() ? QString::fromStdString(tag.value()) : QString();
                } catch (...) {
                    item[QStringLiteral("tag")] = QString();
                }

                batch.append(item);
                totalFound++;

                if (batch.size() >= 25) {
                    QMetaObject::invokeMethod(this, [this, currentGen, b = std::move(batch)]() {
                        if (m_searchGeneration != currentGen || !m_isSearching) return;
                        m_searchResults.append(b);
                        emit searchResultsChanged();
                        emit fileListChanged();
                        rebuildColumnsData();
                    }, Qt::QueuedConnection);
                    batch.clear();
                }
            }
            return true;
        };

        if (recursive) {
            auto it = std::filesystem::recursive_directory_iterator(
                searchPath,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            auto end = std::filesystem::recursive_directory_iterator();
            while (!ec && it != end && !st.stop_requested()) {
                const auto& entry = *it;
                if (!processEntry(entry)) break;
                it.increment(ec);
            }
        } else {
            auto it = std::filesystem::directory_iterator(
                searchPath,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            auto end = std::filesystem::directory_iterator();
            while (!ec && it != end && !st.stop_requested()) {
                const auto& entry = *it;
                if (!processEntry(entry)) break;
                it.increment(ec);
            }
        }

        // Post any remaining items and mark search finished
        QMetaObject::invokeMethod(this, [this, currentGen, b = std::move(batch), totalFound]() {
            if (m_searchGeneration != currentGen || !m_isSearching) return;
            if (!b.isEmpty()) {
                m_searchResults.append(b);
            }
            m_searchFinished = true;
            emit searchFinishedChanged();
            emit searchFinished(totalFound);
            emit searchResultsChanged();
            emit fileListChanged();
            rebuildColumnsData();
        }, Qt::QueuedConnection);
    });
}

bool FilesBridge::navigateToPath(const QString& rawPath) {
    QString p = rawPath.trimmed();
    if (p.isEmpty()) return false;

    // 1. Tilde expansion
    if (p == QStringLiteral("~") || p.startsWith(QStringLiteral("~/"))) {
        QString home = homePath();
        if (p == QStringLiteral("~")) {
            p = home;
        } else {
            p = home + p.mid(1);
        }
    }

    // 2. Relative path resolution
    QFileInfo checkFi(p);
    if (checkFi.isRelative()) {
        p = QDir(m_currentPath).filePath(p);
    }
    p = QDir::cleanPath(p);

    // 3. Existence check
    QFileInfo fi(p);
    if (!fi.exists()) {
        tinexus::log::warn("[files] navigateToPath: path does not exist '{}'", p.toStdString());
        return false;
    }

    // 4. Navigate
    if (fi.isDir()) {
        cd(fi.canonicalFilePath());
        return true;
    } else if (fi.isFile()) {
        cd(fi.dir().canonicalPath());
        selectItem(fi.canonicalFilePath());
        return true;
    }

    cd(fi.canonicalFilePath());
    return true;
}

QVariantList FilesBridge::tabs() const {
    QVariantList list;
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        const auto& t = m_tabs[static_cast<size_t>(i)];
        QVariantMap map;
        map[QStringLiteral("id")] = t.id;
        map[QStringLiteral("index")] = i;
        map[QStringLiteral("title")] = t.title;
        map[QStringLiteral("path")] = t.currentPath;
        map[QStringLiteral("viewMode")] = t.viewMode;
        map[QStringLiteral("isActive")] = (i == m_activeTabIndex);
        list.append(map);
    }
    return list;
}

int FilesBridge::createTab(const QString& path) {
    // 1. Save active tab state
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        auto& active = m_tabs[static_cast<size_t>(m_activeTabIndex)];
        active.currentPath = m_currentPath;
        active.history = m_history;
        active.historyIndex = m_historyIndex;
        active.viewMode = m_viewMode;
        active.selectedPaths = m_selectedPaths;
        active.sortColumn = m_sortColumn;
        active.sortAscending = m_sortAscending;
    }

    QString target = path;
    if (target.isEmpty() || !QDir(target).exists()) {
        target = m_currentPath.isEmpty() ? homePath() : m_currentPath;
    }

    TabInfo tab;
    tab.id = ++m_nextTabId;
    tab.currentPath = target;
    tab.title = QFileInfo(target).fileName();
    if (tab.title.isEmpty() || target == "/") tab.title = QStringLiteral("Root");
    tab.viewMode = m_viewMode;
    tab.sortColumn = m_sortColumn;
    tab.sortAscending = m_sortAscending;
    tab.selectedPaths.clear();

    m_tabs.push_back(tab);
    int newIndex = static_cast<int>(m_tabs.size()) - 1;
    m_activeTabIndex = newIndex;

    m_history.clear();
    m_historyIndex = -1;
    m_selectedPaths.clear();
    loadDirectory(target, true);

    emit tabsChanged();
    emit activeTabIndexChanged();
    emit selectedPathsChanged();
    emit selectedPathChanged();
    return newIndex;
}

void FilesBridge::switchTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size()) || index == m_activeTabIndex) {
        return;
    }

    // 1. Save state of current tab
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        auto& active = m_tabs[static_cast<size_t>(m_activeTabIndex)];
        active.currentPath = m_currentPath;
        active.history = m_history;
        active.historyIndex = m_historyIndex;
        active.viewMode = m_viewMode;
        active.selectedPaths = m_selectedPaths;
        active.sortColumn = m_sortColumn;
        active.sortAscending = m_sortAscending;
    }

    // 2. Switch
    m_activeTabIndex = index;
    const auto& nextTab = m_tabs[static_cast<size_t>(index)];
    m_viewMode = nextTab.viewMode;
    m_selectedPaths = nextTab.selectedPaths;
    m_sortColumn = nextTab.sortColumn;
    m_sortAscending = nextTab.sortAscending;
    m_history = nextTab.history;
    m_historyIndex = nextTab.historyIndex;

    loadDirectory(nextTab.currentPath, false);

    emit tabsChanged();
    emit activeTabIndexChanged();
    emit viewModeChanged();
    emit sortChanged();
    emit selectedPathsChanged();
    emit selectedPathChanged();
}

bool FilesBridge::closeTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) {
        return false;
    }

    if (m_tabs.size() == 1) {
        // Single tab left, reset to home
        loadDirectory(homePath(), true);
        m_tabs[0].currentPath = m_currentPath;
        m_tabs[0].title = currentDirName();
        m_tabs[0].history = m_history;
        m_tabs[0].historyIndex = m_historyIndex;
        m_tabs[0].selectedPaths = m_selectedPaths;
        emit tabsChanged();
        return true;
    }

    bool wasActive = (index == m_activeTabIndex);
    m_tabs.erase(m_tabs.begin() + index);

    if (wasActive) {
        if (m_activeTabIndex >= static_cast<int>(m_tabs.size())) {
            m_activeTabIndex = static_cast<int>(m_tabs.size()) - 1;
        }
        const auto& nextTab = m_tabs[static_cast<size_t>(m_activeTabIndex)];
        m_viewMode = nextTab.viewMode;
        m_selectedPaths = nextTab.selectedPaths;
        m_sortColumn = nextTab.sortColumn;
        m_sortAscending = nextTab.sortAscending;
        m_history = nextTab.history;
        m_historyIndex = nextTab.historyIndex;
        loadDirectory(nextTab.currentPath, false);
        emit viewModeChanged();
        emit sortChanged();
        emit selectedPathsChanged();
        emit selectedPathChanged();
    } else if (m_activeTabIndex > index) {
        m_activeTabIndex--;
    }

    emit tabsChanged();
    emit activeTabIndexChanged();
    return true;
}

void FilesBridge::duplicateTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;
    createTab(m_tabs[static_cast<size_t>(index)].currentPath);
}

// ============================================================================
// Phase 6: inotify Live File Watcher Implementation
// ============================================================================
void FilesBridge::watchDirectory(const QString& path) {
    if (m_inotifyFd < 0 || path.isEmpty()) return;
    unwatchDirectory();

    QByteArray p = path.toUtf8();
    m_watchDescriptor = inotify_add_watch(m_inotifyFd, p.constData(),
        IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY | IN_ATTRIB);
    if (m_watchDescriptor < 0) {
        tinexus::log::warn("[files] Failed to add inotify watch on: {}", path.toStdString());
    } else {
        tinexus::log::info("[files] inotify watching: {} (wd={})", path.toStdString(), m_watchDescriptor);
    }
}

void FilesBridge::unwatchDirectory() {
    if (m_inotifyFd >= 0 && m_watchDescriptor >= 0) {
        inotify_rm_watch(m_inotifyFd, m_watchDescriptor);
        m_watchDescriptor = -1;
    }
}

void FilesBridge::onInotifyEvent() {
    if (m_inotifyFd < 0) return;

    alignas(struct inotify_event) char buffer[4096];
    ssize_t len = ::read(m_inotifyFd, buffer, sizeof(buffer));
    if (len <= 0) return;

    bool modifiedList = false;
    const struct inotify_event* event = nullptr;

    for (char* ptr = buffer; ptr < buffer + len;
         ptr += sizeof(struct inotify_event) + event->len) {
        event = reinterpret_cast<const struct inotify_event*>(ptr);

        if (event->len == 0) continue;
        QString name = QString::fromUtf8(event->name);
        if (name.isEmpty() || name == "." || name == ".." || name.endsWith(".tmp")) continue;

        QString fullPath = m_currentPath + "/" + name;
        QFileInfo fi(fullPath);

        if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO)) {
            bool alreadyExists = false;
            for (const auto& item : m_fileList) {
                if (item.toMap()[QStringLiteral("name")].toString() == name) {
                    alreadyExists = true;
                    break;
                }
            }
            if (!alreadyExists && fi.exists()) {
                QVariantMap item;
                item[QStringLiteral("name")] = fi.fileName();
                item[QStringLiteral("path")] = fi.absoluteFilePath();
                item[QStringLiteral("isDir")] = fi.isDir();
                item[QStringLiteral("isSymLink")] = fi.isSymLink();
                item[QStringLiteral("isHidden")] = fi.isHidden();

                if (fi.isDir()) {
                    QDir subDir(fi.absoluteFilePath());
                    int count = static_cast<int>(subDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count());
                    item[QStringLiteral("sizeStr")] = QStringLiteral("%1 items").arg(count);
                    item[QStringLiteral("iconKind")] = QStringLiteral("folder");
                } else {
                    qint64 bytes = fi.size();
                    if (bytes < 1024) item[QStringLiteral("sizeStr")] = QStringLiteral("%1 B").arg(bytes);
                    else if (bytes < 1024 * 1024) item[QStringLiteral("sizeStr")] = QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 1);
                    else if (bytes < 1024 * 1024 * 1024) item[QStringLiteral("sizeStr")] = QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
                    else item[QStringLiteral("sizeStr")] = QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);

                    QString ext = fi.suffix().toLower();
                    QString iconKind = QStringLiteral("file");
                    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "svg") iconKind = QStringLiteral("image");
                    else if (ext == "mp4" || ext == "mkv" || ext == "mov") iconKind = QStringLiteral("video");
                    else if (ext == "mp3" || ext == "wav" || ext == "flac") iconKind = QStringLiteral("audio");
                    else if (ext == "pdf") iconKind = QStringLiteral("pdf");
                    else if (ext == "txt" || ext == "md" || ext == "cpp" || ext == "hpp" || ext == "py") iconKind = QStringLiteral("code");
                    else if (ext == "zip" || ext == "tar" || ext == "gz") iconKind = QStringLiteral("archive");
                    item[QStringLiteral("iconKind")] = iconKind;
                }

                item[QStringLiteral("modified")] = fi.lastModified().toString(QStringLiteral("MMM d, yyyy h:mm AP"));

                try {
                    auto tag = TagManager::instance().get_tag(std::filesystem::path(fi.absoluteFilePath().toStdString()));
                    item[QStringLiteral("tag")] = tag.has_value() ? QString::fromStdString(tag.value()) : QString();
                } catch (...) {
                    item[QStringLiteral("tag")] = QString();
                }

                m_fileList.append(item);
                modifiedList = true;
            }
        } else if ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM)) {
            for (int i = 0; i < m_fileList.size(); ++i) {
                if (m_fileList[i].toMap()[QStringLiteral("name")].toString() == name) {
                    m_fileList.removeAt(i);
                    modifiedList = true;
                    break;
                }
            }
            if (m_selectedPaths.contains(fullPath)) {
                m_selectedPaths.removeAll(fullPath);
                if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
                    m_tabs[static_cast<size_t>(m_activeTabIndex)].selectedPaths = m_selectedPaths;
                }
                emit selectedPathsChanged();
                emit selectedPathChanged();
            }
        } else if ((event->mask & IN_MODIFY) || (event->mask & IN_ATTRIB)) {
            for (int i = 0; i < m_fileList.size(); ++i) {
                QVariantMap item = m_fileList[i].toMap();
                if (item[QStringLiteral("name")].toString() == name) {
                    if (fi.exists()) {
                        qint64 bytes = fi.size();
                        if (bytes < 1024) item[QStringLiteral("sizeStr")] = QStringLiteral("%1 B").arg(bytes);
                        else if (bytes < 1024 * 1024) item[QStringLiteral("sizeStr")] = QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 1);
                        else if (bytes < 1024 * 1024 * 1024) item[QStringLiteral("sizeStr")] = QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
                        else item[QStringLiteral("sizeStr")] = QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
                        item[QStringLiteral("modified")] = fi.lastModified().toString(QStringLiteral("MMM d, yyyy h:mm AP"));
                        m_fileList[i] = item;
                        modifiedList = true;
                    }
                    break;
                }
            }
        }
    }

    if (modifiedList) {
        sortBy(m_sortColumn, m_sortAscending);
    }
}

} // namespace tinexus::files

#include "moc_FilesBridge.cpp"
