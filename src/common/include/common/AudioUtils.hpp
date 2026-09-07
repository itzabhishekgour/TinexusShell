#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace tinexus::hardware {

struct AudioCardInfo {
    int id{0};
    std::string name;
    std::string driver;
};

class AudioUtils {
public:
    /**
     * @brief Enumerates physical/virtual ALSA sound cards from base_proc_asound/cards.
     */
    static std::vector<AudioCardInfo> get_sound_cards(const std::string& base_proc_asound = "/proc/asound");

    /**
     * @brief Detects the primary analog sound card ID (filtering out HDMI / discrete GPU audio).
     */
    static int detect_primary_card_id(const std::string& base_proc_asound = "/proc/asound");

    /**
     * @brief Dynamically detects the primary playback simple control name (e.g. Master, Speaker, Headphone, PCM).
     */
    static std::string detect_primary_control(const std::string& base_proc_asound = "/proc/asound");

    /**
     * @brief Gets current volume percentage [0 - 100%].
     */
    static int get_volume_percent(const std::string& base_proc_asound = "/proc/asound");

    /**
     * @brief Sets master/primary volume percentage [0 - 100%].
     * @param pct Target percentage
     * @param persist If true, writes to hardware.toml
     * @param throttle If true, throttles subprocess execution to ~40ms for smooth 60fps drag
     */
    static bool set_volume_percent(int pct, bool persist = true, bool throttle = false);

    /**
     * @brief Returns true if output is currently muted.
     */
    static bool is_muted();

    /**
     * @brief Toggles audio mute state.
     */
    static bool toggle_mute(bool persist = true);

    /**
     * @brief Steps volume by delta_pct (+5%, -5%), automatically unmuting if increasing.
     */
    static int step_volume(int delta_pct, bool persist = true);

    /**
     * @brief Plays a clean, short non-blocking audio chime via aplay.
     */
    static void play_chime(const std::string& chime_path = "/usr/share/sounds/tinexus/volume-chime.wav");
};

} // namespace tinexus::hardware
