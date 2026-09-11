// ============================================================================
// LockBridge.cpp — Qt6 Bridge for tinexus-lock with Real PAM Authentication
// ============================================================================
#include "LockBridge.hpp"
#include <common/logger.hpp>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <thread>

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

} // namespace tinexus::lock

#include "moc_LockBridge.cpp"
