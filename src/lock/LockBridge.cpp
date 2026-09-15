// ============================================================================
// LockBridge.cpp — Qt6 Bridge for tinexus-lock
//   • PAM authentication (existing)
//   • Milestone 6: IpcWatcher QThread for live wallpaper sync
// ============================================================================
#include "LockBridge.hpp"
#include <common/logger.hpp>
#include <ipcd/protocol/header.hpp>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <cstdint>
#include <vector>
#include <string>

// For IpcWatcher::run()
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <cerrno>

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

    // M6: Start the IPC watcher thread so the lockscreen always shows the
    // correct wallpaper immediately when it is raised, without polling a file.
    startIpcWatcher();
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
// M6: IPC Watcher startup (called from LockBridge constructor)
// ─────────────────────────────────────────────────────────────────────────────

void LockBridge::startIpcWatcher() {
    auto* watcher = new IpcWatcher(this); // owned by LockBridge (parent)

    // Resolve the IPC socket path using XDG_RUNTIME_DIR (pure XDG, no hardcoded UIDs)
    QString sockPath;
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] != '\0') {
        sockPath = QString::fromUtf8(xdg) + "/tinexus/ipc.sock";
    } else {
        sockPath = QString("/run/user/%1/tinexus/ipc.sock").arg(static_cast<uint>(::getuid()));
    }
    watcher->setSocketPath(sockPath);

    // Cross-thread signal: IpcWatcher runs on its own thread, emits to LockBridge's thread
    connect(watcher, &IpcWatcher::wallpaperPathReceived,
            this,    &LockBridge::onWallpaperPathReceived,
            Qt::QueuedConnection);

    m_ipcWatcherThread = watcher;
    watcher->start(QThread::LowPriority);
    log::info("[lock] IpcWatcher started (socket='{}')", sockPath.toStdString());
}

void LockBridge::onWallpaperPathReceived(const QString& path) {
    if (path == m_wallpaperPath) return; // No change
    m_wallpaperPath = path;
    emit wallpaperPathChanged();
    log::info("[lock] Wallpaper path updated to '{}'", path.toStdString());
}

} // namespace tinexus::lock

// ─────────────────────────────────────────────────────────────────────────────
// IpcWatcher::run() — Milestone 6
//
// Runs on its own QThread. Self-connects to tinexus-ipcd and blocks in a
// poll() read loop. Parses WALLPAPER_CHANGED (5002) frames and emits
// wallpaperPathReceived() for each one. The lockscreen QML then updates its
// background image without any file polling or timer-based tricks.
//
// Reconnect strategy: if ipcd is not reachable or the socket closes, the
// thread sleeps 5 seconds and retries. This ensures the lockscreen always
// gets the correct wallpaper even if ipcd restarts.
// ─────────────────────────────────────────────────────────────────────────────

namespace tinexus::lock {

void IpcWatcher::run() {
    using namespace ipcd::protocol;
    log::info("[lock/ipcwatcher] Thread started");

    while (!isInterruptionRequested()) {
        // ── Connect to tinexus-ipcd ──
        const std::string sock_path = m_socketPath.toStdString();
        int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            log::warn("[lock/ipcwatcher] socket(): {} — retry in 5s", std::strerror(errno));
            msleep(5000); continue;
        }

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        ::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

        if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            log::info("[lock/ipcwatcher] ipcd not reachable: {} — retry in 5s",
                      std::strerror(errno));
            ::close(fd); msleep(5000); continue;
        }
        log::info("[lock/ipcwatcher] Connected to tinexus-ipcd");

