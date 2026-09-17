// ============================================================================
// LockBridge.hpp — Qt6 Bridge for tinexus-lock
//   • PAM authentication (existing)
//   • D-Bus subscriber for live wallpaper sync (io.tinexus.Wallpaper)
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>

namespace tinexus::lock {

class LockBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentTime   READ currentTime   NOTIFY timeChanged)
    Q_PROPERTY(QString currentDate   READ currentDate   NOTIFY timeChanged)
    Q_PROPERTY(QString username      READ username      CONSTANT)
    Q_PROPERTY(bool   isAuthenticating READ isAuthenticating NOTIFY authStateChanged)
    Q_PROPERTY(bool   authFailed     READ authFailed    NOTIFY authFailedChanged)
    // Live wallpaper path — updated instantly via io.tinexus.Wallpaper.WallpaperChanged
    Q_PROPERTY(QString wallpaperPath READ wallpaperPath NOTIFY wallpaperPathChanged)

public:
    explicit LockBridge(QObject* parent = nullptr);
    ~LockBridge() override = default;

    QString currentTime()   const { return m_currentTime; }
    QString currentDate()   const { return m_currentDate; }
    QString username()      const { return m_username; }
    bool    isAuthenticating() const { return m_isAuthenticating; }
    bool    authFailed()    const { return m_authFailed; }
    QString wallpaperPath() const { return m_wallpaperPath; }

    Q_INVOKABLE void authenticate(const QString& password);
    Q_INVOKABLE void resetAuthFailed();

signals:
    void timeChanged();
    void authStateChanged();
    void authFailedChanged();
    void unlockSuccess();
    // Emitted when io.tinexus.Wallpaper broadcasts a new wallpaper via D-Bus
    void wallpaperPathChanged();

public slots:
    // Slot connected to io.tinexus.Wallpaper.WallpaperChanged via D-Bus
    void onWallpaperChanged(const QString& path, uchar mode = 0, bool isDynamic = false);
    void onWallpaperPathReceived(const QString& path);

private slots:
    void updateClock();

private:
    bool verifyPam(const std::string& password);
    void setupDBus();

    QString m_currentTime;
    QString m_currentDate;
    QString m_username;
    QString m_wallpaperPath;          // live path
    bool    m_isAuthenticating{false};
    bool    m_authFailed{false};
    QTimer  m_clockTimer;
};

} // namespace tinexus::lock
