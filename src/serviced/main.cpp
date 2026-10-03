// tinexus-serviced — Tinexus Platform Session Watchdog
//
// ARCHITECTURAL NOTE (arch-pivot-v2):
// tinexus-serviced is NO LONGER PID 1. It now runs as a standard systemd
// service under tinexus-session.target. All former PID-1 responsibilities
// have been migrated to native systemd units in data/systemd/:
//
//   Former responsibility              → Handled by
//   ─────────────────────────────────────────────────────────────────
//   mount /proc /sys /dev /run /tmp    → systemd (native, always)
//   systemd-udevd coldplug trigger     → systemd-udev.service
//   udevadm settle                     → systemd-udev-settle.service
//   modprobe Wi-Fi/audio/network       → systemd-udevd MODALIAS rules
//   rfkill unblock all                 → /etc/udev/rules.d/70-tinexus-rfkill.rules
//   insmod + ip link + udhcpc          → NetworkManager / systemd-networkd
//   serial console getty               → getty@ttyS0.service
//   GraphicsProbe env export           → tinexus-hardware-env.service (oneshot)
//   tinexus-splash fork/exec           → tinexus-splash.service
//   dbus-daemon supervision            → dbus.service (system)
//   PID-1 UID panic check              → not needed — systemd ensures user exists
//
// What serviced still owns (Tinexus-specific, not generic init):
//   - HeartbeatWatchdog: monitors tinexus-comp / tinexus-shell for liveness
//   - RuntimeControlSocket: tinexusctl command interface
//   - IpcdClient: connection to tinexus-ipcd for platform IPC bus
//   - LogindMimic: org.freedesktop.login1 D-Bus shim (PowerOff/Reboot/Suspend)
//   - restore_hardware_state(): brightness + ALSA volume from hardware.toml
//   - AppImage sweep: stale appimage_extract_* directory cleanup on session start

#include "common/logger.hpp"
#include "common/version.hpp"
#include "serviced/daemon_spec.hpp"
#include "serviced/dep_graph.hpp"
#include "serviced/process_manager.hpp"
#include "serviced/runtime_socket.hpp"
#include "serviced/heartbeat_watchdog.hpp"
#include "serviced/ipcd_client.hpp"
#include "serviced/logind_mimic.hpp"
#include "common/HardwareConfig.hpp"
#include "common/AudioUtils.hpp"
#include "common/BacklightUtils.hpp"
#include "common/RuntimePaths.hpp"
#include <iostream>
#include <fstream>
#include <csignal>
#include <cstdlib>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <vector>
#include <filesystem>

namespace {
tinexus::serviced::RuntimeControlSocket* g_socket{nullptr};
tinexus::serviced::ProcessManager* g_pm{nullptr};

// ── AppImage Cleanup Sweep ────────────────────────────────────────────────
// If a previous session was SIGKILLed, AppImage extraction dirs may linger.
// Sweep on session start to prevent OOM accumulation across reboots.
void sweep_stale_appimage_dirs() {
    std::string user_run = tinexus::common::RuntimePaths::get_user_runtime_dir();
    std::vector<std::string> paths = {"/tmp", user_run};
    for (const auto& path : paths) {
        if (!std::filesystem::exists(path)) continue;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_directory() &&
                    entry.path().filename().string().starts_with("appimage_extract_")) {
                    tinexus::log::warn("Sweeping stale AppImage extract dir: {}",
                                       entry.path().string());
                    std::filesystem::remove_all(entry.path());
                }
            }
        } catch (const std::exception& e) {
            tinexus::log::error("AppImage sweep error at {}: {}", path, e.what());
        }
    }
}

