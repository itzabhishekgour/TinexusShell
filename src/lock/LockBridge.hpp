// ============================================================================
// LockBridge.hpp — Qt6 Bridge for tinexus-lock
//   • PAM authentication (existing)
//   • Milestone 6: IpcWatcher QThread for live wallpaper sync
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QThread>

namespace tinexus::lock {

class LockBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentTime   READ currentTime   NOTIFY timeChanged)
    Q_PROPERTY(QString currentDate   READ currentDate   NOTIFY timeChanged)
    Q_PROPERTY(QString username      READ username      CONSTANT)
    Q_PROPERTY(bool   isAuthenticating READ isAuthenticating NOTIFY authStateChanged)
    Q_PROPERTY(bool   authFailed     READ authFailed    NOTIFY authFailedChanged)
    // M6: Live wallpaper path — updated when tinexus-ipcd broadcasts WALLPAPER_CHANGED
    Q_PROPERTY(QString wallpaperPath READ wallpaperPath NOTIFY wallpaperPathChanged)

public:
    explicit LockBridge(QObject* parent = nullptr);
    ~LockBridge() override = default;

    QString currentTime()   const { return m_currentTime; }
    QString currentDate()   const { return m_currentDate; }
    QString username()      const { return m_username; }
    bool    isAuthenticating() const { return m_isAuthenticating; }
    bool    authFailed()    const { return m_authFailed; }
    // M6: wallpaperPath — empty string until first IPC message arrives
    QString wallpaperPath() const { return m_wallpaperPath; }

    Q_INVOKABLE void authenticate(const QString& password);
    Q_INVOKABLE void resetAuthFailed();

signals:
    void timeChanged();
    void authStateChanged();
    void authFailedChanged();
    void unlockSuccess();
    // M6: Emitted when tinexus-wallpaper broadcasts a new path via ipcd
    void wallpaperPathChanged();

private slots:
    void updateClock();
    // M6: Slot connected to IpcWatcher::wallpaperPathReceived (cross-thread Qt signal)
    void onWallpaperPathReceived(const QString& path);

private:
    bool verifyPam(const std::string& password);
    void startIpcWatcher();

    QString m_currentTime;
    QString m_currentDate;
    QString m_username;
    QString m_wallpaperPath;          // M6: live path, '' until first IPC msg
    bool    m_isAuthenticating{false};
    bool    m_authFailed{false};
    QTimer  m_clockTimer;
    QThread* m_ipcWatcherThread{nullptr}; // M6: owned QThread for IpcWatcher
};

// ─────────────────────────────────────────────────────────────────────────────
// IpcWatcher — QThread that self-connects to tinexus-ipcd and emits
// wallpaperPathReceived() whenever a WALLPAPER_CHANGED (5002) frame arrives.
//
// Runs entirely on its own thread. Uses blocking read() — no Qt event loop
// needed inside run(). Communicates with LockBridge via Qt queued signals.
// ─────────────────────────────────────────────────────────────────────────────
class IpcWatcher : public QThread {
    Q_OBJECT
public:
    explicit IpcWatcher(QObject* parent = nullptr) : QThread(parent) {}
    ~IpcWatcher() override { requestInterruption(); wait(2000); }

    // Call before start() to set a non-default socket path (used in tests)
    void setSocketPath(const QString& path) { m_socketPath = path; }

signals:
    // Emitted on the IpcWatcher's thread — LockBridge connects with Qt::QueuedConnection
    void wallpaperPathReceived(const QString& path);

protected:
    void run() override;

private:
    QString m_socketPath; // XDG-resolved path, set before start()
};

} // namespace tinexus::lock
