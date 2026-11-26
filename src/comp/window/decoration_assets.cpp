#include "comp/window/decoration_assets.hpp"
#include "comp/window/decoration_manager.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

extern "C" {
#include <drm_fourcc.h>
#include <wlr/interfaces/wlr_buffer.h>
}

namespace tinexus::comp {

namespace {

struct PixelBuffer {
    struct wlr_buffer base{};
    std::vector<uint32_t> pixels;
    size_t stride{0};
};

void pixel_buffer_destroy(struct wlr_buffer* wlr_buf) {
    PixelBuffer* pb = wl_container_of(wlr_buf, pb, base);
    wlr_buffer_finish(&pb->base);
    std::unique_ptr<PixelBuffer> owner(pb);
}

bool pixel_buffer_begin_data_ptr_access(struct wlr_buffer* wlr_buf, uint32_t flags,
                                        void** data, uint32_t* format, size_t* stride) {
    if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
        return false;
    }
    PixelBuffer* pb = wl_container_of(wlr_buf, pb, base);
    *data = pb->pixels.data();
    *format = DRM_FORMAT_ARGB8888;
    *stride = pb->stride;
    return true;
}

void pixel_buffer_end_data_ptr_access(struct wlr_buffer* /*wlr_buf*/) {}

const struct wlr_buffer_impl kPixelBufferImpl = {
    .destroy = pixel_buffer_destroy,
    .get_dmabuf = nullptr,
    .get_shm = nullptr,
    .begin_data_ptr_access = pixel_buffer_begin_data_ptr_access,
    .end_data_ptr_access = pixel_buffer_end_data_ptr_access,
};

struct wlr_buffer* make_pixel_buffer(int32_t width, int32_t height, std::vector<uint32_t>&& pixels) {
    auto pb = std::make_unique<PixelBuffer>();
    pb->stride = static_cast<size_t>(width) * sizeof(uint32_t);
    pb->pixels = std::move(pixels);
    wlr_buffer_init(&pb->base, &kPixelBufferImpl, width, height);
    return &pb.release()->base;
}

inline void drop_buffer(struct wlr_buffer*& buf) {
    if (buf) {
        wlr_buffer_drop(buf);
        buf = nullptr;
    }
}

inline uint32_t pack_premul_argb(double r, double g, double b, double a) noexcept {
    a = std::clamp(a, 0.0, 1.0);
    r = std::clamp(r, 0.0, 1.0) * a;
    g = std::clamp(g, 0.0, 1.0) * a;
    b = std::clamp(b, 0.0, 1.0) * a;
    auto ia = static_cast<uint32_t>(std::lround(a * 255.0));
    auto ir = static_cast<uint32_t>(std::lround(r * 255.0));
    auto ig = static_cast<uint32_t>(std::lround(g * 255.0));
    auto ib = static_cast<uint32_t>(std::lround(b * 255.0));
    return (ia << 24) | (ir << 16) | (ig << 8) | ib;
}

inline double smoothstep(double edge0, double edge1, double x) noexcept {
    double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Closed-form 1D Gaussian box integral over [a, b] evaluated at coordinate x
inline double gaussian_box_1d(double x, double a, double b, double sigma) noexcept {
    const double inv_s = 1.0 / (sigma * 1.4142135623730951);
    return 0.5 * (std::erf((x - a) * inv_s) - std::erf((x - b) * inv_s));
}

// Signed distance outside a rounded-top box [0, W] x [0, H]
// Negative inside, positive outside. Bottom corners have small 2px bevel to match shadow softly.
inline double frame_sdf_local(double x, double y, double w, double h, double r_top, double r_bot) noexcept {
    const bool in_top = (y < h * 0.5);
    const double r = in_top ? r_top : r_bot;
    const double qx = std::abs(x - w * 0.5) - (w * 0.5 - r);
    const double qy = std::abs(y - h * 0.5) - (h * 0.5 - r);
    const double dx = std::max(qx, 0.0);
    const double dy = std::max(qy, 0.0);
    return std::hypot(dx, dy) + std::min(std::max(qx, qy), 0.0) - r;
}

struct ShadowParams {
    double sigma_contact;
    double offset_y_contact;
    double alpha_contact;
    double sigma_ambient;
    double offset_y_ambient;
    double alpha_ambient;
};

double sample_dual_shadow_alpha(double local_x, double local_y, double ref_w, double ref_h,
                                const ShadowParams& p, bool clip_inside) noexcept {
    // Layer 1: Tight contact shadow anchoring window edges
    const double c_x = gaussian_box_1d(local_x, 0.0, ref_w, p.sigma_contact);
    const double c_y = gaussian_box_1d(local_y - p.offset_y_contact, 0.0, ref_h, p.sigma_contact);
    const double a_contact = p.alpha_contact * c_x * c_y;

    // Layer 2: Wide ambient elevation shadow giving floating depth
    const double a_x = gaussian_box_1d(local_x, 0.0, ref_w, p.sigma_ambient);
    const double a_y = gaussian_box_1d(local_y - p.offset_y_ambient, 0.0, ref_h, p.sigma_ambient);
    const double a_ambient = p.alpha_ambient * a_x * a_y;

    // Porter-Duff "over" combination of contact + ambient layers
    double combined = a_contact + a_ambient * (1.0 - a_contact);

    if (clip_inside) {
        const double sdf = frame_sdf_local(local_x, local_y, ref_w, ref_h,
                                           static_cast<double>(DecorationAssetCache::kCornerRadius), 1.5);
        // Zero alpha strictly inside window interior so translucent clients aren't darkened
        const double outside_mask = smoothstep(-0.25, 0.75, sdf);
        combined *= outside_mask;
    }
    return std::clamp(combined, 0.0, 1.0);
}

// Bake an arbitrary slice using reference virtual frame [0..ref_w] x [0..ref_h]
struct wlr_buffer* bake_shadow_slice(int32_t logical_w, int32_t logical_h,
                                     double origin_x, double origin_y,
                                     double ref_w, double ref_h,
                                     const ShadowParams& params,
                                     bool stretch_x, bool stretch_y) {
    const int32_t S = DecorationAssetCache::kAssetScale;
    const int32_t pw = logical_w * S;
    const int32_t ph = logical_h * S;
    std::vector<uint32_t> pixels(static_cast<size_t>(pw) * static_cast<size_t>(ph), 0);

    for (int32_t py = 0; py < ph; ++py) {
        const double ly = stretch_y ? (ref_h * 0.5)
                                    : (origin_y + (static_cast<double>(py) + 0.5) / static_cast<double>(S));
        for (int32_t px = 0; px < pw; ++px) {
            const double lx = stretch_x ? (ref_w * 0.5)
                                        : (origin_x + (static_cast<double>(px) + 0.5) / static_cast<double>(S));
            double alpha = sample_dual_shadow_alpha(lx, ly, ref_w, ref_h, params, true);

            // Subtle interleaved gradient noise dithering (+/- 0.5 LSB) to prevent 8-bit gradient banding
            if (alpha > 0.001 && !stretch_x && !stretch_y) {
                const double ign = std::fmod(52.9829189 * std::fmod(0.06711056 * px + 0.00583715 * py, 1.0), 1.0);
                alpha = std::clamp(alpha + (ign - 0.5) / 255.0, 0.0, 1.0);
            }
            pixels[static_cast<size_t>(py) * pw + px] = pack_premul_argb(0.0, 0.0, 0.0, alpha);
        }
    }
    return make_pixel_buffer(pw, ph, std::move(pixels));
}

DecorationAssetCache::ShadowSlices bake_shadow_set(bool active) {
    const ShadowParams params = active
        ? ShadowParams{3.2, 1.5, 0.30, 13.5, 7.0, 0.32}
        : ShadowParams{2.4, 1.0, 0.16,  8.5, 3.0, 0.16};

    constexpr int32_t M_X = DecorationAssetCache::kShadowSide;
    constexpr int32_t M_T = DecorationAssetCache::kShadowTop;
    constexpr int32_t M_B = DecorationAssetCache::kShadowBottom;
    constexpr int32_t IN  = DecorationAssetCache::kShadowInset;
    constexpr int32_t E_D = DecorationAssetCache::kShadowEdgeDepth;
    constexpr double REF_W = 240.0;
    constexpr double REF_H = 240.0;

    DecorationAssetCache::ShadowSlices s{};
    // Corner caps span [-margin .. +inset] around each frame corner
    s.tl = bake_shadow_slice(M_X + IN, M_T + IN, -M_X,         -M_T,         REF_W, REF_H, params, false, false);
    s.tr = bake_shadow_slice(M_X + IN, M_T + IN, REF_W - IN,   -M_T,         REF_W, REF_H, params, false, false);
    s.bl = bake_shadow_slice(M_X + IN, M_B + IN, -M_X,         REF_H - IN,   REF_W, REF_H, params, false, false);
    s.br = bake_shadow_slice(M_X + IN, M_B + IN, REF_W - IN,   REF_H - IN,   REF_W, REF_H, params, false, false);

    // 1D edge strips stretched bilinearly; 1 logical px along stretched axis avoids edge sampling drift
    s.t  = bake_shadow_slice(1,        M_T + E_D, 0.0,         -M_T,         REF_W, REF_H, params, true,  false);
    s.b  = bake_shadow_slice(1,        M_B + E_D, 0.0,         REF_H - E_D,  REF_W, REF_H, params, true,  false);
    s.l  = bake_shadow_slice(M_X + E_D, 1,        -M_X,        0.0,          REF_W, REF_H, params, false, true);
    s.r  = bake_shadow_slice(M_X + E_D, 1,        REF_W - E_D, 0.0,          REF_W, REF_H, params, false, true);
    return s;
}

struct wlr_buffer* bake_rounded_corner(const float bg[4], const float border[4], bool is_right) {
    constexpr int32_t R = DecorationAssetCache::kCornerRadius;
    constexpr int32_t S = DecorationAssetCache::kAssetScale;
    constexpr int32_t PS = R * S;
    constexpr int32_t kSub = 4;

    std::vector<uint32_t> pixels(static_cast<size_t>(PS) * static_cast<size_t>(PS), 0);

    // Circle center in logical [0..R]x[0..R] box:
    // Left corner curves around (R, R); Right corner curves around (0, R)
    const double cx = is_right ? 0.0 : static_cast<double>(R);
    const double cy = static_cast<double>(R);

    for (int32_t py = 0; py < PS; ++py) {
        for (int32_t px = 0; px < PS; ++px) {
            double accum_r = 0.0, accum_g = 0.0, accum_b = 0.0, accum_a = 0.0;
            for (int sy = 0; sy < kSub; ++sy) {
                for (int sx = 0; sx < kSub; ++sx) {
                    const double lx = (static_cast<double>(px) + (sx + 0.5) / kSub) / static_cast<double>(S);
                    const double ly = (static_cast<double>(py) + (sy + 0.5) / kSub) / static_cast<double>(S);
                    const double dist = std::hypot(lx - cx, ly - cy);

                    if (dist <= static_cast<double>(R) - 1.0) {
                        accum_r += bg[0];
                        accum_g += bg[1];
                        accum_b += bg[2];
                        accum_a += bg[3];
                    } else if (dist <= static_cast<double>(R)) {
                        accum_r += border[0];
                        accum_g += border[1];
                        accum_b += border[2];
                        accum_a += border[3];
                    }
                }
            }
            constexpr double inv = 1.0 / (kSub * kSub);
            const double a = accum_a * inv;
            if (a > 1e-6) {
                pixels[static_cast<size_t>(py) * PS + px] = pack_premul_argb(
                    accum_r / accum_a, accum_g / accum_a, accum_b / accum_a, a);
            }
        }
    }
    return make_pixel_buffer(PS, PS, std::move(pixels));
}

// Segment distance for crisp glyph strokes inside hovered traffic lights
inline double dist_to_segment(double px, double py, double x1, double y1, double x2, double y2) noexcept {
    const double vx = x2 - x1;
    const double vy = y2 - y1;
    const double wx = px - x1;
    const double wy = py - y1;
    const double c1 = vx * wx + vy * wy;
    if (c1 <= 0.0) return std::hypot(px - x1, py - y1);
    const double c2 = vx * vx + vy * vy;
    if (c2 <= c1) return std::hypot(px - x2, py - y2);
    const double t = c1 / c2;
    return std::hypot(px - (x1 + t * vx), py - (y1 + t * vy));
}

struct wlr_buffer* bake_traffic_button(DecorationAssetCache::ButtonKind kind,
                                       const float fill[4], const float border[4],
                                       bool active, bool hovered) {
    constexpr int32_t LOGICAL = DecorationAssetCache::kTrafficSize; // 12x12
    constexpr int32_t S = DecorationAssetCache::kAssetScale;        // 2x -> 24x24
    constexpr int32_t PS = LOGICAL * S;
    constexpr int32_t kSub = 4;

    const double center = LOGICAL * 0.5;
    const double radius = LOGICAL * 0.5 - 0.15;
    const double border_inner = radius - 0.85;

    std::vector<uint32_t> pixels(static_cast<size_t>(PS) * static_cast<size_t>(PS), 0);

    for (int32_t py = 0; py < PS; ++py) {
        for (int32_t px = 0; px < PS; ++px) {
            double accum_r = 0.0, accum_g = 0.0, accum_b = 0.0, accum_a = 0.0;

            for (int sy = 0; sy < kSub; ++sy) {
                for (int sx = 0; sx < kSub; ++sx) {
                    const double lx = (static_cast<double>(px) + (sx + 0.5) / kSub) / static_cast<double>(S);
                    const double ly = (static_cast<double>(py) + (sy + 0.5) / kSub) / static_cast<double>(S);
                    const double dist = std::hypot(lx - center, ly - center);

                    if (dist > radius) {
                        continue;
                    }

                    double sr = fill[0];
                    double sg = fill[1];
                    double sb = fill[2];
                    double sa = fill[3];

                    if (dist >= border_inner) {
                        sr = border[0];
                        sg = border[1];
                        sb = border[2];
                        sa = border[3];
                    } else if (hovered && active) {
                        // Crisp dark glyph inside traffic circle on hover
                        double glyph_d = 99.0;
                        if (kind == DecorationAssetCache::ButtonKind::Close) {
                            double d1 = dist_to_segment(lx, ly, center - 2.3, center - 2.3, center + 2.3, center + 2.3);
                            double d2 = dist_to_segment(lx, ly, center - 2.3, center + 2.3, center + 2.3, center - 2.3);
                            glyph_d = std::min(d1, d2);
                        } else if (kind == DecorationAssetCache::ButtonKind::Minimize) {
                            glyph_d = dist_to_segment(lx, ly, center - 2.7, center, center + 2.7, center);
                        } else {
                            double d1 = dist_to_segment(lx, ly, center - 2.5, center, center + 2.5, center);
                            double d2 = dist_to_segment(lx, ly, center, center - 2.5, center, center + 2.5);
                            glyph_d = std::min(d1, d2);
                        }
                        if (glyph_d <= 0.62) {
                            // Deep contrasting ink for legibility across red/amber/green
                            sr *= 0.22;
                            sg *= 0.22;
                            sb *= 0.22;
                        }
                    }

                    accum_r += sr;
                    accum_g += sg;
                    accum_b += sb;
                    accum_a += sa;
                }
            }

            constexpr double inv = 1.0 / (kSub * kSub);
            const double a = accum_a * inv;
            if (a > 1e-6) {
                pixels[static_cast<size_t>(py) * PS + px] = pack_premul_argb(
                    accum_r / accum_a, accum_g / accum_a, accum_b / accum_a, a);
            }
        }
    }

    return make_pixel_buffer(PS, PS, std::move(pixels));
}

} // namespace

DecorationAssetCache::~DecorationAssetCache() {
    release();
}

void DecorationAssetCache::release() {
    drop_buffer(m_shadow_active.tl);
    drop_buffer(m_shadow_active.t);
    drop_buffer(m_shadow_active.tr);
    drop_buffer(m_shadow_active.l);
    drop_buffer(m_shadow_active.r);
    drop_buffer(m_shadow_active.bl);
    drop_buffer(m_shadow_active.b);
    drop_buffer(m_shadow_active.br);

    drop_buffer(m_shadow_inactive.tl);
    drop_buffer(m_shadow_inactive.t);
    drop_buffer(m_shadow_inactive.tr);
    drop_buffer(m_shadow_inactive.l);
    drop_buffer(m_shadow_inactive.r);
    drop_buffer(m_shadow_inactive.bl);
    drop_buffer(m_shadow_inactive.b);
    drop_buffer(m_shadow_inactive.br);

    drop_buffer(m_corner_tl_active);
    drop_buffer(m_corner_tr_active);
    drop_buffer(m_corner_tl_inactive);
    drop_buffer(m_corner_tr_inactive);

    drop_buffer(m_close_active);
    drop_buffer(m_close_hover);
    drop_buffer(m_min_active);
    drop_buffer(m_min_hover);
    drop_buffer(m_max_active);
    drop_buffer(m_max_hover);
    drop_buffer(m_btn_inactive);

    m_ready = false;
}

void DecorationAssetCache::rebuild(const DecorationTheme& theme) {
    release();

    m_shadow_active = bake_shadow_set(true);
    m_shadow_inactive = bake_shadow_set(false);

    m_corner_tl_active = bake_rounded_corner(theme.title_bg_active, theme.border_active, false);
    m_corner_tr_active = bake_rounded_corner(theme.title_bg_active, theme.border_active, true);
    m_corner_tl_inactive = bake_rounded_corner(theme.title_bg_inactive, theme.border_inactive, false);
    m_corner_tr_inactive = bake_rounded_corner(theme.title_bg_inactive, theme.border_inactive, true);

    m_close_active = bake_traffic_button(ButtonKind::Close, theme.close_fill, theme.close_border, true, false);
    m_close_hover = bake_traffic_button(ButtonKind::Close, theme.close_fill, theme.close_border, true, true);
    m_min_active = bake_traffic_button(ButtonKind::Minimize, theme.min_fill, theme.min_border, true, false);
    m_min_hover = bake_traffic_button(ButtonKind::Minimize, theme.min_fill, theme.min_border, true, true);
    m_max_active = bake_traffic_button(ButtonKind::Maximize, theme.max_fill, theme.max_border, true, false);
    m_max_hover = bake_traffic_button(ButtonKind::Maximize, theme.max_fill, theme.max_border, true, true);
    m_btn_inactive = bake_traffic_button(ButtonKind::Close, theme.inactive_fill, theme.inactive_border, false, false);

    m_ready = true;
}

struct wlr_buffer* DecorationAssetCache::traffic_button(ButtonKind kind, bool active, bool hovered) const noexcept {
    if (!active) {
        return m_btn_inactive;
    }
    switch (kind) {
        case ButtonKind::Close:
            return hovered ? m_close_hover : m_close_active;
        case ButtonKind::Minimize:
            return hovered ? m_min_hover : m_min_active;
        case ButtonKind::Maximize:
            return hovered ? m_max_hover : m_max_active;
    }
    return m_close_active;
}

} // namespace tinexus::comp