// ── Hardware State Restore ────────────────────────────────────────────────
// Restore brightness and ALSA volume from hardware.toml.
// ALSA routing setup is kept here because it reads the persisted config
// and applies it once at session start — not a kernel-init concern.
void setup_default_audio_routing() {
    auto cards = tinexus::hardware::AudioUtils::get_sound_cards();
    for (int retry = 0; retry < 15 && cards.empty(); ++retry) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        cards = tinexus::hardware::AudioUtils::get_sound_cards();
    }

    if (cards.empty()) {
        tinexus::log::info("No sound cards detected — skipping ALSA routing.");
        return;
    }

    int card_id = tinexus::hardware::AudioUtils::detect_primary_card_id();
    tinexus::log::info("Configuring default ALSA sound routing to Card {}", card_id);

    std::ofstream ofs("/etc/asound.conf");
    if (ofs.is_open()) {
        ofs << "# Tinexus Universal Audio Configuration\n"
            << "defaults.pcm.card " << card_id << "\n"
            << "defaults.ctl.card " << card_id << "\n\n"
            << "# 1. Primary Default: Route ALSA applications through PipeWire-Pulse\n"
            << "pcm.!default {\n"
            << "    type pulse\n"
            << "    fallback \"tinexus_hw\"\n"
            << "    hint {\n"
            << "        show on\n"
            << "        description \"Default Audio Device (PipeWire-Pulse)\"\n"
            << "    }\n"
            << "}\n\n"
            << "ctl.!default {\n"
            << "    type pulse\n"
            << "    fallback \"tinexus_hw\"\n"
            << "}\n\n"
            << "# 2. Hardware Fallback: Direct ALSA with dmix multi-stream mixing\n"
            << "pcm.tinexus_hw {\n"
            << "    type plug\n"
            << "    slave.pcm \"dmix:" << card_id << ",0\"\n"
            << "}\n\n"
            << "ctl.tinexus_hw {\n"
            << "    type hw\n"
            << "    card " << card_id << "\n"
            << "}\n";
        ofs.close();
        tinexus::log::info("Generated /etc/asound.conf with pulse + dmix fallback for Card {}", card_id);
    }

    // alsactl init — initialize hardware codecs
    if (std::filesystem::exists("/usr/sbin/alsactl")) {
        pid_t ap = fork();
        if (ap == 0) {
            execl("/usr/sbin/alsactl", "alsactl", "init", "-q", nullptr);
            _exit(0);
        }
        if (ap > 0) waitpid(ap, nullptr, 0);
    }

    auto run_amixer = [](const std::vector<const char*>& args) {
        pid_t p = fork();
        if (p == 0) {
            std::vector<char*> c_args;
            c_args.push_back(const_cast<char*>("/usr/bin/amixer"));
            for (auto* a : args) c_args.push_back(const_cast<char*>(a));
            c_args.push_back(nullptr);
            execv("/usr/bin/amixer", c_args.data());
            execv("/bin/amixer", c_args.data());
            _exit(0);
        }
        if (p > 0) waitpid(p, nullptr, 0);
    };

    std::string cid_str = std::to_string(card_id);
    for (const char* ctrl : {"Master", "Speaker", "Headphone", "PCM", "Front", "Line Out"}) {
        run_amixer({"-c", cid_str.c_str(), "sset", ctrl, "unmute", "-q"});
        if (std::string(ctrl) != "Master") {
            run_amixer({"-c", cid_str.c_str(), "sset", ctrl, "100%", "unmute", "-q"});
        }
    }
}

void restore_hardware_state() {
    tinexus::log::info("Restoring hardware configuration from hardware.toml...");
    setup_default_audio_routing();

    auto cfg = tinexus::hardware::HardwareConfig::load();

    // Brightness
    int b_pct = cfg.brightness;
    tinexus::hardware::BacklightUtils::set_brightness_percent(b_pct, /*persist=*/false, /*throttle=*/false);
    tinexus::log::info("Applied display brightness: {}%", b_pct);

    // Audio volume
    auto cards = tinexus::hardware::AudioUtils::get_sound_cards();
    if (!cards.empty()) {
        int v_pct = cfg.volume;
        bool is_muted = cfg.muted;
        int card_id = tinexus::hardware::AudioUtils::detect_primary_card_id();
        std::string card_str = std::to_string(card_id);
        std::string ctrl = tinexus::hardware::AudioUtils::detect_primary_control();
        tinexus::hardware::AudioUtils::set_volume_percent(v_pct, /*persist=*/false, /*throttle=*/false);

        const char* mute_arg = is_muted ? "mute" : "unmute";
        pid_t p = fork();
        if (p == 0) {
            execl("/usr/bin/amixer", "amixer", "-c", card_str.c_str(),
                  "sset", ctrl.c_str(), mute_arg, "-q", nullptr);
            _exit(0);
        }
        if (p > 0) waitpid(p, nullptr, 0);
        tinexus::log::info("Applied audio volume: {}% on card {} (muted={})",
                           v_pct, card_id, is_muted);
    }
}

