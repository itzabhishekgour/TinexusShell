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
#include "common/GraphicsProbe.hpp"
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

void run_udev_setup() {
    auto run_cmd = [](const char* path, std::vector<char*> args) {
        if (!std::filesystem::exists(path)) return;
        pid_t pid = fork();
        if (pid == 0) {
            execv(path, args.data());
            _exit(127);
        } else if (pid > 0) {
            int status = 0;
            waitpid(pid, &status, 0);
        }
    };

    tinexus::log::info("Starting udev daemon for automated kernel driver and device discovery...");
    run_cmd("/lib/systemd/systemd-udevd", {
        const_cast<char*>("/lib/systemd/systemd-udevd"),
        const_cast<char*>("--daemon"),
        nullptr
    });
    run_cmd("/sbin/udevd", {
        const_cast<char*>("/sbin/udevd"),
        const_cast<char*>("--daemon"),
        nullptr
    });
    run_cmd("/bin/udevd", {
        const_cast<char*>("/bin/udevd"),
        const_cast<char*>("--daemon"),
        nullptr
    });
    sleep(1); // Give udevd time to bind netlink and control sockets

    // Universal Hardware Discovery: Trigger subsystems and devices.
    // Kernel MODALIAS events will prompt udev rules to load all required vendor modules
    // (Intel HDA, Realtek, AMD, USB audio, Wi-Fi, Ethernet, DRM/i915/amdgpu) automatically.
    tinexus::log::info("Triggering udev coldplug discovery (subsystems)...");
    run_cmd("/usr/bin/udevadm", {
        const_cast<char*>("/usr/bin/udevadm"),
        const_cast<char*>("trigger"),
        const_cast<char*>("--action=add"),
        const_cast<char*>("--type=subsystems"),
        nullptr
    });

    tinexus::log::info("Triggering udev coldplug discovery (devices)...");
    run_cmd("/usr/bin/udevadm", {
        const_cast<char*>("/usr/bin/udevadm"),
        const_cast<char*>("trigger"),
        const_cast<char*>("--action=add"),
        const_cast<char*>("--type=devices"),
        nullptr
    });

    run_cmd("/usr/bin/udevadm", {
        const_cast<char*>("/usr/bin/udevadm"),
        const_cast<char*>("settle"),
        const_cast<char*>("--timeout=8"),
        nullptr
    });
    sleep(1); // Ensure udev database is flushed to /run/udev/data/ before session starts

    // ── Universal RF Unblock (Wi-Fi & Bluetooth) ──
    tinexus::log::info("Unblocking all wireless radios via rfkill...");
    run_cmd("/usr/sbin/rfkill", {
        const_cast<char*>("/usr/sbin/rfkill"),
        const_cast<char*>("unblock"),
        const_cast<char*>("all"),
        nullptr
    });
    run_cmd("/bin/rfkill", {
        const_cast<char*>("/bin/rfkill"),
        const_cast<char*>("unblock"),
        const_cast<char*>("all"),
        nullptr
    });

    // ── Universal Wi-Fi & Network Kernel Driver Probing ──
    // Probe primary modern Wi-Fi drivers (MediaTek MT7921/MT7922, Intel iwlwifi, Realtek rtw88/rtw89, Qualcomm, Realtek r8169)
    tinexus::log::info("Ensuring essential network/Wi-Fi kernel drivers are loaded...");
    const char* wifi_drivers[] = {
        "cfg80211", "mac80211", "mt76", "mt76_connac_lib", "mt7921_common", "mt7921e", "mt7921u",
        "iwlwifi", "iwlmvm", "rtw88_8821ce", "rtw88_pci", "rtw89_8852be", "rtw89_pci", "r8169",
        nullptr
    };
    for (int i = 0; wifi_drivers[i] != nullptr; ++i) {
        pid_t p = fork();
        if (p == 0) {
            execl("/sbin/modprobe", "modprobe", "-q", wifi_drivers[i], nullptr);
            execl("/usr/sbin/modprobe", "modprobe", "-q", wifi_drivers[i], nullptr);
            execl("/bin/modprobe", "modprobe", "-q", wifi_drivers[i], nullptr);
            execl("/usr/bin/modprobe", "modprobe", "-q", wifi_drivers[i], nullptr);
            execlp("modprobe", "modprobe", "-q", wifi_drivers[i], nullptr);
            _exit(0);
        }
        if (p > 0) waitpid(p, nullptr, 0);
    }

    // ── Universal Multimedia Audio Driver Auto-Probing (Bare Metal & VM) ──
    // Enumerate PCI devices with class 0403 (Multimedia Audio Controller).
    // Dynamically query each controller's kernel MODALIAS and invoke modprobe.
    // This provides unified driver loading for both bare metal (snd-hda-intel, SOF)
    // and virtual environments (virtio_snd) without environment-specific branching.
    if (std::filesystem::exists("/sys/bus/pci/devices")) {
        tinexus::log::info("Probing PCI multimedia audio controllers (class 0403)...");
        for (const auto& dev_entry : std::filesystem::directory_iterator("/sys/bus/pci/devices")) {
            auto class_file = dev_entry.path() / "class";
            if (!std::filesystem::exists(class_file)) continue;

            std::ifstream c_ifs(class_file);
            std::string class_val;
            if (c_ifs >> class_val) {
                // PCI audio controllers have base class 0x04, sub-class 0x03 (e.g. 0x040300)
                if (class_val.rfind("0x0403", 0) == 0 || class_val.find("0403") != std::string::npos) {
                    auto modalias_file = dev_entry.path() / "modalias";
                    if (std::filesystem::exists(modalias_file)) {
                        std::ifstream m_ifs(modalias_file);
                        std::string modalias;
                        if (m_ifs >> modalias && !modalias.empty()) {
                            tinexus::log::info("Loading audio driver for PCI device {} (class={}, modalias={})...",
                                                dev_entry.path().filename().string(), class_val, modalias);
                            pid_t mp = fork();
                            if (mp == 0) {
                                execl("/sbin/modprobe", "modprobe", "-q", modalias.c_str(), nullptr);
                                execl("/usr/sbin/modprobe", "modprobe", "-q", modalias.c_str(), nullptr);
                                execl("/bin/modprobe", "modprobe", "-q", modalias.c_str(), nullptr);
                                execl("/usr/bin/modprobe", "modprobe", "-q", modalias.c_str(), nullptr);
                                execlp("modprobe", "modprobe", "-q", modalias.c_str(), nullptr);
                                _exit(0);
                            }
                            if (mp > 0) waitpid(mp, nullptr, 0);
                        }
                    }
                }
            }
        }
    }

    // ── Universal Child Audio Codec & Platform Driver Auto-Probing ──
    // Once the audio controllers are probed, dynamically query any discovered
    // codecs on the HDA bus (/sys/bus/hdaudio/devices) and audio machine platform
    // devices (/sys/bus/platform/devices). This discovers and binds the exact
    // hardware codecs (Realtek, Conexant, Cirrus, HDMI, etc.) across different
    // laptop, desktop, and VM systems without hardcoding any vendor or card IDs.
    auto probe_devices_in_bus = [](const std::string& bus_path, const std::string& bus_label) {
        if (!std::filesystem::exists(bus_path)) return;
        tinexus::log::info("Probing dynamic {} devices in {}...", bus_label, bus_path);
        for (const auto& entry : std::filesystem::directory_iterator(bus_path)) {
            auto modalias_file = entry.path() / "modalias";
            if (std::filesystem::exists(modalias_file)) {
                std::ifstream m_ifs(modalias_file);
                std::string modalias;
                if (m_ifs >> modalias && !modalias.empty()) {
                    tinexus::log::info("Loading driver for {} device {} (modalias={})...",
                                       bus_label, entry.path().filename().string(), modalias);
                    pid_t mp = fork();
                    if (mp == 0) {
                        execl("/sbin/modprobe", "modprobe", "-q", modalias.c_str(), nullptr);
                        execl("/usr/sbin/modprobe", "modprobe", "-q", modalias.c_str(), nullptr);
                        execl("/bin/modprobe", "modprobe", "-q", modalias.c_str(), nullptr);
                        execl("/usr/bin/modprobe", "modprobe", "-q", modalias.c_str(), nullptr);
                        execlp("modprobe", "modprobe", "-q", modalias.c_str(), nullptr);
                        _exit(0);
                    }
                    if (mp > 0) waitpid(mp, nullptr, 0);
                }
            }
        }
    };

    probe_devices_in_bus("/sys/bus/hdaudio/devices", "HDA codec");
    probe_devices_in_bus("/sys/bus/platform/devices", "platform audio");

    tinexus::log::info("--- CHECKING /dev/input NODES ---");
    if (std::filesystem::exists("/dev/input")) {
        for (const auto& entry : std::filesystem::directory_iterator("/dev/input")) {
            tinexus::log::info("Found input node: {}", entry.path().string());
            run_cmd("/usr/bin/udevadm", {
                const_cast<char*>("/usr/bin/udevadm"),
                const_cast<char*>("info"),
                const_cast<char*>("--query=property"),
                const_cast<char*>("--name"),
                const_cast<char*>(entry.path().string().c_str()),
                nullptr
            });
        }
    } else {
        tinexus::log::error("/dev/input directory does NOT exist!");
    }

    tinexus::log::info("--- CHECKING /dev/dri NODES ---");
    if (std::filesystem::exists("/dev/dri")) {
        for (const auto& entry : std::filesystem::directory_iterator("/dev/dri")) {
            tinexus::log::info("Found DRI node: {}", entry.path().string());
            run_cmd("/usr/bin/udevadm", {
                const_cast<char*>("/usr/bin/udevadm"),
                const_cast<char*>("info"),
                const_cast<char*>("--query=property"),
                const_cast<char*>("--name"),
                const_cast<char*>(entry.path().string().c_str()),
                nullptr
            });
        }
    } else {
        tinexus::log::error("/dev/dri directory does NOT exist!");
    }

    tinexus::log::info("--- CHECKING /sys/class/drm ---");
    if (std::filesystem::exists("/sys/class/drm")) {
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/drm")) {
            tinexus::log::info("Found DRM node: {}", entry.path().filename().string());
            std::filesystem::path status_file = entry.path() / "status";
            if (std::filesystem::exists(status_file)) {
                std::ifstream ifs(status_file);
                std::string conn_status;
                if (ifs >> conn_status) {
                    tinexus::log::info("  Status of {}: {}", entry.path().filename().string(), conn_status);
                }
            }
        }
    } else {
        tinexus::log::error("/sys/class/drm directory does NOT exist!");
    }
}

