#ifndef TINEXUS_WALLPAPER_PROVIDER_HPP
#define TINEXUS_WALLPAPER_PROVIDER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <unordered_map>
#include <functional>

namespace tinexus::wallpaper {

// ─────────────────────────────────────────────────────────────────────────────
// Wire-compatible fit mode — values MUST match WallpaperChangedPayload::mode
// ─────────────────────────────────────────────────────────────────────────────
enum class FitMode : uint8_t {
    Fill    = 0, // Aspect-fill: scale to cover entire output, crop edges symmetrically
    Fit     = 1, // Aspect-fit:  scale to fit inside output, letterbox/pillarbox
    Center  = 2, // Natural size, centered, no scaling
    Tile    = 3, // Tile 1:1 pixel across the output
    Stretch = 4, // Unconstrained stretch (distorts — use sparingly)
};

// ─────────────────────────────────────────────────────────────────────────────
// WallpaperBuffer — CPU-side ARGB8888 pixel buffer for one output
// ─────────────────────────────────────────────────────────────────────────────
struct WallpaperBuffer {
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint32_t> pixels; // ARGB8888, row-major
};

// ─────────────────────────────────────────────────────────────────────────────
// WallpaperCache — LRU cache of decoded source pixel buffers
//
// Each entry is the FULL decoded (ARGB8888) source image at native resolution.
// The scaled-to-output copy is NOT cached — it is blitted per-output on demand.
//
// Policy:
//   • Max capacity: MAX_CACHE_BYTES (192 MB)
//   • Eviction: least-recently-used entry is evicted when limit is exceeded
//   • Thread safety: single-threaded (only accessed from the wallpaper render path)
// ─────────────────────────────────────────────────────────────────────────────
struct CachedImage {
    std::string             path;
    uint32_t                width;
    uint32_t                height;
    std::vector<uint32_t>   pixels; // decoded ARGB8888 at source resolution
    size_t                  byte_size; // pixels.size() * 4
};

class WallpaperCache {
public:
    static constexpr size_t MAX_CACHE_BYTES = 192ULL * 1024ULL * 1024ULL; // 192 MB

    // Returns a pointer to the cached image for `path`, loading and evicting
    // as necessary. Returns nullptr if the image cannot be loaded.
    [[nodiscard]] const CachedImage* get_or_load(const std::string& path);

    // Evict a specific path (called when switching away from a dynamic frame).
    void evict(const std::string& path);

    // Force-clear all entries (e.g., on wallpaper daemon teardown).
    void clear();

    [[nodiscard]] size_t total_bytes() const noexcept { return m_total_bytes; }
    [[nodiscard]] size_t entry_count() const noexcept { return m_map.size(); }

private:
    void evict_lru_until_under_limit();

    // LRU: front = most recently used, back = least recently used
    std::list<CachedImage>                              m_lru_list;
    std::unordered_map<std::string, std::list<CachedImage>::iterator> m_map;
    size_t                                              m_total_bytes{0};
};

// ─────────────────────────────────────────────────────────────────────────────
// IWallpaperProvider — abstract provider interface
// ─────────────────────────────────────────────────────────────────────────────
class IWallpaperProvider {
public:
    virtual ~IWallpaperProvider() = default;

    [[nodiscard]] virtual std::string type_name() const noexcept = 0;
    [[nodiscard]] virtual bool load(const std::string& uri) = 0;
    [[nodiscard]] virtual WallpaperBuffer render_buffer(uint32_t target_width,
                                                        uint32_t target_height,
                                                        FitMode   mode = FitMode::Fill) = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// ImageProvider — loads a single image file via stb_image, renders per-output
// ─────────────────────────────────────────────────────────────────────────────
class ImageProvider : public IWallpaperProvider {
public:
    ImageProvider() = default;
    ~ImageProvider() override = default;

    std::string type_name() const noexcept override { return "ImageProvider"; }

    // Loads image from `path` into the shared WallpaperCache.
    // Returns true on success, false if stb_image fails (caller renders gradient).
    bool load(const std::string& path) override;

    // Renders the loaded image into a `target_width × target_height` ARGB8888 buffer
    // using the specified `mode`. Uses bilinear sampling for scaled outputs.
    // If no image is loaded, renders the Deep Teal procedural gradient fallback.
    WallpaperBuffer render_buffer(uint32_t target_width,
                                  uint32_t target_height,
                                  FitMode   mode = FitMode::Fill) override;

    // Performs a CPU cross-fade lerp: output[i] = lerp(src_a[i], src_b[i], t)
    // where t ∈ [0.0, 1.0]. Hot path — compiler auto-vectorizes with -O2.
    // Called 30× per wallpaper transition (500ms at 60fps).
    static void crossfade_lerp(const uint32_t* __restrict__ src_a,
                                const uint32_t* __restrict__ src_b,
                                uint32_t* __restrict__       dst,
                                size_t                       pixel_count,
                                float                        t) noexcept;

    // Access the process-wide singleton cache
    static WallpaperCache& cache() noexcept { return s_cache; }

private:
    // Bilinear sample from a source pixel buffer at sub-pixel coordinate (fx, fy).
    [[nodiscard]] static uint32_t bilinear_sample(const uint32_t* pixels,
                                                   uint32_t src_w,
                                                   uint32_t src_h,
                                                   float    fx,
                                                   float    fy) noexcept;

    std::string  m_path;       // last successfully loaded path
    const CachedImage* m_img{nullptr}; // non-owning pointer into WallpaperCache

    static WallpaperCache s_cache; // Process-wide LRU cache shared across all outputs
};

} // namespace tinexus::wallpaper

#endif // TINEXUS_WALLPAPER_PROVIDER_HPP
