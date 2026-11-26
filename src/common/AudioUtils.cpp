#include "common/AudioUtils.hpp"
#include "common/HardwareConfig.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <cerrno>
#include <cstdio>
#include <memory>
#include <array>

namespace fs = std::filesystem;

namespace tinexus::hardware {

namespace {
static int s_cached_volume = -1;
static bool s_cached_muted = false;
static std::string s_detected_control{};
static std::chrono::steady_clock::time_point s_last_audio_write{};

// Inject PipeWire / PulseAudio / D-Bus user runtime env cleanly for child audio tools
void inject_runtime_audio_env() {
    const std::string fallback_rt = "/run/user/" + std::to_string(::getuid());
    const char* rt_env = std::getenv("XDG_RUNTIME_DIR");
    const std::string rt = (rt_env && *rt_env) ? rt_env : fallback_rt;
    ::setenv("XDG_RUNTIME_DIR", rt.c_str(), 0);
    const std::string pulse_addr = "unix:" + rt + "/pulse/native";
    ::setenv("PULSE_SERVER", pulse_addr.c_str(), 0);
    const std::string dbus_addr = "unix:path=" + rt + "/bus";
    ::setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr.c_str(), 0);
}

// Safe non-shell subprocess stdout capture with bounded timeout (no popen() / /bin/sh)
std::string run_cmd_output(const char* bin, const std::vector<std::string>& args, int timeout_ms = 1500, int* exit_code_out = nullptr) {
    if (!bin || !fs::exists(bin)) {
        if (exit_code_out) *exit_code_out = 127;
        return "";
    }
    int pipefd[2]{-1, -1};
    if (::pipe2(pipefd, O_CLOEXEC) != 0) {
        if (exit_code_out) *exit_code_out = 1;
        return "";
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        if (exit_code_out) *exit_code_out = 1;
        return "";
    }
    if (pid == 0) {
        ::close(pipefd[0]);
        ::dup2(pipefd[1], STDOUT_FILENO);
        int devnull = ::open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            ::dup2(devnull, STDERR_FILENO);
            if (devnull > 2) ::close(devnull);
        }
        if (pipefd[1] > 2) ::close(pipefd[1]);

        inject_runtime_audio_env();

        std::vector<char*> c_args;
        c_args.reserve(args.size() + 2);
        c_args.push_back(const_cast<char*>(bin));
        for (const auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);
        ::execv(bin, c_args.data());
        _exit(127);
    }

    ::close(pipefd[1]);
    int flags = ::fcntl(pipefd[0], F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
    }

    std::string result;
    std::array<char, 256> buffer{};
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    bool eof = false;