void setup_default_audio_routing() {
    // Dynamically wait up to 3 seconds for asynchronous sound card creation to settle
    auto cards = tinexus::hardware::AudioUtils::get_sound_cards();
    for (int retry = 0; retry < 15 && cards.empty(); ++retry) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        cards = tinexus::hardware::AudioUtils::get_sound_cards();
    }

    if (cards.empty()) {
        tinexus::log::info("No sound cards detected on this system — skipping ALSA routing.");
        return;
    }

    int card_id = tinexus::hardware::AudioUtils::detect_primary_card_id();
    tinexus::log::info("Configuring default ALSA sound routing to Card {}", card_id);

    // Generate /etc/asound.conf so all applications default to the detected primary analog audio card
    std::ofstream ofs("/etc/asound.conf");
    if (ofs.is_open()) {
        ofs << "defaults.pcm.card " << card_id << "\n";
        ofs << "defaults.ctl.card " << card_id << "\n";
        ofs.close();
        tinexus::log::info("Generated /etc/asound.conf with defaults.pcm.card = {}", card_id);
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

    // 1. Backlight / Brightness
    int b_pct = cfg.brightness;
    tinexus::hardware::BacklightUtils::set_brightness_percent(b_pct, /*persist=*/false, /*throttle=*/false);
    tinexus::log::info("Applied display brightness: {}%", b_pct);

    // 2. Audio Volume & Unmute (if audio hardware is present)
    auto cards = tinexus::hardware::AudioUtils::get_sound_cards();
    if (!cards.empty()) {
        int v_pct = cfg.volume;
        bool is_muted = cfg.muted;
        int card_id = tinexus::hardware::AudioUtils::detect_primary_card_id();
        std::string card_str = std::to_string(card_id);
        std::string ctrl = tinexus::hardware::AudioUtils::detect_primary_control();
        tinexus::log::info("Detected primary ALSA control on card {}: {}", card_id, ctrl);
        tinexus::hardware::AudioUtils::set_volume_percent(v_pct, /*persist=*/false, /*throttle=*/false);

        if (is_muted) {
            pid_t p = fork();
            if (p == 0) {
                execl("/usr/bin/amixer", "amixer", "-c", card_str.c_str(), "sset", ctrl.c_str(), "mute", "-q", nullptr);
                _exit(0);
            }
            if (p > 0) waitpid(p, nullptr, 0);
        } else {
            pid_t p = fork();
            if (p == 0) {
                execl("/usr/bin/amixer", "amixer", "-c", card_str.c_str(), "sset", ctrl.c_str(), "unmute", "-q", nullptr);
                _exit(0);
            }
            if (p > 0) waitpid(p, nullptr, 0);
        }
        tinexus::log::info("Applied audio volume: {}% on card {} (muted={})", v_pct, card_id, is_muted);
    }
}

