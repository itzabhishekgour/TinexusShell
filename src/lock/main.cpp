#include "common/logger.hpp"
#include "common/version.hpp"
#include <ctime>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <termios.h>
#include <string>

#if TINEXUS_LOCK_HAS_PAM
#include <security/pam_appl.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// tinexus-lock — Session lock screen
//
// Visual design (from docs/05_UI_UX_GUIDELINES.md):
//   • Background: wallpaper + 60px blur + 70% brightness tint  → Phase B (GPU)
//   • Clock: Inter Bold 72px, centered
//   • Password field: glassmorphism card, accent glow on focus  → Phase B (GUI)
//   • Wrong password: horizontal shake (400ms spring)
//
// This version implements terminal-based lock UI.
// Phase B will implement the full Wayland ext-session-lock-v1 surface.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// ── Read password securely (no echo) ─────────────────────────────────────
std::string read_password_noecho(const char* prompt) {
    struct termios oldt{}, newt{};
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~static_cast<unsigned int>(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    printf("%s", prompt);
    fflush(stdout);

    std::string password;
    char ch = 0;
    while (read(STDIN_FILENO, &ch, 1) == 1 && ch != '\n' && ch != '\r') {
        if (ch == 127 || ch == '\b') {
            if (!password.empty()) { password.pop_back(); printf("\b \b"); fflush(stdout); }
        } else {
            password += ch;
            printf("•"); // show bullet instead of nothing — more intuitive
            fflush(stdout);
        }
    }
    printf("\n");
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return password;
}

#if TINEXUS_LOCK_HAS_PAM
// ── PAM conversation ─────────────────────────────────────────────────────
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

bool authenticate(const std::string& password) {
    const char* user = getenv("USER");
    if (!user) user = getenv("LOGNAME");
    if (!user) user = "root";

    PamData data{password.c_str()};
    struct pam_conv conv{pam_conversation, &data};
    pam_handle_t* pamh = nullptr;

    int ret = pam_start("login", user, &conv, &pamh);
    if (ret != PAM_SUCCESS) {
        tinexus::log::error("[lock] pam_start failed: {}", pam_strerror(pamh, ret));
        if (pamh) pam_end(pamh, ret);
        return false;
    }
    ret = pam_authenticate(pamh, 0);
    pam_end(pamh, ret);
    return ret == PAM_SUCCESS;
}
#else
// ── Stub: accept any password when PAM is unavailable ────────────────────
bool authenticate(const std::string& password) {
    // In production, PAM is always available. This stub is for dev builds only.
    tinexus::log::warn("[lock] PAM not compiled in — accepting any non-empty password");
    return !password.empty();
}
#endif

// ── Render the lock screen UI to terminal ────────────────────────────────
void render_lockscreen(int failed_attempts, bool locked_out, int lockout_remaining) {
    // Clear + dark background
    printf("\033[2J\033[H");
    printf("\033[48;2;10;10;14m\033[38;2;240;240;248m"); // #0A0A0E bg, #F0F0F8 text

    for (int i = 0; i < 7; i++) printf("\n");

    // ── Clock ──────────────────────────────────────────────────────────────
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char time_buf[16], date_buf[40];
    strftime(time_buf, sizeof(time_buf), "%H:%M", t);
    strftime(date_buf, sizeof(date_buf), "%A, %B %d", t);

    // Bold clock — center in 80 cols
    printf("\033[1m%*s%s\033[0m\n",
           static_cast<int>((80 - strlen(time_buf)) / 2), "", time_buf);
    printf("\033[38;2;144;144;168m%*s%s\033[0m\n\n",
           static_cast<int>((80 - strlen(date_buf)) / 2), "", date_buf);

    // ── Tinexus logo lockup ──────────────────────────────────────────────
    printf("\033[38;2;107;140;239m"); // accent #6B8CEF
    printf("                                  ╔══════════════╗\n");
    printf("                                  ║  🔒 TINEXUS  ║\n");
    printf("                                  ╚══════════════╝\n\n");
    printf("\033[0m");

    // ── Status / error ────────────────────────────────────────────────────
    if (locked_out) {
        printf("\033[38;2;239;107;107m");
        printf("%*s⚠  Too many attempts. Locked out for %d second%s.\n\n",
               24, "", lockout_remaining, lockout_remaining == 1 ? "" : "s");
        printf("\033[0m");
        fflush(stdout);
        return;
    }

    if (failed_attempts > 0) {
        // Simulate shake by flashing red
        printf("\033[38;2;239;107;107m");
        printf("%*s✗  Incorrect password (%d/5 attempts)\n\n",
               24, "", failed_attempts);
        printf("\033[0m");
    }

    // ── Password field ────────────────────────────────────────────────────
    printf("\033[38;2;107;140;239m");
    printf("                            ┌──────────────────────┐\n");
    printf("                            │  Password: \033[0m");
    fflush(stdout);
    // Caller will read password here
}

void render_lockscreen_close() {
    printf("\033[38;2;107;140;239m  │\n");
    printf("                            └──────────────────────┘\n");
    printf("\033[0m");
    fflush(stdout);
}

// ── Flash red shake animation ─────────────────────────────────────────────
void shake_animation() {
    for (int i = 0; i < 4; ++i) {
        printf("\033[48;2;50;5;5m\033[2J\033[H\033[0m");
        fflush(stdout);
        usleep(55000); // 55ms
        printf("\033[48;2;10;10;14m\033[2J\033[H\033[0m");
        fflush(stdout);
        usleep(55000);
    }
}

} // namespace

int main(int /*argc*/, char** /*argv*/) {
    tinexus::log::set_component_name("tinexus-lock");
    tinexus::log::info("Starting tinexus-lock v{} (PAM={})",
                       tinexus::VERSION_STRING, TINEXUS_LOCK_HAS_PAM);

    // Hide cursor
    printf("\033[?25l");
    fflush(stdout);

    constexpr int MAX_ATTEMPTS    = 5;
    constexpr int LOCKOUT_SECONDS = 30;
    int failed_attempts = 0;

    while (true) {
        // ── Brute-force lockout ────────────────────────────────────────────
        if (failed_attempts >= MAX_ATTEMPTS) {
            for (int remaining = LOCKOUT_SECONDS; remaining > 0; --remaining) {
                render_lockscreen(failed_attempts, true, remaining);
                sleep(1);
            }
            failed_attempts = 0;
            continue;
        }

        // ── Render lock screen ────────────────────────────────────────────
        render_lockscreen(failed_attempts, false, 0);
        std::string password = read_password_noecho("");
        render_lockscreen_close();

        if (password.empty()) continue;

        if (authenticate(password)) {
            tinexus::log::info("[lock] Authentication successful — session unlocked");
            // Restore terminal
            printf("\033[?25h\033[0m\033[2J\033[H");
            fflush(stdout);
            return 0;
        }

        ++failed_attempts;
        tinexus::log::warn("[lock] Authentication failed (attempt {}/{})",
                           failed_attempts, MAX_ATTEMPTS);
        shake_animation();
    }
}
