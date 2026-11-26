#ifndef TINEXUS_COMP_DECORATION_ASSETS_HPP
#define TINEXUS_COMP_DECORATION_ASSETS_HPP

#include <cstdint>

extern "C" {
struct wlr_buffer;
}

namespace tinexus::comp {

struct DecorationTheme;

/**
 * Baked, reference-counted HiDPI RGBA asset cache shared across all
 * server-side decorated windows. Generates 2x supersampled ARGB8888 buffers
 * backing both GLES2 and Pixman scene-graph rendering paths identically.
 */
class DecorationAssetCache {
public:
    static constexpr int32_t kAssetScale = 2;
    static constexpr int32_t kCornerRadius = 8;
    static constexpr int32_t kTitlebarHeight = 28;
    static constexpr int32_t kTrafficSize = 12;

    // 9-slice shadow geometry in logical coordinates around [0..total_w, 0..total_h]
    static constexpr int32_t kShadowSide = 32;
    static constexpr int32_t kShadowTop = 22;
    static constexpr int32_t kShadowBottom = 42;
    static constexpr int32_t kShadowInset = 36;
    static constexpr int32_t kShadowEdgeDepth = 2; // extends 2px under 1px frame border

    struct ShadowSlices {
        struct wlr_buffer* tl{nullptr};
        struct wlr_buffer* t{nullptr};
        struct wlr_buffer* tr{nullptr};
        struct wlr_buffer* l{nullptr};
        struct wlr_buffer* r{nullptr};
        struct wlr_buffer* bl{nullptr};
        struct wlr_buffer* b{nullptr};
        struct wlr_buffer* br{nullptr};
    };

    DecorationAssetCache() = default;
    ~DecorationAssetCache();

    DecorationAssetCache(const DecorationAssetCache&) = delete;
    DecorationAssetCache& operator=(const DecorationAssetCache&) = delete;

    void rebuild(const DecorationTheme& theme);
    void release();

    [[nodiscard]] bool is_ready() const noexcept { return m_ready; }

    [[nodiscard]] const ShadowSlices& shadow(bool active) const noexcept {
        return active ? m_shadow_active : m_shadow_inactive;
    }

    [[nodiscard]] struct wlr_buffer* corner_tl(bool active) const noexcept {
        return active ? m_corner_tl_active : m_corner_tl_inactive;
    }
    [[nodiscard]] struct wlr_buffer* corner_tr(bool active) const noexcept {
        return active ? m_corner_tr_active : m_corner_tr_inactive;
    }

    enum class ButtonKind : uint8_t { Close, Minimize, Maximize };

    [[nodiscard]] struct wlr_buffer* traffic_button(ButtonKind kind, bool active, bool hovered) const noexcept;

private:
    bool m_ready{false};

    ShadowSlices m_shadow_active;
    ShadowSlices m_shadow_inactive;

    struct wlr_buffer* m_corner_tl_active{nullptr};
    struct wlr_buffer* m_corner_tr_active{nullptr};
    struct wlr_buffer* m_corner_tl_inactive{nullptr};
    struct wlr_buffer* m_corner_tr_inactive{nullptr};

    struct wlr_buffer* m_close_active{nullptr};
    struct wlr_buffer* m_close_hover{nullptr};
    struct wlr_buffer* m_min_active{nullptr};
    struct wlr_buffer* m_min_hover{nullptr};
    struct wlr_buffer* m_max_active{nullptr};
    struct wlr_buffer* m_max_hover{nullptr};
    struct wlr_buffer* m_btn_inactive{nullptr};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_DECORATION_ASSETS_HPP
