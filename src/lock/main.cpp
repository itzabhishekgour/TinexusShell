#include "common/logger.hpp"
#include "common/version.hpp"
#include "lock/LockWidget.hpp"
#include <txui/window/Window.hpp>
#include <txui/input/Event.hpp>
#include <txui/core/SingleInstance.hpp>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <string>
#include <chrono>

#if TINEXUS_LOCK_HAS_PAM
#include <security/pam_appl.h>
#endif

namespace {

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
// ── Stub: in no-PAM mode, empty password means no password is set — bypass directly ──────────────────
bool authenticate(const std::string& password) {
    tinexus::log::warn("[lock] PAM not compiled in — empty password bypasses lock");
    // If no password typed, treat as 'no password set' — unlock immediately
    // If password typed, accept any non-empty string (test mode)
    return true; // always allow in no-PAM mode
}
#endif


char key_to_char(txui::Key key, bool shift) {
    // simplified key mapping for password input
    if (key >= txui::Key::A && key <= txui::Key::Z) {
        char base = shift ? 'A' : 'a';
        return static_cast<char>(base + (static_cast<int>(key) - static_cast<int>(txui::Key::A)));
    }
    if (key >= txui::Key::N0 && key <= txui::Key::N9) {
        if (!shift) return static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(txui::Key::N0)));
        // handle shift numbers (symbols) if needed, simplified for now
    }
    // Very basic mapping for demo purposes.
    if (key == txui::Key::Space) return ' ';
    return '\0';
}

} // namespace

int main(int /*argc*/, char** /*argv*/) {
    tinexus::log::set_component_name("tinexus-lock");
    tinexus::log::info("Starting tinexus-lock graphical UI v{} (PAM={})",
                       tinexus::VERSION_STRING, TINEXUS_LOCK_HAS_PAM);

    txui::SingleInstance single_instance("tinexus-lock");
    if (!single_instance.is_primary()) {
        tinexus::log::warn("[lock] tinexus-lock is already active; exiting secondary instance.");
        return 0;
    }

    auto window = txui::Window::create(800, 600, "Tinexus Lock");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("[lock] Failed to connect to Wayland display!");
        return 1;
    }

    auto root = txui::make_ref<tinexus::lock::LockWidget>();
    window->set_root_widget(root);
    window->set_fullscreen(true);

    // Warm-up: wait for the compositor's fullscreen configure event to arrive
    // before the first visible present(). Without this, the first frame renders
    // at the hardcoded 1920×1080 instead of the actual output size (e.g. 1536×793
    // on Virtual-1), producing a visibly wrong/clipped first frame.
    //
    // Strategy: drain events every 8ms until window->width() changes from the
    // initial hardcoded value (configure processed), or 400ms timeout expires.
    {
        const uint32_t initial_w = window->width(); // hardcoded value from create()
        txui::Event ev;
        for (int i = 0; i < 50; ++i) {            // 50 × 8ms = 400ms hard timeout
            while (window->poll_event(ev)) {}      // drain pending events
            if (window->width() != initial_w) {
                tinexus::log::info("[lock] Fullscreen configure received: {}×{} (after {}ms)",
                                   window->width(), window->height(), i * 8);
                break;
            }
            usleep(8000); // 8ms
        }
        if (window->width() == initial_w) {
            tinexus::log::warn("[lock] Fullscreen configure not received within 400ms — using initial dimensions.");
        }
    }

    // Initial present — window is now at the correct compositor-assigned size
    window->present();


    constexpr int MAX_ATTEMPTS    = 5;
    constexpr int LOCKOUT_SECONDS = 30;
    int failed_attempts = 0;
    
    auto lockout_end_time = std::chrono::steady_clock::now();
    bool locked_out = false;

    auto last_caret_time = std::chrono::steady_clock::now();
    auto last_clock_time = std::chrono::steady_clock::now();
    auto last_shake_time = std::chrono::steady_clock::now();

    bool running = true;
    bool needs_redraw = true;

    while (running && !window->should_close()) {
        auto now = std::chrono::steady_clock::now();

        // Caret blink timer (500ms)
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_caret_time).count() >= 500) {
            root->toggle_caret();
            last_caret_time = now;
            needs_redraw = true;
        }

        // Clock update timer (1000ms)
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_clock_time).count() >= 1) {
            last_clock_time = now;
            needs_redraw = true;
        }

        // Shake animation timer (60ms per frame — 9 frames = ~540ms total)
        if (root->is_shaking() &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_shake_time).count() >= 60) {
            root->advance_shake();
            last_shake_time = now;
            needs_redraw = true;
        }

        // Handle lockout timer
        if (locked_out) {
            if (now >= lockout_end_time) {
                locked_out = false;
                failed_attempts = 0;
                root->set_lockout(0);
                needs_redraw = true;
            } else {
                int remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(lockout_end_time - now).count());
                root->set_lockout(remaining);
                needs_redraw = true;
            }
        }

        // Process pending events
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                // Ignore window close on lock screen
            } else if (event.type == txui::EventType::KeyDown && !locked_out) {
                needs_redraw = true;
                root->set_caret_visible(true);
                last_caret_time = std::chrono::steady_clock::now();

                if (event.keyboard.key == txui::Key::Escape) {
                    root->clear_password();
                } else if (event.keyboard.key == txui::Key::Backspace) {
                    root->remove_password_char();
                } else if (event.keyboard.key == txui::Key::Enter) {
                    std::string pwd = root->get_password();
                    if (authenticate(pwd)) {
                        tinexus::log::info("[lock] Authentication successful — session unlocked");
                        running = false;
                        break;
                    } else {
                        ++failed_attempts;
                        tinexus::log::warn("[lock] Authentication failed (attempt {}/{})", failed_attempts, MAX_ATTEMPTS);
                        root->trigger_shake_animation();
                        if (failed_attempts >= MAX_ATTEMPTS) {
                            locked_out = true;
                            lockout_end_time = std::chrono::steady_clock::now() + std::chrono::seconds(LOCKOUT_SECONDS);
                        }
                    }
                } else {
                    bool shift = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Shift);
                    char ch = key_to_char(event.keyboard.key, shift);
                    if (ch != '\0') {
                        root->add_password_char(ch);
                    }
                }
            }
        }

        if (!running) break;

        if (needs_redraw) {
            window->present();
            needs_redraw = false;
        }

        // Compute sleep timeout until next timer trigger (at most 500ms)
        auto now_after = std::chrono::steady_clock::now();
        int ms_until_caret = 500 - static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now_after - last_caret_time).count());
        if (ms_until_caret < 10) ms_until_caret = 10;
        if (ms_until_caret > 500) ms_until_caret = 500;

        window->wait_timeout(ms_until_caret);
    }

    tinexus::log::info("[lock] Exiting lock screen.");
    return 0;
}