    while (!eof) {
        auto now = std::chrono::steady_clock::now();
        int remaining = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        if (remaining <= 0) {
            ::kill(pid, SIGKILL);
            break;
        }
        struct pollfd pfd{};
        pfd.fd = pipefd[0];
        pfd.events = POLLIN;
        int pr = ::poll(&pfd, 1, remaining);
        if (pr > 0) {
            while (true) {
                ssize_t n = ::read(pipefd[0], buffer.data(), buffer.size());
                if (n > 0) {
                    result.append(buffer.data(), static_cast<size_t>(n));
                } else if (n == 0) {
                    eof = true;
                    break;
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else if (errno == EINTR) {
                    continue;
                } else {
                    eof = true;
                    break;
                }
            }
        } else if (pr == 0) {
            ::kill(pid, SIGKILL);
            break;
        } else if (errno != EINTR) {
            break;
        }
    }

    ::close(pipefd[0]);
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (exit_code_out) {
        *exit_code_out = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    return result;
}

bool run_cmd_status(const char* bin, const std::vector<std::string>& args, int timeout_ms = 1500) {
    int rc = 1;
    (void)run_cmd_output(bin, args, timeout_ms, &rc);
    return rc == 0;
}

void run_cmd_async(const char* bin, const std::vector<std::string>& args) {
    // Bounded subprocess execution (1500ms) so cold-starting audio daemons complete reliably.
    (void)run_cmd_output(bin, args, 1500);
}

struct PactlNode {
    std::string id;
    std::string name;
    std::string description;
};

std::string prettify_pa_name(std::string label) {
    if (label.rfind("alsa_output.", 0) == 0) {
        label = label.substr(12);
    } else if (label.rfind("alsa_input.", 0) == 0) {
        label = label.substr(11);
    }
    std::replace(label.begin(), label.end(), '_', ' ');
    std::replace(label.begin(), label.end(), '.', ' ');
    return label;
}

std::vector<PactlNode> parse_pactl_nodes(const char* kind_long, const char* kind_short, bool skip_monitors = false) {
    std::vector<PactlNode> nodes;
    if (!fs::exists("/usr/bin/pactl")) return nodes;

    std::string detail = run_cmd_output("/usr/bin/pactl", {"list", kind_long}, 1500);
    if (!detail.empty()) {
        std::istringstream iss(detail);
        std::string line;
        PactlNode cur;
        bool in_node = false;
        auto flush = [&]() {
            if (!in_node || cur.name.empty()) return;
            if (skip_monitors && cur.name.find(".monitor") != std::string::npos) return;
            if (cur.description.empty()) {
                cur.description = prettify_pa_name(cur.name);
            }
            nodes.push_back(cur);
        };
        while (std::getline(iss, line)) {
            auto start = line.find_first_not_of(" \t\r");
            if (start == std::string::npos) continue;
            std::string trimmed = line.substr(start);
            if (trimmed.rfind("Sink #", 0) == 0 || trimmed.rfind("Source #", 0) == 0) {
                flush();
                cur = PactlNode{};
                auto hash = trimmed.find('#');
                cur.id = (hash != std::string::npos) ? trimmed.substr(hash + 1) : "";
                in_node = true;
            } else if (in_node && trimmed.rfind("Name:", 0) == 0) {
                std::string val = trimmed.substr(5);
                auto s = val.find_first_not_of(" \t\r");
                cur.name = (s != std::string::npos) ? val.substr(s) : "";
            } else if (in_node && trimmed.rfind("Description:", 0) == 0) {
                std::string val = trimmed.substr(12);
                auto s = val.find_first_not_of(" \t\r");
                cur.description = (s != std::string::npos) ? val.substr(s) : "";
            }
        }
        flush();
    }

    if (nodes.empty()) {
        std::string out = run_cmd_output("/usr/bin/pactl", {"list", "short", kind_short}, 1500);
        std::istringstream iss(out);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            std::istringstream liness(line);
            std::string idx, name;
            if (liness >> idx >> name) {
                if (skip_monitors && name.find(".monitor") != std::string::npos) continue;
                nodes.push_back({idx, name, prettify_pa_name(name)});
            }
        }
    }
    return nodes;
}

std::string trim_whitespace(std::string s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}
} // namespace

std::vector<AudioCardInfo> AudioUtils::get_sound_cards(const std::string& base_proc_asound) {
    std::vector<AudioCardInfo> cards;
    fs::path cards_file = fs::path(base_proc_asound) / "cards";
    if (!fs::exists(cards_file)) return cards;

    std::ifstream ifs(cards_file);
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        auto bracket_open = line.find('[');
        auto bracket_close = line.find("]:");
        if (bracket_open == std::string::npos || bracket_close == std::string::npos) {
            continue; // Skip continuation line
        }

        std::string num_str = line.substr(0, bracket_open);
        std::istringstream iss(num_str);
        int id = 0;
        if (iss >> id) {
            std::string desc = line.substr(bracket_close + 2);
            auto start = desc.find_first_not_of(" \t");
            if (start != std::string::npos) desc = desc.substr(start);

            AudioCardInfo info;
            info.id = id;
            info.name = desc.empty() ? ("Audio Card " + std::to_string(id)) : desc;
            cards.push_back(info);
        }
    }
    return cards;
}