        // ── Register as subscriber for WALLPAPER_CHANGED (5002) ──
        auto send_frame = [&](MessageType mt, uint32_t seq,
                               const uint8_t* payload, uint32_t plen) -> bool {
            Header hdr{};
            hdr.magic = TINEXUS_IPC_MAGIC; hdr.version = TINEXUS_IPC_VERSION_1;
            hdr.msg_type = static_cast<uint16_t>(mt);
            hdr.sequence_id = seq; hdr.payload_len = plen;
            std::vector<uint8_t> buf(sizeof(hdr) + plen);
            std::memcpy(buf.data(), &hdr, sizeof(hdr));
            if (plen && payload) std::memcpy(buf.data() + sizeof(hdr), payload, plen);
            const uint8_t* p = buf.data(); size_t r = buf.size();
            while (r > 0) {
                ssize_t n = ::write(fd, p, r);
                if (n < 0) { if (errno == EINTR) continue; return false; }
                p += n; r -= static_cast<size_t>(n);
            }
            return true;
        };

        const char* svc = "lock";
        send_frame(MessageType::SYS_REGISTER_SERVICE, 1,
                   reinterpret_cast<const uint8_t*>(svc), static_cast<uint32_t>(std::strlen(svc) + 1));

        uint16_t topic = static_cast<uint16_t>(MessageType::WALLPAPER_CHANGED);
        send_frame(MessageType::SYS_SUBSCRIBE_TOPIC, 2,
                   reinterpret_cast<const uint8_t*>(&topic), sizeof(topic));

        // ── Read loop ──
        std::vector<uint8_t> buf;
        buf.reserve(sizeof(Header) + sizeof(WallpaperChangedPayload) + 16);

        bool disconnected = false;
        while (!isInterruptionRequested() && !disconnected) {
            struct pollfd pfd = { .fd = fd, .events = POLLIN | POLLHUP | POLLERR, .revents = 0 };
            int ret = ::poll(&pfd, 1, 1000); // 1s timeout to allow interruption check

            if (ret < 0) {
                if (errno == EINTR) continue;
                log::warn("[lock/ipcwatcher] poll() error: {}", std::strerror(errno));
                disconnected = true; break;
            }
            if (ret == 0) continue; // Timeout — loop back to check isInterruptionRequested()

            if (pfd.revents & (POLLHUP | POLLERR)) {
                log::warn("[lock/ipcwatcher] ipcd socket closed — reconnecting");
                disconnected = true; break;
            }

            if (pfd.revents & POLLIN) {
                uint8_t tmp[4096]; ssize_t n;
                while ((n = ::read(fd, tmp, sizeof(tmp))) > 0)
                    buf.insert(buf.end(), tmp, tmp + n);
                if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    disconnected = true; break;
                }

                constexpr size_t HDR = sizeof(Header);
                while (buf.size() >= HDR) {
                    Header hdr{}; std::memcpy(&hdr, buf.data(), HDR);
                    if (hdr.magic != TINEXUS_IPC_MAGIC || hdr.version != TINEXUS_IPC_VERSION_1) {
                        buf.clear(); break;
                    }
                    if (hdr.payload_len > 4u * 1024u * 1024u) { buf.clear(); break; }
                    if (buf.size() < HDR + hdr.payload_len) break;

                    if (static_cast<MessageType>(hdr.msg_type) == MessageType::WALLPAPER_CHANGED) {
                        if (hdr.payload_len >= sizeof(WallpaperChangedPayload)) {
                            WallpaperChangedPayload pkt{};
                            std::memcpy(&pkt, buf.data() + HDR, sizeof(pkt));
                            pkt.path[sizeof(pkt.path) - 1] = '\0';
                            emit wallpaperPathReceived(QString::fromUtf8(pkt.path));
                        }
                    }

                    buf.erase(buf.begin(),
                              buf.begin() + static_cast<ptrdiff_t>(HDR + hdr.payload_len));
                }
            }
        }

        ::close(fd);
        if (!disconnected) break; // Clean exit (interruption requested)
        log::info("[lock/ipcwatcher] Reconnecting in 5 seconds...");
        msleep(5000);
    }

    log::info("[lock/ipcwatcher] Thread exited cleanly");
}

} // namespace tinexus::lock

#include "moc_LockBridge.cpp"
