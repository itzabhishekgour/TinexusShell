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
    std::string ctrl = detect_primary_control(base_proc_asound);
    int card_id = detect_primary_card_id(base_proc_asound);

    if (fs::exists("/usr/bin/amixer")) {
        std::string cmd = "/usr/bin/amixer -c " + std::to_string(card_id) + " sget " + ctrl + " 2>/dev/null";
        std::string out = run_cmd_output(cmd);

        if (!out.empty()) {
            // Parse [XX%] and [on]/[off]
            auto pct_pos = out.find('%');
            if (pct_pos != std::string::npos) {
                auto bracket_open = out.rfind('[', pct_pos);
                if (bracket_open != std::string::npos) {
                    try {
                        int val = std::stoi(out.substr(bracket_open + 1, pct_pos - bracket_open - 1));
                        s_cached_volume = std::clamp(val, 0, 100);
                    } catch (...) {}
                }
            }

            if (out.find("[off]") != std::string::npos) {
                s_cached_muted = true;
            } else if (out.find("[on]") != std::string::npos) {
                s_cached_muted = false;
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

    int card_id = detect_primary_card_id();
    std::string card_str = std::to_string(card_id);
    std::string ctrl = detect_primary_control();

    if (fs::exists("/usr/bin/amixer")) {
        std::string arg_val = std::to_string(pct) + "%";
        if (pct > 0) {
            run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", ctrl, (arg_val + " unmute")});
        } else {
            run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", ctrl, "mute"});
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

    int card_id = detect_primary_card_id();
    std::string card_str = std::to_string(card_id);
    std::string ctrl = detect_primary_control();

    if (fs::exists("/usr/bin/amixer")) {
        run_cmd_async("/usr/bin/amixer", {"-c", card_str, "sset", ctrl, "toggle"});
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

void AudioUtils::play_chime(const std::string& chime_path) {
    if (!fs::exists(chime_path) || !fs::exists("/usr/bin/aplay")) {
        return;
    }

    int card_id = detect_primary_card_id();
    std::string dev_str = "plughw:" + std::to_string(card_id) + ",0";

    pid_t pid = fork();
    if (pid == 0) {
        // Child: play chime to the analog card directly, fallback to default ALSA device
        execl("/usr/bin/aplay", "aplay", "-q", "-D", dev_str.c_str(), chime_path.c_str(), nullptr);
        execl("/usr/bin/aplay", "aplay", "-q", chime_path.c_str(), nullptr);
        _exit(127);
    }
}

} // namespace tinexus::hardware