int AudioUtils::detect_primary_card_id(const std::string& base_proc_asound) {
    auto cards = get_sound_cards(base_proc_asound);
    if (cards.empty()) return 0;
    if (cards.size() == 1) return cards[0].id;

    // Filter out discrete GPU / digital HDMI/DisplayPort audio cards
    // Priority: Return the first analog card (PCH, Realtek, SOF, Conexant, Generic, VirtIO)
    for (const auto& card : cards) {
        std::string lower = card.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("nvidia") == std::string::npos &&
            lower.find("hdmi") == std::string::npos &&
            lower.find("displayport") == std::string::npos) {
            return card.id;
        }
    }
    return cards[0].id;
}

std::string AudioUtils::detect_primary_control(const std::string& base_proc_asound) {
    if (!s_detected_control.empty()) {
        return s_detected_control;
    }

    if (!fs::exists("/usr/bin/amixer")) {
        s_detected_control = "Master";
        return s_detected_control;
    }

    int card_id = detect_primary_card_id(base_proc_asound);
    std::string scontrols = run_cmd_output("/usr/bin/amixer", {"-c", std::to_string(card_id), "scontrols"});
    // Priority order: Master -> Speaker -> Headphone -> PCM -> Front
    for (const char* candidate : {"'Master'", "'Speaker'", "'Headphone'", "'PCM'", "'Front'"}) {
        if (scontrols.find(candidate) != std::string::npos) {
            std::string name = candidate;
            name = name.substr(1, name.length() - 2); // remove quotes
            s_detected_control = name;
            return s_detected_control;
        }
    }

    s_detected_control = "Master";
    return s_detected_control;
}

int AudioUtils::get_volume_percent(const std::string& base_proc_asound) {
    // 1. Query PipeWire (wpctl) if active (hardware-agnostic sink status)
    if (fs::exists("/usr/bin/wpctl")) {
        std::string wp_out = run_cmd_output("/usr/bin/wpctl", {"get-volume", "@DEFAULT_AUDIO_SINK@"});
        if (!wp_out.empty() && wp_out.find("Volume:") != std::string::npos) {
            std::istringstream iss(wp_out);
            std::string tag;
            double vol = 0.0;
            if (iss >> tag >> vol) {
                s_cached_volume = std::clamp(static_cast<int>(vol * 100.0 + 0.5), 0, 100);
                s_cached_muted = (wp_out.find("[MUTED]") != std::string::npos) || (s_cached_volume == 0);
                return s_cached_volume;
            }
        }
    }

    std::string ctrl = detect_primary_control(base_proc_asound);
    int card_id = detect_primary_card_id(base_proc_asound);

    if (fs::exists("/usr/bin/amixer")) {
        std::string out = run_cmd_output("/usr/bin/amixer", {"-c", std::to_string(card_id), "sget", ctrl});

        if (!out.empty()) {
            bool found_on = false;
            bool found_off = false;
            std::istringstream iss(out);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.find("Playback") != std::string::npos) {
                    auto pct_pos = line.find('%');
                    if (pct_pos != std::string::npos) {
                        auto bracket_open = line.rfind('[', pct_pos);
                        if (bracket_open != std::string::npos) {
                            try {
                                int val = std::stoi(line.substr(bracket_open + 1, pct_pos - bracket_open - 1));
                                s_cached_volume = std::clamp(val, 0, 100);
                            } catch (...) {}
                        }
                    }
                    if (line.find("[on]") != std::string::npos) {
                        found_on = true;
                    }
                    if (line.find("[off]") != std::string::npos) {
                        found_off = true;
                    }
                }
            }

            if (found_on) {
                s_cached_muted = (s_cached_volume == 0);
            } else if (found_off) {
                s_cached_muted = true;
            } else if (out.find("[off]") != std::string::npos) {
                s_cached_muted = true;
            }

            if (s_cached_volume >= 0) {
                return s_cached_volume;
            }
        }
    }

    if (s_cached_volume >= 0) {
        return s_cached_volume;
    }

    auto cfg = HardwareConfig::load();
    s_cached_volume = std::clamp(cfg.volume, 0, 100);
    s_cached_muted = cfg.muted;
    return s_cached_volume;
}

