// ============================================================================
// solar_schedule.cpp — Tinexus Dynamic Wallpaper Engine (Milestone 5)
// ============================================================================
#include "wallpaper/solar_schedule.hpp"
#include "common/logger.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

// timerfd (Linux only — guarded for cross-platform headers)
#include <sys/timerfd.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tinexus::wallpaper {

// ─────────────────────────────────────────────────────────────────────────────
// Constants and math helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace {

constexpr double PI = 3.14159265358979323846;

[[nodiscard]] inline double deg2rad(double d) noexcept { return d * PI / 180.0; }
[[nodiscard]] inline double rad2deg(double r) noexcept { return r * 180.0 / PI; }

// Normalize angle to [0, 360)
[[nodiscard]] inline double norm360(double d) noexcept {
    d = std::fmod(d, 360.0);
    if (d < 0.0) d += 360.0;
    return d;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SolarCalculator — VSOP87-lite implementation
//
// Based on Jean Meeus "Astronomical Algorithms" (2nd ed.) Chapter 25 (Low
// Accuracy Solar Position), simplified for the date range 2000–2100.
// ─────────────────────────────────────────────────────────────────────────────

double SolarCalculator::to_julian_day(int64_t unix_utc) noexcept {
    // J2000.0 = JD 2451545.0 = 2000-01-01 12:00:00 UTC = UNIX 946728000
    return 2451545.0 + static_cast<double>(unix_utc - 946728000LL) / 86400.0;
}

double SolarCalculator::gmst_deg(double jd) noexcept {
    // Julian centuries since J2000.0
    const double T = (jd - 2451545.0) / 36525.0;
    // Greenwich Mean Sidereal Time (Meeus Eq. 12.4), degrees
    double gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0)
                + 0.000387933 * T * T
                - T * T * T / 38710000.0;
    return norm360(gmst);
}

SolarPosition SolarCalculator::compute(double latitude_deg,
                                       double longitude_deg,
                                       int64_t unix_utc) noexcept {
    const double jd = to_julian_day(unix_utc);
    const double T  = (jd - 2451545.0) / 36525.0; // Julian centuries from J2000.0

    // ── Geometric mean longitude of the Sun (degrees) ──
    double L0 = norm360(280.46646 + 36000.76983 * T + 0.0003032 * T * T);

    // ── Mean anomaly of the Sun (degrees) ──
    double M = norm360(357.52911 + 35999.05029 * T - 0.0001537 * T * T);
    double M_rad = deg2rad(M);

    // ── Equation of center ──
    double C = (1.914602 - 0.004817 * T - 0.000014 * T * T) * std::sin(M_rad)
             + (0.019993 - 0.000101 * T) * std::sin(2.0 * M_rad)
             +  0.000289 * std::sin(3.0 * M_rad);

    // ── Sun's true longitude ──
    double sun_lon = norm360(L0 + C);

    // ── Apparent longitude (corrected for aberration and nutation) ──
    double omega = norm360(125.04 - 1934.136 * T);
    double lambda = sun_lon - 0.00569 - 0.00478 * std::sin(deg2rad(omega));
    double lambda_rad = deg2rad(lambda);

    // ── Obliquity of the ecliptic ──
    double epsilon0 = 23.0 + 26.0/60.0 + 21.448/3600.0
                    - (46.8150/3600.0) * T
                    - (0.00059/3600.0) * T * T
                    + (0.001813/3600.0) * T * T * T;
    // Apparent obliquity
    double epsilon = epsilon0 + 0.00256 * std::cos(deg2rad(omega));
    double epsilon_rad = deg2rad(epsilon);

    // ── Sun's right ascension and declination ──
    double sin_lambda = std::sin(lambda_rad);
    double alpha = std::atan2(std::cos(epsilon_rad) * sin_lambda, std::cos(lambda_rad));
    double delta = std::asin(std::sin(epsilon_rad) * sin_lambda); // declination, radians

    // ── Greenwich Apparent Sidereal Time → Local Hour Angle ──
    double gmst = gmst_deg(jd);
    double gast = norm360(gmst + 0.00256 * std::cos(deg2rad(omega))); // approximate
    double lha  = deg2rad(norm360(gast + longitude_deg - rad2deg(alpha)));

    // ── Altitude (elevation) and Azimuth ──
    double lat_rad = deg2rad(latitude_deg);
    double sin_alt = std::sin(lat_rad) * std::sin(delta)
                   + std::cos(lat_rad) * std::cos(delta) * std::cos(lha);
    double altitude = rad2deg(std::asin(std::clamp(sin_alt, -1.0, 1.0)));

    double cos_az = (std::sin(delta) - std::sin(lat_rad) * sin_alt)
                  / (std::cos(lat_rad) * std::cos(std::asin(std::clamp(sin_alt, -1.0, 1.0))));
    double az = rad2deg(std::acos(std::clamp(cos_az, -1.0, 1.0)));
    if (std::sin(lha) > 0.0) az = 360.0 - az;

    // Atmospheric refraction correction for elevation near the horizon
    // (Meeus simplified formula, valid for elevation > -0.575°)
    if (altitude > -0.575) {
        double r = 1.02 / std::tan(deg2rad(altitude + 10.3 / (altitude + 5.11)));
        altitude += r / 60.0;
    }

    return { altitude, az };
}

