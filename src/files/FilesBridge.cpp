// ============================================================================
// FilesBridge.cpp — Qt6 Bridge for tinexus-files with Real Filesystem Backend
// ============================================================================
#include "FilesBridge.hpp"
#include <common/logger.hpp>
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
    rootCol[QStringLiteral("dirName")] = currentDirName();
    rootCol[QStringLiteral("selectedIndex")] = -1;
    rootCol[QStringLiteral("items")] = m_fileList;
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

    m_selectedPath = targetPath;
    emit selectedPathChanged();

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
        if (m_viewMode == 2) {
            rebuildColumnsData();
        }
        emit viewModeChanged();
    }
}

void FilesBridge::selectItem(const QString& path) {
    if (m_selectedPath != path) {
        m_selectedPath = path;
        emit selectedPathChanged();
    }
}

QVariantMap FilesBridge::selectedItemDetails() const {
    if (m_selectedPath.isEmpty()) return QVariantMap();
    return const_cast<FilesBridge*>(this)->getFileDetails(m_selectedPath);
}

void FilesBridge::loadDirectory(const QString& path, bool addToHistory) {
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

        m_fileList.append(item);
    }

    if (!m_fileList.isEmpty() && m_selectedPath.isEmpty()) {
        m_selectedPath = m_fileList.first().toMap()[QStringLiteral("path")].toString();
        emit selectedPathChanged();
    }

    emit currentPathChanged();
    emit fileListChanged();
    rebuildColumnsData();
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
        if (m_selectedPath == oldPath) {
            selectItem(target);
        }
        refresh();
    }
    return ok;
}

bool FilesBridge::deleteItem(const QString& path) {
    QFileInfo fi(path);
    if (!fi.exists()) return false;
    bool ok = false;
    if (fi.isDir()) {
        QDir dir(path);
        ok = dir.removeRecursively();
    } else {
        ok = QFile::remove(path);
    }
    if (ok) {
        if (m_selectedPath == path) {
            m_selectedPath.clear();
            emit selectedPathChanged();
        }
        refresh();
    }
    return ok;
}

bool FilesBridge::moveItem(const QString& srcPath, const QString& destDir) {
    if (srcPath.isEmpty() || destDir.isEmpty()) return false;
    QFileInfo src(srcPath);
    if (!src.exists()) return false;
    QString target = destDir + "/" + src.fileName();
    bool ok = QFile::rename(srcPath, target);
    if (!ok) {
        // Cross-device fallback
        ok = (QProcess::execute(QStringLiteral("mv"), {srcPath, target}) == 0);
    }
    if (ok) refresh();
    return ok;
}

void FilesBridge::copyItem(const QString& path) {
    m_clipboardPath = path;
    m_clipboardIsCut = false;
    emit clipboardChanged();
}

void FilesBridge::cutItem(const QString& path) {
    m_clipboardPath = path;
    m_clipboardIsCut = true;
    emit clipboardChanged();
}

bool FilesBridge::pasteItem(const QString& targetDir) {
    if (m_clipboardPath.isEmpty()) return false;
    QString destDir = targetDir.isEmpty() ? m_currentPath : targetDir;
    QFileInfo srcInfo(m_clipboardPath);
    if (!srcInfo.exists()) return false;

    QString destPath = destDir + "/" + srcInfo.fileName();
    bool ok = false;
    if (m_clipboardIsCut) {
        ok = QFile::rename(m_clipboardPath, destPath);
        m_clipboardPath.clear();
        m_clipboardIsCut = false;
        emit clipboardChanged();
    } else {
        if (srcInfo.isDir()) {
            // Simple recursive copy or system cp -r
            ok = (QProcess::execute(QStringLiteral("cp"), {QStringLiteral("-r"), m_clipboardPath, destPath}) == 0);
        } else {
            ok = QFile::copy(m_clipboardPath, destPath);
        }
    }
    if (ok) refresh();
    return ok;
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

#include "moc_FilesBridge.cpp"