bool AudioUtils::set_volume_percent(int pct, bool persist, bool throttle) {
    pct = std::clamp(pct, 0, 100);
    s_cached_volume = pct;
    s_cached_muted = (pct == 0);

    if (throttle) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_audio_write).count();
        if (elapsed < 40) {
            // Coalesce drag events
            return true;
        }
        s_last_audio_write = now;
    } else {
        s_last_audio_write = std::chrono::steady_clock::now();
    }

    // 1. Primary PipeWire / PulseAudio sink synchronization (fast path on drag)
    bool pw_ok = false;
    if (fs::exists("/usr/bin/wpctl")) {
        char vol_buf[16];
        snprintf(vol_buf, sizeof(vol_buf), "%.2f", pct / 100.0);
        bool v_ok = run_cmd_status("/usr/bin/wpctl", {"set-volume", "@DEFAULT_AUDIO_SINK@", vol_buf}, 1000);
        bool m_ok = run_cmd_status("/usr/bin/wpctl", {"set-mute", "@DEFAULT_AUDIO_SINK@", (pct == 0 ? "1" : "0")}, 1000);
        pw_ok = v_ok && m_ok;
    }
    if (!pw_ok && fs::exists("/usr/bin/pactl")) {
        bool v_ok = run_cmd_status("/usr/bin/pactl", {"set-sink-volume", "@DEFAULT_SINK@", std::to_string(pct) + "%"}, 1000);
        bool m_ok = run_cmd_status("/usr/bin/pactl", {"set-sink-mute", "@DEFAULT_SINK@", (pct == 0 ? "1" : "0")}, 1000);
        pw_ok = v_ok || m_ok;
    }

    // 2. ALSA hardware mixer fallback/sync (skip multi-control sweep while actively dragging or when PipeWire already succeeded)
    if (!throttle || !pw_ok) {
        auto cards = get_sound_cards();
        if (!cards.empty() && fs::exists("/usr/bin/amixer")) {
            int card_id = detect_primary_card_id();
            std::string card_str = std::to_string(card_id);
            std::string ctrl = detect_primary_control();

            std::string arg_val = std::to_string(pct) + "%";
            if (pct > 0) {
                run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", ctrl, arg_val, "unmute"});
                if (!throttle) {
                    for (const char* sec_ctrl : {"Speaker", "Headphone", "PCM", "Front"}) {
                        if (std::string(sec_ctrl) != ctrl) {
                            run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", sec_ctrl, "unmute", "-q"});
                        }
                    }
                }
            } else {
                run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", ctrl, "mute"});
            }
        }
    }

    if (persist) {
        auto cfg = HardwareConfig::load();
        cfg.volume = pct;
        cfg.muted = s_cached_muted;
        HardwareConfig::save(cfg);
    }

    return true;
}

bool AudioUtils::is_muted() {
    if (s_cached_volume < 0) {
        get_volume_percent();
    }
    return s_cached_muted;
}

bool AudioUtils::toggle_mute(bool persist) {
    s_cached_muted = !s_cached_muted;

    bool pw_ok = false;
    if (fs::exists("/usr/bin/wpctl")) {
        pw_ok = run_cmd_status("/usr/bin/wpctl", {"set-mute", "@DEFAULT_AUDIO_SINK@", s_cached_muted ? "1" : "0"}, 1000);
    }
    if (!pw_ok && fs::exists("/usr/bin/pactl")) {
        pw_ok = run_cmd_status("/usr/bin/pactl", {"set-sink-mute", "@DEFAULT_SINK@", s_cached_muted ? "1" : "0"}, 1000);
    }

    auto cards = get_sound_cards();
    if (!cards.empty() && fs::exists("/usr/bin/amixer")) {
        int card_id = detect_primary_card_id();
        std::string card_str = std::to_string(card_id);
        std::string ctrl = detect_primary_control();
        run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", ctrl, s_cached_muted ? "mute" : "unmute"});
    }

    if (persist) {
        auto cfg = HardwareConfig::load();
        cfg.muted = s_cached_muted;
        HardwareConfig::save(cfg);
    }

    return s_cached_muted;
}

int AudioUtils::step_volume(int delta_pct, bool persist) {
    int cur = get_volume_percent();
    int target = std::clamp(cur + delta_pct, 0, 100);
    set_volume_percent(target, persist, false);
    return target;
}