void signal_handler(int signal) {
    if (signal == SIGCHLD) {
        int status = 0;
        pid_t pid = 0;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (g_pm) {
                int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                int term_sig = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
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
    tinexus::log::info("Starting Platform Runtime Manager v{} (PID 1 Service Authority)...", tinexus::VERSION_STRING);

    // ── Boot-Time UID Validation (PID 1 Panic Prevention) ──
    struct passwd* pw = getpwuid(1000);
    if (!pw || std::string(pw->pw_name) != "tinexus") {
        tinexus::log::error("FATAL: UID 1000 does not map to 'tinexus' user in /etc/passwd.");
        tinexus::log::error("Dropping to emergency root shell. System halt.");
        pid_t rescue = fork();
        if (rescue == 0) {
            execl("/bin/sh", "sh", nullptr);
            _exit(127);
        }
        while (true) pause();
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGCHLD, signal_handler);

    // Basic Environment Setup for PID 1 Session
    setenv("HOME", "/root", 1);
    setenv("USER", "root", 1);
    setenv("LOGNAME", "root", 1);
    setenv("SHELL", "/bin/sh", 1);
    setenv("PATH", "/usr/bin:/usr/sbin:/bin:/sbin", 1);
    setenv("XDG_RUNTIME_DIR", "/run/user/0", 1);
    setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/0/bus", 1);
    // C.UTF-8 is a POSIX-standard locale requiring NO locale-gen and NO locale
    // archive — it enables full UTF-8 encoding on top of the C locale.
    // foot and other terminal apps require a UTF-8 locale or they refuse to start.
    setenv("LANG",   "C.UTF-8", 1);
    setenv("LC_ALL", "C.UTF-8", 1);

    // Generic Mesa / GLVND / GBM driver search paths for rootfs
    setenv("LIBGL_DRIVERS_PATH", "/usr/lib/x86_64-linux-gnu/dri:/usr/lib/dri", 1);
    setenv("GBM_BACKENDS_PATH", "/usr/lib/x86_64-linux-gnu/gbm", 1);
    setenv("__EGL_VENDOR_LIBRARY_DIRS", "/usr/share/glvnd/egl_vendor.d", 1);

    // Early Universal Hardware Discovery:
    // Start udevd and trigger coldplug BEFORE evaluating graphics capabilities so that
    // kernel drivers (xe, i915, amdgpu, etc.) and DRM device nodes (/dev/dri/card*) are populated.
    run_udev_setup();

    // Wlroots compositor environment
    // WLR_DRM_NO_ATOMIC: Disable DRM atomic commits — virtio-gpu (QEMU) does not
    // support non-blocking atomic commits reliably, causing "Device or resource busy" errors.
    // Legacy commit path (setcrtc/setplane) is stable in QEMU.
    setenv("WLR_DRM_NO_ATOMIC", "1", 1);
    setenv("WLR_NO_HARDWARE_CURSORS", "1", 1);

    // Dynamic runtime graphics capability evaluation:
    // Pre-flight check: probe candidate GPU hardware stack on primary KMS card node.
    // If fully proven usable: set WLR_RENDERER=gles2 and WLR_DRM_DEVICES to the verified card node.
    // If probe is uncertain / non-Intel: leave WLR_RENDERER and WLR_DRM_DEVICES UNSET so wlroots
    // performs its native KMS auto-discovery and attempts GLES2 before any software fallback.
    auto probe_result = tinexus::hardware::GraphicsProbe::evaluate();
    if (probe_result.hardware_available) {
        setenv("WLR_RENDERER", "gles2", 1);
        if (!probe_result.selected_card_node.empty()) {
            setenv("WLR_DRM_DEVICES", probe_result.selected_card_node.c_str(), 1);
            tinexus::log::info("[graphics] Bound WLR_DRM_DEVICES to verified primary card: {}", probe_result.selected_card_node);
        }
        tinexus::log::info("[graphics] Hardware renderer capability probe succeeded (EGL {})", probe_result.egl_version);
        tinexus::log::info("[graphics] Using hardware renderer (gles2)");
    } else {
        unsetenv("WLR_RENDERER");
        unsetenv("WLR_DRM_DEVICES");
        tinexus::log::warn("[graphics] Authoritative pre-flight probe did not lock hardware: {}", probe_result.reason);
        tinexus::log::info("[graphics] Leaving WLR_RENDERER and WLR_DRM_DEVICES unset for wlroots native autocreate");
    }
    // Use 'builtin' standalone libseat backend for root compositors (opens physical DRM & input devices)
    setenv("LIBSEAT_BACKEND", "builtin", 1);
    setenv("WLR_LOG_LEVEL", "DEBUG", 1);

    // Ensure dbus directories exist (since /run and /var are tmpfs mounts)
    try {
        std::filesystem::create_directories("/run/dbus");
        std::filesystem::create_directories("/var/lib/dbus");
        std::filesystem::create_directories("/var/run/dbus"); // For legacy paths
        
        // Ensure messagebus user owns them
        // User messagebus has UID 104 in our rootfs
        chown("/run/dbus", 104, 104);
        chown("/var/lib/dbus", 104, 104);
        chown("/var/run/dbus", 104, 104);
    } catch (const std::exception& e) {
        tinexus::log::error("Failed to create D-Bus directories: {}", e.what());
    }

    // Generate machine-id if missing
    if (!std::filesystem::exists("/var/lib/dbus/machine-id") && !std::filesystem::exists("/etc/machine-id")) {
        tinexus::log::info("Generating D-Bus machine-id...");
        pid_t p = fork();
        if (p == 0) {
            execl("/usr/bin/dbus-uuidgen", "dbus-uuidgen", "--ensure", nullptr);
            _exit(127);
        } else if (p > 0) {
            waitpid(p, nullptr, 0);
        }
    }

    // Ensure XDG_RUNTIME_DIR exists with correct permissions so unprivileged apps can traverse it
    try {
        std::filesystem::create_directories("/run/user/0");
        chmod("/run/user/0", 0755);
    } catch (const std::exception& e) {
        tinexus::log::error("Failed to create /run/user/0: {}", e.what());
    }

    // ── Phase 1.5 AppImage Cleanup Sweep ──
    // If tinexus-serviced crashed or a third-party app was SIGKILLed, its temporary
    // AppImage extraction directory might have been left behind. We sweep them on boot
    // from common tmpfs locations to prevent OOM leaks over time.
    try {
        for (const auto& path : {"/tmp", "/run/user/0", "/run/user/1000"}) {
            if (std::filesystem::exists(path)) {
                for (const auto& entry : std::filesystem::directory_iterator(path)) {
                    if (entry.is_directory() && entry.path().filename().string().starts_with("appimage_extract_")) {
                        tinexus::log::warn("Sweeping stale AppImage extract dir: {}", entry.path().string());
                        std::filesystem::remove_all(entry.path());
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        tinexus::log::error("Failed AppImage sweep: {}", e.what());
    }

    // ── Non-Blocking Userspace Network Bring-up (lo + insmod/modprobe + udhcpc) ──
    // POLICY BOUNDARY: Core platform daemons (tinexus-comp, tinexus-searchd,
    // tinexus-serviced, tinexus-ipcd) strictly NEVER open network connections.
    // DHCP/DNS staging is purely for user-space client apps (browser, curl, AppImages).
    {
        pid_t net_bringup_pid = fork();
        if (net_bringup_pid == 0) {
            // Child: load drivers and configure networking in the background
            for (const char* mod : {
                "/lib/modules/failover.ko",
                "/lib/modules/net_failover.ko",
                "/lib/modules/virtio_net.ko",
                "/lib/modules/e1000.ko",
                "/lib/modules/e1000e.ko",
                "/lib/modules/r8169.ko"
            }) {
                if (std::filesystem::exists(mod)) {
                    pid_t p = fork();
                    if (p == 0) {
                        execl("/bin/busybox", "busybox", "insmod", mod, nullptr);
                        execl("/sbin/insmod", "insmod", mod, nullptr);
                        _exit(0);
                    }
                    if (p > 0) waitpid(p, nullptr, 0);
                }
            }

            // Unblock wireless radios before interface scan
            pid_t rfk_p = fork();
            if (rfk_p == 0) {
                execl("/usr/sbin/rfkill", "rfkill", "unblock", "all", nullptr);
                execl("/bin/rfkill", "rfkill", "unblock", "all", nullptr);
                _exit(0);
            }
            if (rfk_p > 0) waitpid(rfk_p, nullptr, 0);

            // Bring up loopback
            pid_t lo_p = fork();
            if (lo_p == 0) {
                execl("/bin/ip", "ip", "link", "set", "lo", "up", nullptr);
                execl("/sbin/ifconfig", "ifconfig", "lo", "127.0.0.1", "up", nullptr);
                _exit(0);
            }
            if (lo_p > 0) waitpid(lo_p, nullptr, 0);

            // Bring up discovered physical/virtual interfaces and start udhcpc
            try {
                if (std::filesystem::exists("/sys/class/net")) {
                    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net")) {
                        std::string iface = entry.path().filename().string();
                        if (iface == "lo") continue;

                        pid_t up_p = fork();
                        if (up_p == 0) {
                            execl("/bin/ip", "ip", "link", "set", iface.c_str(), "up", nullptr);
                            execl("/sbin/ifconfig", "ifconfig", iface.c_str(), "up", nullptr);
                            _exit(0);
                        }
                        if (up_p > 0) waitpid(up_p, nullptr, 0);

                        pid_t dhcp_p = fork();
                        if (dhcp_p == 0) {
                            execl("/bin/busybox", "busybox", "udhcpc", "-b", "-i", iface.c_str(), "-s", "/usr/share/udhcpc/default.script", "-q", nullptr);
                            execl("/sbin/udhcpc", "udhcpc", "-b", "-i", iface.c_str(), "-s", "/usr/share/udhcpc/default.script", "-q", nullptr);
                            execl("/bin/udhcpc", "udhcpc", "-b", "-i", iface.c_str(), "-s", "/usr/share/udhcpc/default.script", "-q", nullptr);
                            _exit(0);
                        }
                    }
                }
            } catch (...) {}

            _exit(0);
        }
    }

    // ── Launch splash screen immediately — fills the framebuffer before Wayland ──
    // Splash runs on /dev/fb0 independently of the compositor. We kill it with
    // SIGTERM once wayland-0 is up so the compositor's first frame takes over.
    pid_t splash_pid = -1;
    {
        pid_t pid = fork();
        if (pid == 0) {
            // Child: exec splash. No WAYLAND_DISPLAY needed — uses /dev/fb0 directly.
            execlp("tinexus-splash", "tinexus-splash", nullptr);
            execl("/usr/bin/tinexus-splash", "tinexus-splash", nullptr);
            _exit(127);
        } else if (pid > 0) {
            splash_pid = pid;
            tinexus::log::info("Splash screen started (PID={}).", splash_pid);
        } else {
            tinexus::log::warn("Failed to fork tinexus-splash — boot will show black screen.");
        }
    }

    // Default Platform Supervision Graph
    tinexus::serviced::DependencyGraph graph;

    // Phase A: Simplified Desktop Bring-up graph
    tinexus::serviced::DaemonSpec dbus;
    dbus.id = "dbus-daemon";
    dbus.executable = "/usr/bin/dbus-daemon";
    dbus.arguments = {"--system", "--nofork", "--nopidfile", "--nosyslog"};
    dbus.critical = true;
    graph.add_service(dbus);

    tinexus::serviced::DaemonSpec comp;
    comp.id = "comp";
    comp.executable = "tinexus-comp";
    comp.critical = true;
    graph.add_service(comp);

    tinexus::serviced::DaemonSpec session;
    session.id = "session";
    session.executable = "tinexus-session";
    session.hard_dependencies = {"comp"};
    session.critical = true;
    graph.add_service(session);

    if (graph.has_cycle()) {
        tinexus::log::error("FATAL: Circular dependency detected in supervision tree!");
        if (splash_pid > 0) kill(splash_pid, SIGTERM);
        return 1;
    }

    tinexus::serviced::ProcessManager pm(graph);
    g_pm = &pm;

    tinexus::serviced::RuntimeControlSocket runtime_sock(pm);
    g_socket = &runtime_sock;


    if (!runtime_sock.start()) {
        tinexus::log::error("Failed to start Runtime Control Socket");
        if (splash_pid > 0) kill(splash_pid, SIGTERM);
        return 1;
    }

    tinexus::log::info("Platform Runtime Manager ready. Auto-spawning supervision tree...");
    restore_hardware_state();
    
    // First, explicitly start dbus-daemon so we can poll for its socket
    pm.start_service("dbus-daemon");

    // Wait up to 5 seconds for the D-Bus system socket.
    // dbus-daemon may write to either /run/dbus or /var/run/dbus depending on
    // the host configuration compiled into the binary; check both.
    tinexus::log::info("Waiting for D-Bus system socket...");
    bool dbus_ready = false;
    const char* dbus_socket_path = nullptr;
    for (int tries = 0; tries < 50; ++tries) {
        if (std::filesystem::exists("/run/dbus/system_bus_socket")) {
            dbus_socket_path = "/run/dbus/system_bus_socket";
            dbus_ready = true;
            break;
        }
        if (std::filesystem::exists("/var/run/dbus/system_bus_socket")) {
            dbus_socket_path = "/var/run/dbus/system_bus_socket";
            dbus_ready = true;
            break;
        }
        usleep(100'000); // 100ms
    }

    if (!dbus_ready) {
        tinexus::log::error("FATAL: dbus-daemon failed to create system_bus_socket within 5 seconds.");
        tinexus::log::error("Checked: /run/dbus/system_bus_socket and /var/run/dbus/system_bus_socket");
        tinexus::log::error("Dependent D-Bus services will fail to connect. Failing loud.");
        if (splash_pid > 0) kill(splash_pid, SIGTERM);
        return 1;
    }

    // Export system bus address so all child processes inherit it automatically.
    std::string dbus_addr = std::string("unix:path=") + dbus_socket_path;
    setenv("DBUS_SYSTEM_BUS_ADDRESS", dbus_addr.c_str(), 1);
    tinexus::log::info("DBUS_SYSTEM_BUS_ADDRESS={}", dbus_addr);
    
    tinexus::log::info("D-Bus system socket is ready. Starting LogindMimic...");
    if (!tinexus::serviced::LogindMimic::instance().start()) {
        tinexus::log::error("Failed to start LogindMimic on system bus!");
    }

    pm.start_all_services(); // starts comp, session, etc.

    // ── Wait for wayland-0 socket, then dismiss splash ──────────────────────────
    // tinexus-comp writes the socket; once it exists the compositor is rendering.
    // Killing splash here minimises the fb0→Wayland black gap to ≤1 frame (~16ms).
    {
        const std::filesystem::path wayland_sock("/run/user/0/wayland-0");
        tinexus::log::info("Waiting for Wayland socket (wayland-0)...");
        for (int tries = 0; tries < 100; ++tries) { // up to 10 seconds
            if (std::filesystem::exists(wayland_sock)) {
                tinexus::log::info("wayland-0 socket is ready.");
                break;
            }
            usleep(100'000); // 100ms
        }
        if (splash_pid > 0) {
            kill(splash_pid, SIGTERM);
            waitpid(splash_pid, nullptr, 0); // reap immediately — don't leave zombie
            tinexus::log::info("Splash screen dismissed.");
        }
    }

    tinexus::serviced::IpcdClient::instance().start();

    runtime_sock.run_accept_loop();
    return 0;
}
