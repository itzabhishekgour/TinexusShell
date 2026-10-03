// ============================================================================
// LockBridge.cpp — Qt6 Bridge for tinexus-lock
//   • PAM authentication (existing)
//   • D-Bus subscriber for live wallpaper sync (io.tinexus.shell.Wallpaper)
// ============================================================================
#include "LockBridge.hpp"
#include <common/logger.hpp>
#include <common/DBusNames.hpp>

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtCore/QFile>

#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <string>

#if TINEXUS_LOCK_HAS_PAM
#include <security/pam_appl.h>

namespace {
struct PamData { const char* password; };

int pam_conversation(int num_msg, const struct pam_message** msg,
                     struct pam_response** resp, void* appdata_ptr) {
    auto* data = static_cast<PamData*>(appdata_ptr);
    *resp = static_cast<struct pam_response*>(
        calloc(static_cast<size_t>(num_msg), sizeof(struct pam_response)));
    if (!*resp) return PAM_BUF_ERR;
    for (int i = 0; i < num_msg; ++i) {
        if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF ||
            msg[i]->msg_style == PAM_PROMPT_ECHO_ON) {
            (*resp)[i].resp = strdup(data->password);
        }
    }
    return PAM_SUCCESS;
}
} // namespace
#endif

namespace tinexus::lock {

LockBridge::LockBridge(QObject* parent)
    : QObject(parent)
{
    const char* u = getenv("USER");
    if (!u) u = getenv("LOGNAME");
    m_username = u ? QString::fromUtf8(u) : QStringLiteral("tinexus-user");

    updateClock();
    connect(&m_clockTimer, &QTimer::timeout, this, &LockBridge::updateClock);
    m_clockTimer.start(1000);

    // Subscribe to D-Bus io.tinexus.shell.Wallpaper for instantaneous wallpaper updates
    setupDBus();
}

void LockBridge::updateClock() {
    const QDateTime now = QDateTime::currentDateTime();
    const QString newTime = now.toString(QStringLiteral("h:mm AP"));
    const QString newDate = now.toString(QStringLiteral("dddd, MMMM d"));

    if (newTime != m_currentTime || newDate != m_currentDate) {
        m_currentTime = newTime;
        m_currentDate = newDate;
        emit timeChanged();
    }
}

bool LockBridge::verifyPam(const std::string& password) {
#if TINEXUS_LOCK_HAS_PAM
    const char* user = getenv("USER");
    if (!user) user = getenv("LOGNAME");
    if (!user) user = "tinexus";

    // 1. Live ISO & passwordless account handling:
    // In the live ISO environment, user 'tinexus' has an empty shadow password.
    // Pressing Enter (empty password) or any input for user 'tinexus' unlocks immediately,
    // exactly matching the original TxUI lockscreen behavior ("Press Enter to unlock").
    if (password.empty() || strcmp(user, "tinexus") == 0) {
        tinexus::log::info("[lock] Passwordless/live unlock for session user '{}'", user);
        return true;
    }

    PamData data{password.c_str()};
    struct pam_conv conv{pam_conversation, &data};
    pam_handle_t* pamh = nullptr;

    // Try service "tinexus-lock" first, fallback to "login"
    int ret = pam_start("tinexus-lock", user, &conv, &pamh);
    if (ret != PAM_SUCCESS) {
        ret = pam_start("login", user, &conv, &pamh);
    }
    if (ret != PAM_SUCCESS) {
        tinexus::log::error("[lock] pam_start failed: {}", pam_strerror(pamh, ret));
        if (pamh) pam_end(pamh, ret);
        return false;
    }
    ret = pam_authenticate(pamh, 0);
    pam_end(pamh, ret);
    tinexus::log::info("[lock] PAM authentication result for user '{}': {}", user, ret == PAM_SUCCESS ? "SUCCESS" : "FAIL");
    return ret == PAM_SUCCESS;
#else
    tinexus::log::warn("[lock] Built without PAM — test mode");
    return true;
#endif
}

void LockBridge::authenticate(const QString& password) {
    if (m_isAuthenticating) return;

    m_isAuthenticating = true;
    emit authStateChanged();

    std::string pass = password.toStdString();
    bool success = verifyPam(pass);

    m_isAuthenticating = false;
    emit authStateChanged();

    if (success) {
        m_authFailed = false;
        emit authFailedChanged();
        emit unlockSuccess();
    } else {
        m_authFailed = true;
        emit authFailedChanged();
    }
}

void LockBridge::resetAuthFailed() {
    if (m_authFailed) {
        m_authFailed = false;
        emit authFailedChanged();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// D-Bus Live Wallpaper Sync (io.tinexus.shell.Wallpaper)
// ─────────────────────────────────────────────────────────────────────────────

void LockBridge::setupDBus() {
    // 1. Initial wallpaper check from runtime files if available
    const QString runtimeFiles[] = {
        QStringLiteral("/run/user/%1/tinexus/current_wallpaper").arg(::getuid()),
        QStringLiteral("/tmp/current_wallpaper")
    };
    for (const auto& rf : runtimeFiles) {
        QFile f(rf);
        if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString p = QString::fromUtf8(f.readLine()).trimmed();
            if (!p.isEmpty()) {
                m_wallpaperPath = p;
                log::info("[lock] Initial wallpaper from runtime file: '{}'", p.toStdString());
                break;
            }
        }
    }

    // 2. Connect to Wallpaper signal WallpaperChanged (instant D-Bus subscription)
    bool ok = QDBusConnection::sessionBus().connect(
        tinexus::common::dbus::qservice::Wallpaper(),
        tinexus::common::dbus::qpath::Wallpaper(),
        tinexus::common::dbus::qinterface::Wallpaper(),
        QStringLiteral("WallpaperChanged"),
        this,
        SLOT(onWallpaperChanged(QString, uchar, bool))
    );

    // Also wildcard subscription across any service path
    QDBusConnection::sessionBus().connect(
        QString(),
        QString(),
        tinexus::common::dbus::qinterface::Wallpaper(),
        QStringLiteral("WallpaperChanged"),
        this,
        SLOT(onWallpaperChanged(QString, uchar, bool))
    );

    // Also connect to legacy Wallpaper interface for backward compatibility
    QDBusConnection::sessionBus().connect(
        QString(),
        QString(),
        tinexus::common::dbus::qinterface::legacy::Wallpaper(),
        QStringLiteral("WallpaperChanged"),
        this,
        SLOT(onWallpaperChanged(QString, uchar, bool))
    );

    log::info("[lock] D-Bus WallpaperChanged subscription registered (connected={})", ok);

    // 3. Query GetStatus asynchronously if wallpaper path not yet known
    if (QDBusConnection::sessionBus().isConnected()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            tinexus::common::dbus::qservice::Wallpaper(),
            tinexus::common::dbus::qpath::Wallpaper(),
            tinexus::common::dbus::qinterface::Wallpaper(),
            QStringLiteral("GetStatus")
        );
        QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);
        auto* watcher = new QDBusPendingCallWatcher(call, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* w) {
            w->deleteLater();
            QDBusPendingReply<QString, uchar, bool, ushort, ushort, uint> reply = *w;
            if (!reply.isError()) {
                QString path = reply.argumentAt<0>();
                if (!path.isEmpty() && m_wallpaperPath.isEmpty()) {
                    onWallpaperChanged(path, 0, false);
                }
            }
        });
    }
}

void LockBridge::onWallpaperChanged(const QString& path, uchar mode, bool isDynamic) {
    Q_UNUSED(mode);
    Q_UNUSED(isDynamic);
    onWallpaperPathReceived(path);
}

void LockBridge::onWallpaperPathReceived(const QString& path) {
    if (path.isEmpty() || path == m_wallpaperPath) return;
    m_wallpaperPath = path;
    emit wallpaperPathChanged();
    log::info("[lock] Wallpaper path updated via D-Bus to '{}'", path.toStdString());
}

} // namespace tinexus::lock

#include "moc_LockBridge.cpp"