// ── Signal Handling ───────────────────────────────────────────────────────
void signal_handler(int signal) {
    if (signal == SIGCHLD) {
        // Reap supervised child processes and forward to ProcessManager.
        int status = 0;
        pid_t pid = 0;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (g_pm) {
                int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                int term_sig  = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
                g_pm->handle_child_exit(pid, exit_code, term_sig);
            }
        }
    } else if (g_socket) {
        tinexus::log::info("Received signal {}, stopping tinexus-serviced...", signal);
        g_socket->stop();
        tinexus::serviced::IpcdClient::instance().stop();
        tinexus::serviced::LogindMimic::instance().stop();
    }
}
} // namespace

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-serviced");
    tinexus::log::info("Starting Tinexus Platform Watchdog v{} (systemd session service)...",
                       tinexus::VERSION_STRING);

    // ── Signal Handlers ───────────────────────────────────────────────────
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGCHLD, signal_handler);

    // ── Stale AppImage Cleanup ────────────────────────────────────────────
    sweep_stale_appimage_dirs();

    // ── Hardware State Restore ────────────────────────────────────────────
    // Applies persisted brightness and ALSA volume from hardware.toml.
    // This is Tinexus-specific state management — not kernel init work.
    restore_hardware_state();

    // ── Tinexus Supervision Graph ─────────────────────────────────────────
    // serviced supervises Tinexus-specific session daemons via its own
    // HeartbeatWatchdog + ProcessManager, complementing systemd's unit restart
    // with application-level health checks (e.g., IPC heartbeat timeouts).
    //
    // NOTE: dbus-daemon, tinexus-comp, and tinexus-splash are now managed
    // entirely by systemd units. Only tinexus-session (the Qt application session
    // proxy, not the target) remains here as a supervised child.
    tinexus::serviced::DependencyGraph graph;

    tinexus::serviced::DaemonSpec session;
    session.id         = "session";
    session.executable = "tinexus-session";
    session.critical   = true;
    graph.add_service(session);

    if (graph.has_cycle()) {
        tinexus::log::error("FATAL: Circular dependency detected in supervision tree!");
        return 1;
    }

    tinexus::serviced::ProcessManager pm(graph);
    g_pm = &pm;

    tinexus::serviced::RuntimeControlSocket runtime_sock(pm);
    g_socket = &runtime_sock;

    if (!runtime_sock.start()) {
        tinexus::log::error("Failed to start Runtime Control Socket");
        return 1;
    }

    // ── LogindMimic ───────────────────────────────────────────────────────
    // Claim org.freedesktop.login1 on the system bus so that Tinexus UI
    // components can call PowerOff/Reboot/Suspend without real logind.
    // When systemd-logind is present (production), this service will fail
    // to claim the name — that is expected and non-fatal on systems with a
    // full systemd-logind. On the minimal Tinexus rootfs it provides the shim.
    if (!tinexus::serviced::LogindMimic::instance().start()) {
        tinexus::log::warn("LogindMimic: Failed to claim org.freedesktop.login1 — "
                           "either logind is running (OK) or system bus is unavailable.");
    }

    tinexus::log::info("Platform Watchdog ready. Starting supervised session tree...");
    pm.start_all_services();

    tinexus::serviced::IpcdClient::instance().start();

    runtime_sock.run_accept_loop();
    return 0;
}