std::string AudioUtils::resolve_chime_path(const std::string& preferred_path) {
    if (!preferred_path.empty() && fs::exists(preferred_path)) {
        return preferred_path;
    }
    const std::vector<std::string> candidates = {
        "/usr/share/sounds/tinexus/volume-chime.wav",
        "assets/sounds/volume-chime.wav",
        "../assets/sounds/volume-chime.wav",
        "../../assets/sounds/volume-chime.wav",
        "/workspace/assets/sounds/volume-chime.wav",
        "/mnt/e/Tinu's Technology/Tinexus Manager/assets/sounds/volume-chime.wav"
    };
    for (const auto& c : candidates) {
        if (fs::exists(c)) {
            return c;
        }
    }
    return "";
}

void AudioUtils::play_chime(const std::string& chime_path) {
    std::string actual_path = resolve_chime_path(chime_path);
    if (actual_path.empty()) {
        return;
    }

    // 1. If PipeWire pw-play is available, play asynchronously via PipeWire server
    if (fs::exists("/usr/bin/pw-play")) {
        pid_t pid = fork();
        if (pid == 0) {
            inject_runtime_audio_env();
            execl("/usr/bin/pw-play", "pw-play", actual_path.c_str(), nullptr);
            _exit(127);
        }
        return;
    }

    // 2. If PulseAudio paplay is available, play asynchronously via PulseAudio
    if (fs::exists("/usr/bin/paplay")) {
        pid_t pid = fork();
        if (pid == 0) {
            inject_runtime_audio_env();
            execl("/usr/bin/paplay", "paplay", actual_path.c_str(), nullptr);
            _exit(127);
        }
        return;
    }

    // 3. Fallback to ALSA aplay (using default PCM device from /etc/asound.conf)
    if (fs::exists("/usr/bin/aplay")) {
        pid_t pid = fork();
        if (pid == 0) {
            inject_runtime_audio_env();
            execl("/usr/bin/aplay", "aplay", "-q", actual_path.c_str(), nullptr);
            _exit(127);
        }
    }
}

static int s_current_output_index = 0;

std::vector<std::string> AudioUtils::get_output_devices(const std::string& base_proc_asound) {
    std::vector<std::string> devices;

    // 1. Query runtime PipeWire / PulseAudio sinks if pactl is available
    auto pa_sinks = parse_pactl_nodes("sinks", "sinks", false);
    for (const auto& s : pa_sinks) {
        devices.push_back(s.description);
    }

    // 2. Fallback to wpctl status Sinks section if pactl didn't yield devices
    if (devices.empty() && fs::exists("/usr/bin/wpctl")) {
        std::string wp_status = run_cmd_output("/usr/bin/wpctl", {"status"}, 1500);
        std::istringstream iss(wp_status);
        std::string line;
        bool in_sinks = false;
        while (std::getline(iss, line)) {
            if (line.find("Sinks:") != std::string::npos) {
                in_sinks = true;
                continue;
            }
            if (in_sinks) {
                if (line.find("Sources:") != std::string::npos ||
                    line.find("Filters:") != std::string::npos ||
                    line.find("Streams:") != std::string::npos ||
                    line.find("Video") != std::string::npos) {
                    break;
                }
                auto dot = line.find('.');
                if (dot != std::string::npos) {
                    std::string desc = trim_whitespace(line.substr(dot + 1));
                    auto bracket = desc.rfind("[vol:");
                    if (bracket != std::string::npos) {
                        desc = trim_whitespace(desc.substr(0, bracket));
                    }
                    if (!desc.empty()) devices.push_back(desc);
                }
            }
        }
    }

    // 3. Query physical / virtual ALSA sound cards dynamically from /proc/asound/cards
    if (devices.empty()) {
        auto cards = get_sound_cards(base_proc_asound);
        for (const auto& card : cards) {
            std::string desc = "Card " + std::to_string(card.id) + ": " + card.name;
            devices.push_back(desc);
        }
    }

    // 4. Graceful fallback if no audio hardware exists
    if (devices.empty()) {
        devices.push_back("No Audio Output Devices Available");
    }

    return devices;
}

