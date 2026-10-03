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

struct PipeDeleter {
    void operator()(FILE* fp) const { if (fp) pclose(fp); }
};

std::string run_cmd_output(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, PipeDeleter> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return "";
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void run_cmd_async(const char* bin, const std::vector<std::string>& args) {
    if (!fs::exists(bin)) return;
    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(bin));
        for (const auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);
        execv(bin, c_args.data());
        _exit(127);
    } else if (pid > 0) {
        int status = 0;
        waitpid(pid, &status, 0);
    }
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
    std::string scontrols = run_cmd_output("/usr/bin/amixer -c " + std::to_string(card_id) + " scontrols 2>/dev/null");
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
        std::string wp_out = run_cmd_output("/usr/bin/wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null");
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
        std::string cmd = "/usr/bin/amixer -c " + std::to_string(card_id) + " sget " + ctrl + " 2>/dev/null";
        std::string out = run_cmd_output(cmd);

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

    auto cards = get_sound_cards();
    if (!cards.empty() && fs::exists("/usr/bin/amixer")) {
        int card_id = detect_primary_card_id();
        std::string card_str = std::to_string(card_id);
        std::string ctrl = detect_primary_control();

        std::string arg_val = std::to_string(pct) + "%";
        if (pct > 0) {
            // Unmute and set percentage as separate arguments so execv doesn't pass merged strings
            run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", ctrl, arg_val, "unmute"});
            // Simultaneously ensure all constituent analog sinks are unmuted
            for (const char* sec_ctrl : {"Speaker", "Headphone", "PCM", "Front"}) {
                if (std::string(sec_ctrl) != ctrl) {
                    run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", sec_ctrl, "unmute", "-q"});
                }
            }
        } else {
            run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", ctrl, "mute"});
        }
    }

    // PipeWire / WirePlumber / PulseAudio synchronization
    if (fs::exists("/usr/bin/wpctl")) {
        char vol_buf[16];
        snprintf(vol_buf, sizeof(vol_buf), "%.2f", pct / 100.0);
        run_cmd_async("/usr/bin/wpctl", {"set-volume", "@DEFAULT_AUDIO_SINK@", vol_buf});
        run_cmd_async("/usr/bin/wpctl", {"set-mute", "@DEFAULT_AUDIO_SINK@", (pct == 0 ? "1" : "0")});
    } else if (fs::exists("/usr/bin/pactl")) {
        run_cmd_async("/usr/bin/pactl", {"set-sink-volume", "@DEFAULT_SINK@", std::to_string(pct) + "%"});
        run_cmd_async("/usr/bin/pactl", {"set-sink-mute", "@DEFAULT_SINK@", (pct == 0 ? "1" : "0")});
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

    auto cards = get_sound_cards();
    if (!cards.empty() && fs::exists("/usr/bin/amixer")) {
        int card_id = detect_primary_card_id();
        std::string card_str = std::to_string(card_id);
        std::string ctrl = detect_primary_control();
        run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", ctrl, "toggle"});
    }

    if (fs::exists("/usr/bin/wpctl")) {
        run_cmd_async("/usr/bin/wpctl", {"set-mute", "@DEFAULT_AUDIO_SINK@", "toggle"});
    } else if (fs::exists("/usr/bin/pactl")) {
        run_cmd_async("/usr/bin/pactl", {"set-sink-mute", "@DEFAULT_SINK@", "toggle"});
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
            execl("/usr/bin/pw-play", "pw-play", actual_path.c_str(), nullptr);
            _exit(127);
        }
        return;
    }

    // 2. If PulseAudio paplay is available, play asynchronously via PulseAudio
    if (fs::exists("/usr/bin/paplay")) {
        pid_t pid = fork();
        if (pid == 0) {
            execl("/usr/bin/paplay", "paplay", actual_path.c_str(), nullptr);
            _exit(127);
        }
        return;
    }

    // 3. Fallback to ALSA aplay (using default PCM device from /etc/asound.conf)
    if (fs::exists("/usr/bin/aplay")) {
        pid_t pid = fork();
        if (pid == 0) {
            execl("/usr/bin/aplay", "aplay", "-q", actual_path.c_str(), nullptr);
            _exit(127);
        }
    }
}

static int s_current_output_index = 0;

std::vector<std::string> AudioUtils::get_output_devices(const std::string& base_proc_asound) {
    std::vector<std::string> devices;

    // 1. Query runtime PipeWire / PulseAudio sinks if pactl is available
    if (fs::exists("/usr/bin/pactl")) {
        std::string out = run_cmd_output("/usr/bin/pactl list short sinks 2>/dev/null");
        if (!out.empty()) {
            std::istringstream iss(out);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.empty()) continue;
                std::istringstream liness(line);
                std::string idx, name;
                if (liness >> idx >> name) {
                    std::string label = name;
                    if (label.rfind("alsa_output.", 0) == 0) {
                        label = label.substr(12);
                    }
                    std::replace(label.begin(), label.end(), '_', ' ');
                    std::replace(label.begin(), label.end(), '.', ' ');
                    devices.push_back(label);
                }
            }
        }
    }

    // 2. Query physical / virtual ALSA sound cards dynamically from /proc/asound/cards
    if (devices.empty()) {
        auto cards = get_sound_cards(base_proc_asound);
        for (const auto& card : cards) {
            std::string desc = "Card " + std::to_string(card.id) + ": " + card.name;
            devices.push_back(desc);
        }
    }

    // 3. Graceful fallback if no audio hardware exists
    if (devices.empty()) {
        devices.push_back("No Audio Output Devices Available");
    }

    return devices;
}

int AudioUtils::get_current_output_device_index() {
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
    if (fs::exists("/usr/bin/pactl")) {
        std::string out = run_cmd_output("/usr/bin/pactl list short sinks 2>/dev/null");
        std::istringstream iss(out);
        std::string line;
        int i = 0;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            std::istringstream liness(line);
            std::string sink_id, sink_name;
            if (liness >> sink_id >> sink_name) {
                if (i == index) {
                    run_cmd_async("/usr/bin/pactl", {"set-default-sink", sink_name});
                    break;
                }
                i++;
            }
        }
    }

    // ALSA default routing update
    auto cards = get_sound_cards();
    if (index < static_cast<int>(cards.size())) {
        int card_id = cards[index].id;
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
        s_detected_control.clear();
        s_cached_volume = -1;
        get_volume_percent();
    }

    return true;
}

} // namespace tinexus::hardware