// ─────────────────────────────────────────────────────────────────────────────
// DynamicSchedule — .twallpaper TOML manifest parser
// ─────────────────────────────────────────────────────────────────────────────

bool DynamicSchedule::load(const std::string& file_path) {
    std::ifstream f(file_path);
    if (!f.is_open()) {
        log::warn("[schedule] Cannot open manifest '{}': {}", file_path, std::strerror(errno));
        return false;
    }

    m_frames.clear();
    m_manifest_path = file_path;

    std::string line;
    bool in_frame_section = false;

    // Temporary accumulator for the current [[frame]] block
    std::string frame_path;
    double frame_min = -90.0;
    double frame_max = 90.0;
    bool   frame_has_path = false;

    auto commit_frame = [&]() {
        if (!frame_has_path || frame_path.empty()) return;
        ScheduleFrame sf;
        sf.path          = std::move(frame_path);
        sf.min_elevation = frame_min;
        sf.max_elevation = frame_max;
        sf.index         = static_cast<uint16_t>(m_frames.size());
        m_frames.push_back(std::move(sf));

        // Reset accumulator
        frame_path.clear();
        frame_min     = -90.0;
        frame_max     =  90.0;
        frame_has_path = false;
    };

    auto strip = [](std::string s) -> std::string {
        const auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        s = s.substr(start);
        const auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(0, end + 1);
    };

    auto strip_quotes = [](std::string s) -> std::string {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            return s.substr(1, s.size() - 2);
        return s;
    };

    while (std::getline(f, line)) {
        // Strip comments
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        std::string trimmed = strip(line);
        if (trimmed.empty()) continue;

        // Section header
        if (trimmed == "[[frame]]") {
            // Commit any previously accumulated frame
            commit_frame();
            in_frame_section = true;
            continue;
        }
        if (!trimmed.empty() && trimmed[0] == '[') {
            commit_frame();
            in_frame_section = false;
            continue;
        }

        // Key=value parsing
        auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = strip(trimmed.substr(0, eq));
        std::string val = strip(trimmed.substr(eq + 1));

        if (!in_frame_section) {
            // Top-level keys
            if (key == "latitude") {
                try { m_latitude = std::stod(val); } catch (...) {}
            } else if (key == "longitude") {
                try { m_longitude = std::stod(val); } catch (...) {}
            }
        } else {
            // [[frame]] block keys
            if (key == "path") {
                frame_path     = strip_quotes(val);
                frame_has_path = true;
            } else if (key == "min_elevation") {
                try { frame_min = std::stod(val); } catch (...) {}
            } else if (key == "max_elevation") {
                try { frame_max = std::stod(val); } catch (...) {}
            }
        }
    }
    commit_frame(); // flush last frame

    if (m_frames.empty()) {
        log::warn("[schedule] Manifest '{}' contains no valid [[frame]] entries", file_path);
        return false;
    }

    log::info("[schedule] Loaded manifest '{}' — {} frames, lat={:.2f}, lon={:.2f}",
              file_path, m_frames.size(), m_latitude, m_longitude);
    return true;
}

std::optional<ScheduleFrame> DynamicSchedule::resolve_frame(int64_t unix_utc) const {
    if (m_frames.empty()) return std::nullopt;

    const SolarPosition pos = SolarCalculator::compute(m_latitude, m_longitude, unix_utc);

    for (const auto& frame : m_frames) {
        if (pos.elevation_deg >= frame.min_elevation &&
            pos.elevation_deg <  frame.max_elevation) {
            return frame;
        }
    }

    // Fallback: return the frame whose range covers this elevation most closely
    // (handles edge cases at exactly ±90°)
    double best_dist = std::numeric_limits<double>::max();
    const ScheduleFrame* best = nullptr;
    for (const auto& frame : m_frames) {
        double center = (frame.min_elevation + frame.max_elevation) * 0.5;
        double dist   = std::abs(pos.elevation_deg - center);
        if (dist < best_dist) { best_dist = dist; best = &frame; }
    }
    if (best) return *best;
    return std::nullopt;
}