int AudioUtils::get_current_output_device_index() {
    auto pa_sinks = parse_pactl_nodes("sinks", "sinks", false);
    if (!pa_sinks.empty() && fs::exists("/usr/bin/pactl")) {
        std::string def_sink = trim_whitespace(run_cmd_output("/usr/bin/pactl", {"get-default-sink"}, 1500));
        if (!def_sink.empty()) {
            for (size_t i = 0; i < pa_sinks.size(); ++i) {
                if (pa_sinks[i].name == def_sink) {
                    s_current_output_index = static_cast<int>(i);
                    return s_current_output_index;
                }
            }
        }
    }
    auto devs = get_output_devices();
    if (devs.empty() || devs[0] == "No Audio Output Devices Available") {
        return 0;
    }
    return std::clamp(s_current_output_index, 0, static_cast<int>(devs.size() - 1));
}

bool AudioUtils::set_output_device_by_index(int index) {
    auto devs = get_output_devices();
    if (index < 0 || index >= static_cast<int>(devs.size())) {
        return false;
    }

    s_current_output_index = index;

    // PipeWire / PulseAudio default sink switch
    auto pa_sinks = parse_pactl_nodes("sinks", "sinks", false);
    if (index < static_cast<int>(pa_sinks.size()) && fs::exists("/usr/bin/pactl")) {
        run_cmd_async("/usr/bin/pactl", {"set-default-sink", pa_sinks[static_cast<size_t>(index)].name});
        if (fs::exists("/usr/bin/wpctl")) {
            run_cmd_async("/usr/bin/wpctl", {"set-default", pa_sinks[static_cast<size_t>(index)].id});
        }
    }

    // ALSA default routing update (only if root/writable)
    auto cards = get_sound_cards();
    if (index < static_cast<int>(cards.size())) {
        int card_id = cards[index].id;
        if (::access("/etc/asound.conf", W_OK) == 0 ||
            (!fs::exists("/etc/asound.conf") && ::access("/etc", W_OK) == 0)) {
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
            }
        }
        s_detected_control.clear();
        s_cached_volume = -1;
        get_volume_percent();
    }

    return true;
}

static int s_current_input_index = 0;

std::vector<std::string> AudioUtils::get_input_devices() {
    std::vector<std::string> devices;
    auto pa_sources = parse_pactl_nodes("sources", "sources", /*skip_monitors=*/true);
    for (const auto& s : pa_sources) {
        devices.push_back(s.description);
    }
    if (devices.empty() && fs::exists("/usr/bin/wpctl")) {
        std::string wp_status = run_cmd_output("/usr/bin/wpctl", {"status"}, 1500);
        std::istringstream iss(wp_status);
        std::string line;
        bool in_sources = false;
        while (std::getline(iss, line)) {
            if (line.find("Sources:") != std::string::npos) {
                in_sources = true;
                continue;
            }
            if (in_sources) {
                if (line.find("Filters:") != std::string::npos ||
                    line.find("Streams:") != std::string::npos ||
                    line.find("Video") != std::string::npos ||
                    line.find("Settings") != std::string::npos) {
                    break;
                }
                auto dot = line.find('.');
                if (dot != std::string::npos) {
                    std::string desc = trim_whitespace(line.substr(dot + 1));
                    auto bracket = desc.rfind("[vol:");
                    if (bracket != std::string::npos) {
                        desc = trim_whitespace(desc.substr(0, bracket));
                    }
                    if (!desc.empty()) devices.push_back(desc);
                }
            }
        }
    }
    if (devices.empty()) {
        devices.push_back("No Audio Input Devices Available");
    }
    return devices;
}

int AudioUtils::get_current_input_device_index() {
    auto pa_sources = parse_pactl_nodes("sources", "sources", true);
    if (!pa_sources.empty() && fs::exists("/usr/bin/pactl")) {
        std::string def_src = trim_whitespace(run_cmd_output("/usr/bin/pactl", {"get-default-source"}, 1500));
        if (!def_src.empty()) {
            for (size_t i = 0; i < pa_sources.size(); ++i) {
                if (pa_sources[i].name == def_src) {
                    s_current_input_index = static_cast<int>(i);
                    return s_current_input_index;
                }
            }
        }
    }
    auto devs = get_input_devices();
    if (devs.empty() || devs[0] == "No Audio Input Devices Available") return 0;
    return std::clamp(s_current_input_index, 0, static_cast<int>(devs.size() - 1));
}

