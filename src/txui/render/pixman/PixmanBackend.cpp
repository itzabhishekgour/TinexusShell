#include <txui/render/PixmanBackend.hpp>
#include <txui/core/Assert.hpp>
#include <txui/core/Logger.hpp>
#include <algorithm>
#include <cmath>

#if defined(TXUI_HAS_PIXMAN)
#include <pixman.h>
#endif

namespace txui {

namespace {

// True Porter-Duff Source-Over alpha blending for Premultiplied ARGB8888 pixels (Gate 0 format)
[[nodiscard]] constexpr uint32 blend_argb32_premultiplied(uint32 dst, uint32 src) noexcept {
    const uint32 src_a = (src >> 24) & 0xFFU;
    if (src_a == 255U) {
        return src;
    }
    if (src_a == 0U) {
        return dst;
    }

    const uint32 inv_a = 255U - src_a;

    const uint32 src_r = (src >> 16) & 0xFFU;
    const uint32 src_g = (src >> 8)  & 0xFFU;
    const uint32 src_b =  src        & 0xFFU;

    const uint32 dst_r = (dst >> 16) & 0xFFU;
    const uint32 dst_g = (dst >> 8)  & 0xFFU;
    const uint32 dst_b =  dst        & 0xFFU;
    const uint32 dst_a = (dst >> 24) & 0xFFU;

    const uint32 out_r = src_r + ((dst_r * inv_a) / 255U);
    const uint32 out_g = src_g + ((dst_g * inv_a) / 255U);
    const uint32 out_b = src_b + ((dst_b * inv_a) / 255U);
    const uint32 out_a = src_a + ((dst_a * inv_a) / 255U);

    return (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

void rasterize_solid_rect(RenderTarget& target, const Rect& rect, const Color& color) noexcept {
    const int32 target_w = static_cast<int32>(target.width());
    const int32 target_h = static_cast<int32>(target.height());

    // Coordinate precision: geometry uses double (float64), rasterizer converts to int32
    int32 left   = std::max(0, static_cast<int32>(std::floor(rect.left())));
    int32 top    = std::max(0, static_cast<int32>(std::floor(rect.top())));
    int32 right  = std::min(target_w, static_cast<int32>(std::ceil(rect.right())));
    int32 bottom = std::min(target_h, static_cast<int32>(std::ceil(rect.bottom())));

    if (left >= right || top >= bottom) {
        return;
    }

    const uint32 src_argb = color.to_argb32_premultiplied();
    const bool is_opaque = (color.a() == 255U);

#if defined(TXUI_HAS_PIXMAN)
    pixman_image_t* dest_img = pixman_image_create_bits(
        PIXMAN_a8r8g8b8,
        target_w, target_h,
        target.data(),
        target_w * static_cast<int32>(sizeof(uint32))
    );

    if (dest_img) {
        pixman_color_t pcolor;
        pcolor.red   = static_cast<uint16>(color.r() * 257);
        pcolor.green = static_cast<uint16>(color.g() * 257);
        pcolor.blue  = static_cast<uint16>(color.b() * 257);
        pcolor.alpha = static_cast<uint16>(color.a() * 257);

        pixman_fill(
            target.data(),
            target_w,
            32,
            left, top,
            right - left,
            bottom - top,
            src_argb
        );

        pixman_image_unref(dest_img);
        return;
    }
#endif

    uint32* buffer = target.data();
    for (int32 y = top; y < bottom; ++y) {
        uint32* row = buffer + (static_cast<std::size_t>(y) * static_cast<std::size_t>(target_w));
        for (int32 x = left; x < right; ++x) {
            if (is_opaque) {
                row[x] = src_argb;
            } else {
                row[x] = blend_argb32_premultiplied(row[x], src_argb);
            }
        }
    }
}

void rasterize_rounded_rect(RenderTarget& target, const Rect& rect, double radius, const Color& color) noexcept {
    const double max_rad = std::min(rect.width(), rect.height()) * 0.5;
    const double rad = std::clamp(radius, 0.0, max_rad);

    if (rad <= 0.5) {
        rasterize_solid_rect(target, rect, color);
        return;
    }

    const int32 target_w = static_cast<int32>(target.width());
    const int32 target_h = static_cast<int32>(target.height());

    int32 left   = std::max(0, static_cast<int32>(std::floor(rect.left())));
    int32 top    = std::max(0, static_cast<int32>(std::floor(rect.top())));
    int32 right  = std::min(target_w, static_cast<int32>(std::ceil(rect.right())));
    int32 bottom = std::min(target_h, static_cast<int32>(std::ceil(rect.bottom())));

    if (left >= right || top >= bottom) {
        return;
    }

    const double rx1 = rect.left() + rad;
    const double ry1 = rect.top() + rad;
    const double rx2 = rect.right() - rad;
    const double ry2 = rect.bottom() - rad;

    const uint32 solid_argb = color.to_argb32_premultiplied();
    const bool is_opaque = (color.a() == 255U);

    uint32* buffer = target.data();
    for (int32 y = top; y < bottom; ++y) {
        uint32* row = buffer + (static_cast<std::size_t>(y) * static_cast<std::size_t>(target_w));
        const double cy = y + 0.5;

        for (int32 x = left; x < right; ++x) {
            const double cx = x + 0.5;
            double dist = 0.0;

            if (cx < rx1 && cy < ry1) {
                dist = std::hypot(rx1 - cx, ry1 - cy);
            } else if (cx > rx2 && cy < ry1) {
                dist = std::hypot(cx - rx2, ry1 - cy);
            } else if (cx < rx1 && cy > ry2) {
                dist = std::hypot(rx1 - cx, cy - ry2);
            } else if (cx > rx2 && cy > ry2) {
                dist = std::hypot(cx - rx2, cy - ry2);
            }

            double cov = 1.0;
            if (dist > 0.0) {
                if (dist >= rad + 0.7071) {
                    cov = 0.0;
                } else if (dist > rad - 0.7071) {
                    double d = rad - dist;
                    cov = std::clamp((d + 0.7071) / 1.4142, 0.0, 1.0);
                    cov = cov * cov * (3.0 - 2.0 * cov); // Smoothstep analytic AA
                }
            }

            if (cov <= 0.0) {
                continue;
            }

            if (cov >= 0.9999 && is_opaque) {
                row[x] = solid_argb;
            } else {
                uint32 a_mod = static_cast<uint32>(std::round(cov * static_cast<double>(color.a())));
                if (a_mod > 0) {
                    Color mod_color(color.r(), color.g(), color.b(), static_cast<uint8>(a_mod));
                    row[x] = blend_argb32_premultiplied(row[x], mod_color.to_argb32_premultiplied());
                }
            }
        }
    }
}

} // namespace

void PixmanBackend::execute(const CommandBuffer& buffer, RenderTarget& target) {
    TXUI_ASSERT(!buffer.is_empty(), "Cannot execute empty CommandBuffer");

    // Stateless renderer execution loop
    Color current_color = Color::white();

    for (const auto& command : buffer.commands()) {
        std::visit([&](auto&& cmd) {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, BeginFrameCommand>) {
                // Begin frame command processing
            } else if constexpr (std::is_same_v<T, EndFrameCommand>) {
                // End frame command processing
            } else if constexpr (std::is_same_v<T, SetBrushCommand>) {
                std::visit([&](const SolidBrush& solid) {
                    current_color = solid.color();
                }, cmd.brush);
            } else if constexpr (std::is_same_v<T, DrawRectCommand>) {
                Color rect_color = current_color;
                std::visit([&](const SolidBrush& solid) {
                    rect_color = solid.color();
                }, cmd.brush);

                rasterize_solid_rect(target, cmd.rect, rect_color);
            } else if constexpr (std::is_same_v<T, DrawRoundedRectCommand>) {
                Color rrect_color = current_color;
                std::visit([&](const SolidBrush& solid) {
                    rrect_color = solid.color();
                }, cmd.brush);

                rasterize_rounded_rect(target, cmd.rect, cmd.radius, rrect_color);
            }
        }, command);
    }
}

} // namespace txui