uint32_t DynamicSchedule::seconds_until_next_transition(int64_t unix_utc) const {
    if (m_frames.empty()) return 3600;

    // Determine current frame index
    auto cur = resolve_frame(unix_utc);
    const uint16_t cur_idx = cur ? cur->index : 0xFFFF;

    // Binary search forward in 60-second steps up to 24 hours
    constexpr int64_t STEP = 60;
    constexpr int64_t MAX_LOOK_AHEAD = 24 * 3600;

    for (int64_t delta = STEP; delta <= MAX_LOOK_AHEAD; delta += STEP) {
        auto next = resolve_frame(unix_utc + delta);
        const uint16_t next_idx = next ? next->index : 0xFFFF;
        if (next_idx != cur_idx) {
            // Transition happens somewhere in [delta-STEP, delta]
            // Return conservative estimate: fire at the start of that window
            return static_cast<uint32_t>(std::max<int64_t>(1, delta - STEP));
        }
    }
    return 3600; // No transition found in 24h — recheck in 1 hour
}

// ─────────────────────────────────────────────────────────────────────────────
// DynamicTimer — timerfd-based next-frame scheduler
// ─────────────────────────────────────────────────────────────────────────────

DynamicTimer::DynamicTimer() {
    m_fd = ::timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (m_fd < 0) {
        log::warn("[timer] timerfd_create failed: {} — dynamic schedule disabled",
                  std::strerror(errno));
    }
}

DynamicTimer::~DynamicTimer() {
    if (m_fd >= 0) ::close(m_fd);
}

void DynamicTimer::arm(uint32_t seconds_from_now) {
    if (m_fd < 0) return;
    if (seconds_from_now == 0) { disarm(); return; }

    // Clamp to [1, 86400]
    const uint32_t secs = std::clamp(seconds_from_now, 1u, 86400u);

    struct itimerspec ts{};
    ts.it_value.tv_sec  = static_cast<time_t>(secs);
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec  = 0; // One-shot — must re-arm after each fire
    ts.it_interval.tv_nsec = 0;

    if (::timerfd_settime(m_fd, 0, &ts, nullptr) < 0) {
        log::warn("[timer] timerfd_settime failed: {}", std::strerror(errno));
    } else {
        log::debug("[timer] Armed for {} seconds", secs);
    }
}

void DynamicTimer::disarm() {
    if (m_fd < 0) return;
    struct itimerspec ts{};
    ::timerfd_settime(m_fd, 0, &ts, nullptr);
}

void DynamicTimer::consume_expiry() {
    if (m_fd < 0) return;
    uint64_t count = 0;
    // Non-blocking read — discards the expiry count
    ssize_t n = ::read(m_fd, &count, sizeof(count));
    (void)n; // Expected: returns 8 bytes
}

// ─────────────────────────────────────────────────────────────────────────────
// find_twallpaper_manifests — XDG discovery
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::string> find_twallpaper_manifests() {
    std::vector<std::string> results;
    std::error_code ec;

    auto scan_dir = [&](const fs::path& dir) {
        if (!fs::is_directory(dir, ec) || ec) return;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (entry.path().extension() == ".twallpaper") {
                results.push_back(entry.path().string());
            }
        }
    };

    // Priority 1: $XDG_DATA_HOME/tinexus/wallpapers/
    const char* xdg_data_home = std::getenv("XDG_DATA_HOME");
    if (xdg_data_home && xdg_data_home[0] != '\0') {
        scan_dir(fs::path(xdg_data_home) / "tinexus/wallpapers");
    } else {
        const char* home = std::getenv("HOME");
        if (home && home[0] != '\0') {
            scan_dir(fs::path(home) / ".local/share/tinexus/wallpapers");
        }
    }

    // Priority 2: $XDG_DATA_DIRS/tinexus/wallpapers/ (colon-separated)
    const char* xdg_data_dirs = std::getenv("XDG_DATA_DIRS");
    const std::string dirs_str = xdg_data_dirs ? xdg_data_dirs : "/usr/local/share:/usr/share";
    std::istringstream ss(dirs_str);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (!dir.empty()) scan_dir(fs::path(dir) / "tinexus/wallpapers");
    }

    return results;
}

} // namespace tinexus::wallpaper
