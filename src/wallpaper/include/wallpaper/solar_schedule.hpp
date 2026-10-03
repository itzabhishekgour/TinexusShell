// ============================================================================
// solar_schedule.hpp — Tinexus Dynamic Wallpaper Engine (Milestone 5)
// ============================================================================
// Pure C++20 VSOP87-lite solar position calculator, .twallpaper TOML parser,
// and timerfd-based next-frame scheduler.
//
// Design rules:
//   • Zero external astronomy or TOML library dependencies.
//   • VSOP87 simplified formulas: ±0.03° precision over 2000–2100.
//   • No blocking I/O on the wallpaper daemon thread.
//   • timerfd added to the existing poll() fd set — zero CPU spinning.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace tinexus::wallpaper {

// ─────────────────────────────────────────────────────────────────────────────
// SolarPosition — output of SolarCalculator
// ─────────────────────────────────────────────────────────────────────────────
struct SolarPosition {
    double elevation_deg;  // Altitude above horizon (-90 to +90)
    double azimuth_deg;    // Compass bearing (0=N, 90=E, 180=S, 270=W)
};

// ─────────────────────────────────────────────────────────────────────────────
// SolarCalculator — VSOP87-lite sun elevation angle
// ─────────────────────────────────────────────────────────────────────────────
class SolarCalculator {
public:
    // Compute sun position at UTC unix timestamp for given geographic coordinates.
    [[nodiscard]] static SolarPosition compute(double  latitude_deg,
                                               double  longitude_deg,
                                               int64_t unix_utc) noexcept;

private:
    [[nodiscard]] static double gmst_deg(double jd) noexcept;
    [[nodiscard]] static double to_julian_day(int64_t unix_utc) noexcept;
};

// ─────────────────────────────────────────────────────────────────────────────
// ScheduleFrame — one entry in a .twallpaper manifest
// ─────────────────────────────────────────────────────────────────────────────
struct ScheduleFrame {
    std::string path;           // Absolute or XDG-relative path to image
    double      min_elevation;  // Inclusive lower bound (degrees)
    double      max_elevation;  // Exclusive upper bound (degrees)
    uint16_t    index;          // 0-based index in the manifest
};

// ─────────────────────────────────────────────────────────────────────────────
// DynamicSchedule — .twallpaper TOML manifest parser and frame resolver
//
// Manifest format (TOML):
//   latitude  = 28.6
//   longitude = 77.2
//
//   [[frame]]
//   path          = "/usr/share/tinexus/wallpapers/night.jpg"
//   min_elevation = -90.0
//   max_elevation = -6.0
//
//   [[frame]]
//   path          = "/usr/share/tinexus/wallpapers/dawn.jpg"
//   min_elevation = -6.0
//   max_elevation = 0.0
//
//   [[frame]]
//   path          = "/usr/share/tinexus/wallpapers/day.jpg"
//   min_elevation = 0.0
//   max_elevation = 90.0
// ─────────────────────────────────────────────────────────────────────────────
class DynamicSchedule {
public:
    DynamicSchedule() = default;

    // Parse a .twallpaper TOML manifest from file_path.
    [[nodiscard]] bool load(const std::string& file_path);

    // Resolve which frame is active at unix_utc. Returns nullopt if schedule empty.
    [[nodiscard]] std::optional<ScheduleFrame> resolve_frame(int64_t unix_utc) const;

    // Returns seconds until the next solar elevation frame boundary.
    // Binary-searches forward in 60-second steps. Returns 3600 if no transition found.
    [[nodiscard]] uint32_t seconds_until_next_transition(int64_t unix_utc) const;

    [[nodiscard]] bool     is_loaded()    const noexcept { return !m_frames.empty(); }
    [[nodiscard]] uint16_t frame_count()  const noexcept { return static_cast<uint16_t>(m_frames.size()); }
    [[nodiscard]] double   latitude()     const noexcept { return m_latitude; }
    [[nodiscard]] double   longitude()    const noexcept { return m_longitude; }
    [[nodiscard]] const std::vector<ScheduleFrame>& frames() const noexcept { return m_frames; }

private:
    std::vector<ScheduleFrame> m_frames;
    double      m_latitude{28.6};
    double      m_longitude{77.2};
    std::string m_manifest_path;
};

// ─────────────────────────────────────────────────────────────────────────────
// DynamicTimer — timerfd-based next-frame scheduler
// ─────────────────────────────────────────────────────────────────────────────
class DynamicTimer {
public:
    DynamicTimer();
    ~DynamicTimer();

    DynamicTimer(const DynamicTimer&) = delete;
    DynamicTimer& operator=(const DynamicTimer&) = delete;

    // Arm to fire `seconds_from_now` seconds in the future. 0 = disarm.
    void arm(uint32_t seconds_from_now);

    // Disarm the timer.
    void disarm();

    // Must be called after poll() returns POLLIN on this fd.
    void consume_expiry();

    [[nodiscard]] int  fd()       const noexcept { return m_fd; }
    [[nodiscard]] bool is_valid() const noexcept { return m_fd >= 0; }

private:
    int m_fd{-1};
};

// Find all .twallpaper manifests in XDG data directories.
// Priority: $XDG_DATA_HOME/tinexus/wallpapers/ → $XDG_DATA_DIRS/tinexus/wallpapers/
[[nodiscard]] std::vector<std::string> find_twallpaper_manifests();

} // namespace tinexus::wallpaper