bool AudioUtils::set_input_device_by_index(int index) {
    auto pa_sources = parse_pactl_nodes("sources", "sources", true);
    if (index < 0 || index >= static_cast<int>(pa_sources.size())) {
        return false;
    }
    s_current_input_index = index;
    if (fs::exists("/usr/bin/pactl")) {
        run_cmd_async("/usr/bin/pactl", {"set-default-source", pa_sources[static_cast<size_t>(index)].name});
        return true;
    }
    if (fs::exists("/usr/bin/wpctl")) {
        run_cmd_async("/usr/bin/wpctl", {"set-default", pa_sources[static_cast<size_t>(index)].id});
        return true;
    }
    return false;
}

int AudioUtils::get_input_volume_percent() {
    if (fs::exists("/usr/bin/wpctl")) {
        std::string out = run_cmd_output("/usr/bin/wpctl", {"get-volume", "@DEFAULT_AUDIO_SOURCE@"}, 1500);
        if (!out.empty() && out.find("Volume:") != std::string::npos) {
            std::istringstream iss(out);
            std::string tag;
            double vol = 0.0;
            if (iss >> tag >> vol) {
                return std::clamp(static_cast<int>(vol * 100.0 + 0.5), 0, 100);
            }
        }
    }
    if (fs::exists("/usr/bin/pactl")) {
        std::string out = run_cmd_output("/usr/bin/pactl", {"get-source-volume", "@DEFAULT_SOURCE@"}, 1500);
        auto pct_pos = out.find('%');
        if (pct_pos != std::string::npos) {
            auto sp = out.rfind(' ', pct_pos);
            if (sp != std::string::npos) {
                try {
                    return std::clamp(std::stoi(out.substr(sp + 1, pct_pos - sp - 1)), 0, 100);
                } catch (...) {}
            }
        }
    }
    return 75;
}

bool AudioUtils::set_input_volume_percent(int pct) {
    pct = std::clamp(pct, 0, 100);
    if (fs::exists("/usr/bin/wpctl")) {
        char vol_buf[16];
        snprintf(vol_buf, sizeof(vol_buf), "%.2f", pct / 100.0);
        run_cmd_async("/usr/bin/wpctl", {"set-volume", "@DEFAULT_AUDIO_SOURCE@", vol_buf});
        return true;
    }
    if (fs::exists("/usr/bin/pactl")) {
        run_cmd_async("/usr/bin/pactl", {"set-source-volume", "@DEFAULT_SOURCE@", std::to_string(pct) + "%"});
        return true;
    }
    return false;
}

bool AudioUtils::is_input_muted() {
    if (fs::exists("/usr/bin/wpctl")) {
        std::string out = run_cmd_output("/usr/bin/wpctl", {"get-volume", "@DEFAULT_AUDIO_SOURCE@"}, 1500);
        if (!out.empty()) {
            return out.find("[MUTED]") != std::string::npos;
        }
    }
    if (fs::exists("/usr/bin/pactl")) {
        std::string out = run_cmd_output("/usr/bin/pactl", {"get-source-mute", "@DEFAULT_SOURCE@"}, 1500);
        return out.find("yes") != std::string::npos;
    }
    return false;
}

bool AudioUtils::toggle_input_mute() {
    if (fs::exists("/usr/bin/wpctl")) {
        run_cmd_async("/usr/bin/wpctl", {"set-mute", "@DEFAULT_AUDIO_SOURCE@", "toggle"});
    } else if (fs::exists("/usr/bin/pactl")) {
        run_cmd_async("/usr/bin/pactl", {"set-source-mute", "@DEFAULT_SOURCE@", "toggle"});
    }
    return is_input_muted();
}

} // namespace tinexus::hardware
