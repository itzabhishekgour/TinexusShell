#include <txui/render/PixmanBackend.hpp>
#include <txui/core/Assert.hpp>
#include <txui/core/Logger.hpp>
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <unordered_map>
#include <vector>
#include <list>

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

void rasterize_solid_rect(RenderTarget& target, const Rect& rect, const Color& color, const Rect& clip) noexcept {
    const int32 target_w = static_cast<int32>(target.width());
    const int32 target_h = static_cast<int32>(target.height());

    // Coordinate precision: geometry uses double (float64), rasterizer converts to int32
    int32 left   = std::max({0, static_cast<int32>(std::floor(clip.left())), static_cast<int32>(std::floor(rect.left()))});
    int32 top    = std::max({0, static_cast<int32>(std::floor(clip.top())), static_cast<int32>(std::floor(rect.top()))});
    int32 right  = std::min({target_w, static_cast<int32>(std::ceil(clip.right())), static_cast<int32>(std::ceil(rect.right()))});
    int32 bottom = std::min({target_h, static_cast<int32>(std::ceil(clip.bottom())), static_cast<int32>(std::ceil(rect.bottom()))});

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

void rasterize_rounded_rect(RenderTarget& target, const Rect& rect, double radius, const Color& color, const Rect& clip) noexcept {
    const double max_rad = std::min(rect.width(), rect.height()) * 0.5;
    const double rad = std::clamp(radius, 0.0, max_rad);

    if (rad <= 0.5) {
        rasterize_solid_rect(target, rect, color, clip);
        return;
    }

    const int32 target_w = static_cast<int32>(target.width());
    const int32 target_h = static_cast<int32>(target.height());

    int32 left   = std::max({0, static_cast<int32>(std::floor(clip.left())), static_cast<int32>(std::floor(rect.left()))});
    int32 top    = std::max({0, static_cast<int32>(std::floor(clip.top())), static_cast<int32>(std::floor(rect.top()))});
    int32 right  = std::min({target_w, static_cast<int32>(std::ceil(clip.right())), static_cast<int32>(std::ceil(rect.right()))});
    int32 bottom = std::min({target_h, static_cast<int32>(std::ceil(clip.bottom())), static_cast<int32>(std::ceil(rect.bottom()))});

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

// ---------------------------------------------------------------------------
// Embedded 8x16 Bitmap Font for UI Labels & Search Input
// ---------------------------------------------------------------------------
static const uint8_t s_font8x16[128][16] = {
    // 0..31 control characters (empty)
    {0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},
    {0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},

    // 32 ' ' (Space)
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    // 33 '!'
    {0,0,0x18,0x3c,0x3c,0x3c,0x18,0x18,0x18,0,0x18,0x18,0,0,0,0},
    // 34 '"'
    {0,0,0x66,0x66,0x66,0x24,0,0,0,0,0,0,0,0,0,0},
    // 35 '#'
    {0,0,0x66,0x66,0xff,0x66,0x66,0xff,0x66,0x66,0,0,0,0,0,0},
    // 36 '$'
    {0,0,0x18,0x7e,0xdb,0xd8,0x7c,0x1e,0x1b,0xdb,0x7e,0x18,0,0,0,0},
    // 37 '%'
    {0,0,0xc3,0xc6,0xcc,0x18,0x30,0x66,0xc6,0,0,0,0,0,0,0},
    // 38 '&'
    {0,0,0x3c,0x66,0x66,0x3c,0x7a,0xcd,0xcc,0x66,0x3b,0,0,0,0,0},
    // 39 '\''
    {0,0,0x18,0x18,0x30,0,0,0,0,0,0,0,0,0,0,0},
    // 40 '('
    {0,0,0x0c,0x18,0x30,0x30,0x30,0x30,0x30,0x18,0x0c,0,0,0,0,0},
    // 41 ')'
    {0,0,0x30,0x18,0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x30,0,0,0,0,0},
    // 42 '*'
    {0,0,0,0x66,0x3c,0xff,0x3c,0x66,0,0,0,0,0,0,0,0},
    // 43 '+'
    {0,0,0,0x18,0x18,0x7e,0x18,0x18,0,0,0,0,0,0,0,0},
    // 44 ','
    {0,0,0,0,0,0,0,0,0x18,0x18,0x30,0,0,0,0,0},
    // 45 '-'
    {0,0,0,0,0,0x7e,0,0,0,0,0,0,0,0,0,0},
    // 46 '.'
    {0,0,0,0,0,0,0,0,0x18,0x18,0,0,0,0,0,0},
    // 47 '/'
    {0,0,0x03,0x06,0x0c,0x18,0x30,0x60,0xc0,0,0,0,0,0,0,0},
    // 48 '0'
    {0,0,0x3c,0x66,0xc3,0xc3,0xc3,0xc3,0xc3,0x66,0x3c,0,0,0,0,0},
    // 49 '1'
    {0,0,0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x7e,0,0,0,0,0},
    // 50 '2'
    {0,0,0x3c,0x66,0xc3,0x06,0x0c,0x18,0x30,0x60,0xff,0,0,0,0,0},
    // 51 '3'
    {0,0,0x3c,0x66,0xc3,0x0e,0x1c,0x03,0xc3,0x66,0x3c,0,0,0,0,0},
    // 52 '4'
    {0,0,0x06,0x0e,0x1e,0x36,0x66,0xc6,0xff,0x06,0x06,0,0,0,0,0},
    // 53 '5'
    {0,0,0xff,0xc0,0xc0,0xfc,0x06,0x03,0xc3,0x66,0x3c,0,0,0,0,0},
    // 54 '6'
    {0,0,0x3e,0x60,0xc0,0xfc,0xc6,0xc3,0xc3,0x66,0x3c,0,0,0,0,0},
    // 55 '7'
    {0,0,0xff,0xc3,0x06,0x0c,0x18,0x18,0x18,0x18,0x18,0,0,0,0,0},
    // 56 '8'
    {0,0,0x3c,0x66,0xc3,0x66,0x3c,0x66,0xc3,0x66,0x3c,0,0,0,0,0},
    // 57 '9'
    {0,0,0x3c,0x66,0xc3,0xc3,0x67,0x3f,0x03,0x06,0x7c,0,0,0,0,0},
    // 58 ':'
    {0,0,0,0,0x18,0x18,0,0,0x18,0x18,0,0,0,0,0,0},
    // 59 ';'
    {0,0,0,0,0x18,0x18,0,0,0x18,0x18,0x30,0,0,0,0,0},
    // 60 '<'
    {0,0,0x06,0x0c,0x18,0x30,0x60,0x30,0x18,0x0c,0x06,0,0,0,0,0},
    // 61 '='
    {0,0,0,0,0x7e,0,0,0x7e,0,0,0,0,0,0,0,0},
    // 62 '>'
    {0,0,0x60,0x30,0x18,0x0c,0x06,0x0c,0x18,0x30,0x60,0,0,0,0,0},
    // 63 '?'
    {0,0,0x3c,0x66,0xc3,0x0c,0x18,0x18,0,0x18,0x18,0,0,0,0,0},
    // 64 '@'
    {0,0,0x3c,0x66,0xc3,0xdf,0xdd,0xdd,0xde,0x60,0x3c,0,0,0,0,0},
    // 65 'A'
    {0,0,0x18,0x3c,0x66,0xc3,0xc3,0xff,0xc3,0xc3,0xc3,0,0,0,0,0},
    // 66 'B'
    {0,0,0xfc,0x66,0x66,0x7c,0x66,0x66,0x66,0x66,0xfc,0,0,0,0,0},
    // 67 'C'
    {0,0,0x3c,0x66,0xc3,0xc0,0xc0,0xc0,0xc3,0x66,0x3c,0,0,0,0,0},
    // 68 'D'
    {0,0,0xf8,0x6c,0x66,0x66,0x66,0x66,0x66,0x6c,0xf8,0,0,0,0,0},
    // 69 'E'
    {0,0,0xfe,0x62,0x68,0x78,0x68,0x60,0x62,0x62,0xfe,0,0,0,0,0},
    // 70 'F'
    {0,0,0xfe,0x62,0x68,0x78,0x68,0x60,0x60,0x60,0xf0,0,0,0,0,0},
    // 71 'G'
    {0,0,0x3c,0x66,0xc3,0xc0,0xc7,0xc3,0xc3,0x66,0x3e,0,0,0,0,0},
    // 72 'H'
    {0,0,0xc3,0xc3,0xc3,0xc3,0xff,0xc3,0xc3,0xc3,0xc3,0,0,0,0,0},
    // 73 'I'
    {0,0,0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7e,0,0,0,0,0},
    // 74 'J'
    {0,0,0x1f,0x06,0x06,0x06,0x06,0x06,0xc6,0xc6,0x7c,0,0,0,0,0},
    // 75 'K'
    {0,0,0xc3,0xc6,0xcc,0xd8,0xf0,0xd8,0xcc,0xc6,0xc3,0,0,0,0,0},
    // 76 'L'
    {0,0,0xf0,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xfe,0,0,0,0,0},
    // 77 'M'
    {0,0,0xc3,0xe7,0xff,0xdb,0xc3,0xc3,0xc3,0xc3,0xc3,0,0,0,0,0},
    // 78 'N'
    {0,0,0xc3,0xe3,0xf3,0xd3,0xcb,0xc7,0xc3,0xc3,0xc3,0,0,0,0,0},
    // 79 'O'
    {0,0,0x3c,0x66,0xc3,0xc3,0xc3,0xc3,0xc3,0x66,0x3c,0,0,0,0,0},
    // 80 'P'
    {0,0,0xfc,0x66,0x66,0x66,0x7c,0x60,0x60,0x60,0xf0,0,0,0,0,0},
    // 81 'Q'
    {0,0,0x3c,0x66,0xc3,0xc3,0xc3,0xc3,0xd3,0x66,0x3c,0x0e,0,0,0,0},
    // 82 'R'
    {0,0,0xfc,0x66,0x66,0x66,0x7c,0x6c,0x66,0x66,0xc3,0,0,0,0,0},
    // 83 'S'
    {0,0,0x3c,0x66,0xc3,0x60,0x3c,0x06,0xc3,0x66,0x3c,0,0,0,0,0},
    // 84 'T'
    {0,0,0x7e,0x5a,0x18,0x18,0x18,0x18,0x18,0x18,0x3c,0,0,0,0,0},
    // 85 'U'
    {0,0,0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0x66,0x3c,0,0,0,0,0},
    // 86 'V'
    {0,0,0xc3,0xc3,0xc3,0xc3,0xc3,0x66,0x66,0x3c,0x18,0,0,0,0,0},
    // 87 'W'
    {0,0,0xc3,0xc3,0xc3,0xc3,0xdb,0xff,0xe7,0xc3,0xc3,0,0,0,0,0},
    // 88 'X'
    {0,0,0xc3,0x66,0x3c,0x18,0x18,0x3c,0x66,0xc3,0xc3,0,0,0,0,0},
    // 89 'Y'
    {0,0,0xc3,0xc3,0x66,0x3c,0x18,0x18,0x18,0x18,0x3c,0,0,0,0,0},
    // 90 'Z'
    {0,0,0xff,0xc3,0x06,0x0c,0x18,0x30,0x60,0xc3,0xff,0,0,0,0,0},
    // 91 '['
    {0,0,0x3c,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3c,0,0,0,0,0},
    // 92 '\'
    {0,0,0xc0,0x60,0x30,0x18,0x0c,0x06,0x03,0,0,0,0,0,0,0},
    // 93 ']'
    {0,0,0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0,0,0,0,0},
    // 94 '^'
    {0,0,0x18,0x3c,0x66,0xc3,0,0,0,0,0,0,0,0,0,0},
    // 95 '_'
    {0,0,0,0,0,0,0,0,0,0,0,0xff,0,0,0,0},
    // 96 '`'
    {0,0,0x30,0x18,0x0c,0,0,0,0,0,0,0,0,0,0,0},
    // 97 'a'
    {0,0,0,0,0x3c,0x06,0x3e,0x66,0x66,0x3e,0,0,0,0,0,0},
    // 98 'b'
    {0,0,0xf0,0x60,0x6c,0x72,0x66,0x66,0x66,0x72,0xec,0,0,0,0,0},
    // 99 'c'
    {0,0,0,0,0x3c,0x66,0x60,0x60,0x60,0x66,0x3c,0,0,0,0,0},
    // 100 'd'
    {0,0,0x0f,0x03,0x3b,0x47,0x63,0x63,0x63,0x47,0x3b,0,0,0,0,0},
    // 101 'e'
    {0,0,0,0,0x3c,0x66,0x66,0xfe,0x60,0x66,0x3c,0,0,0,0,0},
    // 102 'f'
    {0,0,0x1c,0x36,0x30,0x78,0x30,0x30,0x30,0x30,0x78,0,0,0,0,0},
    // 103 'g'
    {0,0,0,0,0x3b,0x47,0x63,0x63,0x37,0x03,0x36,0x6c,0,0,0,0},
    // 104 'h'
    {0,0,0xf0,0x60,0x6c,0x72,0x66,0x66,0x66,0x66,0x66,0,0,0,0,0},
    // 105 'i'
    {0,0,0x18,0x18,0,0x38,0x18,0x18,0x18,0x18,0x3c,0,0,0,0,0},
    // 106 'j'
    {0,0,0x06,0x06,0,0x0e,0x06,0x06,0x06,0x06,0x66,0x3c,0,0,0,0},
    // 107 'k'
    {0,0,0xf0,0x60,0x66,0x6c,0x78,0x6c,0x66,0x66,0x66,0,0,0,0,0},
    // 108 'l'
    {0,0,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3c,0,0,0,0,0},
    // 109 'm'
    {0,0,0,0,0xec,0xfe,0xd6,0xd6,0xd6,0xd6,0xc6,0,0,0,0,0},
    // 110 'n'
    {0,0,0,0,0xdc,0x66,0x66,0x66,0x66,0x66,0x66,0,0,0,0,0},
    // 111 'o'
    {0,0,0,0,0x3c,0x66,0x66,0x66,0x66,0x66,0x3c,0,0,0,0,0},
    // 112 'p'
    {0,0,0,0,0xdc,0x66,0x66,0x66,0x7c,0x60,0xf0,0,0,0,0,0},
    // 113 'q'
    {0,0,0,0,0x3b,0x63,0x63,0x63,0x3f,0x03,0x0f,0,0,0,0,0},
    // 114 'r'
    {0,0,0,0,0xde,0x76,0x60,0x60,0x60,0x60,0xf0,0,0,0,0,0},
    // 115 's'
    {0,0,0,0,0x3e,0x60,0x3c,0x06,0x06,0x6c,0x38,0,0,0,0,0},
    // 116 't'
    {0,0,0x10,0x30,0x7c,0x30,0x30,0x30,0x30,0x34,0x18,0,0,0,0,0},
    // 117 'u'
    {0,0,0,0,0x66,0x66,0x66,0x66,0x66,0x66,0x3b,0,0,0,0,0},
    // 118 'v'
    {0,0,0,0,0x66,0x66,0x66,0x66,0x66,0x3c,0x18,0,0,0,0,0},
    // 119 'w'
    {0,0,0,0,0xc3,0xc3,0xd6,0xd6,0xfe,0xfe,0x6c,0,0,0,0,0},
    // 120 'x'
    {0,0,0,0,0xc3,0x66,0x3c,0x18,0x3c,0x66,0xc3,0,0,0,0,0},
    // 121 'y'
    {0,0,0,0,0x66,0x66,0x66,0x66,0x3e,0x06,0x0c,0x70,0,0,0,0},
    // 122 'z'
    {0,0,0,0,0x7e,0x66,0x0c,0x18,0x30,0x66,0x7e,0,0,0,0,0},
    // 123 '{'
    {0,0,0x0e,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x0e,0,0,0,0,0},
    // 124 '|'
    {0,0,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0,0,0,0,0},
    // 125 '}'
    {0,0,0x70,0x18,0x18,0x18,0x0e,0x18,0x18,0x18,0x70,0,0,0,0,0},
    // 126 '~'
    {0,0,0x76,0xdc,0,0,0,0,0,0,0,0,0,0,0,0},
    // 127
    {0}
};
namespace {
    FT_Library g_ft_library   = nullptr;
    FT_Face    g_ft_face_ui   = nullptr; // Inter (proportional) — primary UI font
    FT_Face    g_ft_face_mono = nullptr; // DejaVuSansMono — terminal/code font
    bool g_ft_initialized = false;
    bool g_ft_failed      = false;

    struct GlyphCacheKey {
        uint32_t codepoint;
        int size;
        bool bold;
        uint8_t font_family; // 0=UI(Inter), 1=Monospace(DejaVu)
        bool operator==(const GlyphCacheKey& other) const {
            return codepoint == other.codepoint && size == other.size
                && bold == other.bold && font_family == other.font_family;
        }
    };

    struct GlyphCacheKeyHash {
        std::size_t operator()(const GlyphCacheKey& k) const {
            return std::hash<uint32_t>()(k.codepoint)
                ^ (std::hash<int>()(k.size) << 1)
                ^ (std::hash<bool>()(k.bold) << 2)
                ^ (std::hash<uint8_t>()(k.font_family) << 3);
        }
    };

    struct CachedGlyph {
        std::vector<uint8_t> bitmap;
        int width;
        int rows;
        int pitch;
        int bitmap_left;
        int bitmap_top;
        int advance_x;
    };

    constexpr size_t MAX_CACHED_GLYPHS = 2000;
    std::list<GlyphCacheKey> g_glyph_lru;
    std::unordered_map<GlyphCacheKey, std::pair<CachedGlyph, std::list<GlyphCacheKey>::iterator>, GlyphCacheKeyHash> g_glyph_cache;

    size_t g_cache_hits = 0;
    size_t g_cache_misses = 0;

    void init_freetype() {
        if (g_ft_initialized || g_ft_failed) return;
        g_ft_initialized = true;

        if (FT_Init_FreeType(&g_ft_library)) {
            txui::log_message(txui::LogLevel::Error, "PixmanBackend: Could not init FreeType library");
            g_ft_failed = true;
            return;
        }

        // ── UI Font: Inter (hardcoded, bundled) ──────────────────────────────────
        // No fallback chain. If this fails, g_ft_face_ui = nullptr,
        // UI text falls back to bitmap font (s_font8x16). Explicit logged failure.
        const char* inter_path = "/usr/share/tinexus/fonts/Inter-Regular.ttf";
        if (FT_New_Face(g_ft_library, inter_path, 0, &g_ft_face_ui) != 0) {
            txui::log_message(txui::LogLevel::Error,
                "PixmanBackend: FONT MISSING — Inter-Regular.ttf not found at " +
                std::string(inter_path) +
                ". UI text will use fallback bitmap font until font is staged.");
            g_ft_face_ui = nullptr;
        } else {
            txui::log_message(txui::LogLevel::Info,
                std::string("PixmanBackend: Loaded UI font: ") + inter_path);
        }

        // ── Monospace Font: DejaVuSansMono (fallback chain — terminal callers) ──
        const char* mono_paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf"
        };
        for (const char* path : mono_paths) {
            if (FT_New_Face(g_ft_library, path, 0, &g_ft_face_mono) == 0) {
                txui::log_message(txui::LogLevel::Info,
                    std::string("PixmanBackend: Loaded Monospace font: ") + path);
                break;
            }
        }
        if (!g_ft_face_mono) {
            txui::log_message(txui::LogLevel::Warn,
                "PixmanBackend: No monospace font found. Terminal text rendering disabled.");
        }
    }

    const CachedGlyph* get_cached_glyph(uint32_t codepoint, int size, bool bold, uint8_t font_family) {
        GlyphCacheKey key{codepoint, size, bold, font_family};
        auto it = g_glyph_cache.find(key);
        if (it != g_glyph_cache.end()) {
            g_cache_hits++;
            // Move to front of LRU
            g_glyph_lru.splice(g_glyph_lru.begin(), g_glyph_lru, it->second.second);
            return &it->second.first;
        }
        g_cache_misses++;
        if (g_cache_misses % 1000 == 0) {
            txui::log_message(txui::LogLevel::Info, "Glyph Cache: hits=" + std::to_string(g_cache_hits) + ", misses=" + std::to_string(g_cache_misses) + ", size=" + std::to_string(g_glyph_cache.size()));
        }

        // Select face based on font family
        FT_Face active_face = (font_family == static_cast<uint8_t>(FontFamily::Monospace))
            ? g_ft_face_mono
            : g_ft_face_ui;

        if (!active_face) return nullptr; // Font not loaded — caller falls back to bitmap

        if (FT_Set_Pixel_Sizes(active_face, 0, static_cast<FT_UInt>(size))) {
            return nullptr;
        }

        FT_UInt glyph_index = FT_Get_Char_Index(active_face, static_cast<FT_ULong>(codepoint));
        if (FT_Load_Glyph(active_face, glyph_index, FT_LOAD_RENDER)) {
            return nullptr;
        }

        CachedGlyph glyph;
        FT_Bitmap* bitmap = &active_face->glyph->bitmap;

        glyph.width       = static_cast<int>(bitmap->width);
        glyph.rows        = static_cast<int>(bitmap->rows);
        glyph.pitch       = static_cast<int>(bitmap->pitch);
        glyph.bitmap_left = static_cast<int>(active_face->glyph->bitmap_left);
        glyph.bitmap_top  = static_cast<int>(active_face->glyph->bitmap_top);
        glyph.advance_x   = static_cast<int>(active_face->glyph->advance.x >> 6);

        if (bitmap->buffer && bitmap->rows > 0 && bitmap->pitch > 0) {
            glyph.bitmap.assign(bitmap->buffer, bitmap->buffer + (static_cast<std::size_t>(bitmap->rows) * static_cast<std::size_t>(bitmap->pitch)));
        }

        if (g_glyph_cache.size() >= MAX_CACHED_GLYPHS) {
            auto last = g_glyph_lru.back();
            g_glyph_lru.pop_back();
            g_glyph_cache.erase(last);
        }

        g_glyph_lru.push_front(key);
        g_glyph_cache[key] = {std::move(glyph), g_glyph_lru.begin()};
        return &g_glyph_cache[key].first;
    }
}

void rasterize_text(RenderTarget& target, const Point& pos, const std::string& text,
                    const Color& color, double scale, bool bold, bool italic,
                    const Rect& clip, FontFamily font_family) noexcept {
    init_freetype();

    // Select active face; nullptr means font not loaded → graceful no-op
    FT_Face active_face = (font_family == FontFamily::Monospace) ? g_ft_face_mono : g_ft_face_ui;
    if (!active_face) return;

    const uint8_t ff = static_cast<uint8_t>(font_family);

    // Handle both old scale multipliers (e.g. 1.0, 2.0) and new explicit point sizes (e.g. 14, 15)
    int size = (scale <= 5.0) ? static_cast<int>(std::round(16.0 * scale)) : static_cast<int>(std::round(scale));
    size = std::max(1, size);
    
    const int32 target_w = static_cast<int32>(target.width());
    const int32 target_h = static_cast<int32>(target.height());
    uint32* buffer = target.data();
    if (!buffer) return;

    const uint32 solid_argb = color.to_argb32_premultiplied();
    const bool is_opaque = (color.a() == 255U);

    int32 pen_x = static_cast<int32>(std::floor(pos.x));
    int32 pen_y = static_cast<int32>(std::floor(pos.y)) + size; // Baseline adjustment

    int32 clip_left   = std::max(0, static_cast<int32>(clip.left()));
    int32 clip_top    = std::max(0, static_cast<int32>(clip.top()));
    int32 clip_right  = std::min(target_w, static_cast<int32>(std::ceil(clip.right())));
    int32 clip_bottom = std::min(target_h, static_cast<int32>(std::ceil(clip.bottom())));

    for (size_t i = 0; i < text.size(); ) {
        uint32_t codepoint = 0;
        uint8_t c = text[i];
        if (c < 0x80) {
            codepoint = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= text.size()) break;
            codepoint = ((c & 0x1F) << 6) | (text[i+1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= text.size()) break;
            codepoint = ((c & 0x0F) << 12) | ((text[i+1] & 0x3F) << 6) | (text[i+2] & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= text.size()) break;
            codepoint = ((c & 0x07) << 18) | ((text[i+1] & 0x3F) << 12) | ((text[i+2] & 0x3F) << 6) | (text[i+3] & 0x3F);
            i += 4;
        } else {
            i += 1;
            continue;
        }

        const CachedGlyph* glyph = get_cached_glyph(codepoint, size, bold, ff);
        if (!glyph) continue;

        int32 draw_x = pen_x + glyph->bitmap_left;
        int32 draw_y = pen_y - glyph->bitmap_top;

        for (int32 row = 0; row < glyph->rows; ++row) {
            int32 py = draw_y + row;
            if (py < clip_top || py >= clip_bottom) continue;

            uint32* row_ptr = buffer + (static_cast<size_t>(py) * static_cast<size_t>(target_w));
            for (int32 col = 0; col < glyph->width; ++col) {
                int32 px = draw_x + col;
                if (px < clip_left || px >= clip_right) continue;

                uint8_t alpha = glyph->bitmap[static_cast<std::size_t>(row * glyph->pitch + col)];
                if (alpha == 0) continue;

                if (alpha == 255 && is_opaque) {
                    row_ptr[px] = solid_argb;
                } else {
                    uint32 combined_a = (color.a() * alpha) / 255;
                    uint32 src_r = (color.r() * combined_a) / 255;
                    uint32 src_g = (color.g() * combined_a) / 255;
                    uint32 src_b = (color.b() * combined_a) / 255;
                    uint32 premult_src = (combined_a << 24) | (src_r << 16) | (src_g << 8) | src_b;
                    
                    row_ptr[px] = blend_argb32_premultiplied(row_ptr[px], premult_src);
                }
            }
        }
        pen_x += glyph->advance_x;
    }
}

} // namespace (freetype anonymous)

namespace {

// ---------------------------------------------------------------------------
// Linear Gradient Rasterizer
// Interpolates linearly from color_start to color_end across the rect.
// ---------------------------------------------------------------------------
void rasterize_gradient_rect(RenderTarget& target, const Rect& rect,
                              const Color& cs, const Color& ce,
                              bool horizontal, const Rect& clip) noexcept {
    const int32 target_w = static_cast<int32>(target.width());
    const int32 target_h = static_cast<int32>(target.height());

    int32 left   = std::max({0, static_cast<int32>(std::floor(clip.left())), static_cast<int32>(std::floor(rect.left()))});
    int32 top    = std::max({0, static_cast<int32>(std::floor(clip.top())), static_cast<int32>(std::floor(rect.top()))});
    int32 right  = std::min({target_w, static_cast<int32>(std::ceil(clip.right())), static_cast<int32>(std::ceil(rect.right()))});
    int32 bottom = std::min({target_h, static_cast<int32>(std::ceil(clip.bottom())), static_cast<int32>(std::ceil(rect.bottom()))});

    if (left >= right || top >= bottom) return;

    const double span = horizontal
        ? std::max(1.0, rect.width())
        : std::max(1.0, rect.height());

    uint32* buffer = target.data();
    for (int32 y = top; y < bottom; ++y) {
        uint32* row = buffer + static_cast<size_t>(y) * static_cast<size_t>(target_w);
        const double vy = horizontal ? 0.0 : (y + 0.5 - rect.top()) / span;

        for (int32 x = left; x < right; ++x) {
            const double t = horizontal
                ? std::clamp((x + 0.5 - rect.left()) / span, 0.0, 1.0)
                : std::clamp(vy, 0.0, 1.0);

            const uint32 r = static_cast<uint32>(cs.r() + t * (static_cast<double>(ce.r()) - cs.r()));
            const uint32 g = static_cast<uint32>(cs.g() + t * (static_cast<double>(ce.g()) - cs.g()));
            const uint32 b = static_cast<uint32>(cs.b() + t * (static_cast<double>(ce.b()) - cs.b()));
            const uint32 a = static_cast<uint32>(cs.a() + t * (static_cast<double>(ce.a()) - cs.a()));

            Color px(static_cast<uint8>(r), static_cast<uint8>(g),
                     static_cast<uint8>(b), static_cast<uint8>(a));
            const uint32 src = px.to_argb32_premultiplied();

            if (a == 255) {
                row[x] = src;
            } else {
                row[x] = blend_argb32_premultiplied(row[x], src);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Gradient Rounded Rect Rasterizer (gradient + corner rounding combined)
// ---------------------------------------------------------------------------
void rasterize_gradient_rounded_rect(RenderTarget& target, const Rect& rect, double radius,
                                     const Color& cs, const Color& ce,
                                     bool horizontal, const Rect& clip) noexcept {
    const double max_rad = std::min(rect.width(), rect.height()) * 0.5;
    const double rad = std::clamp(radius, 0.0, max_rad);

    if (rad <= 0.5) {
        rasterize_gradient_rect(target, rect, cs, ce, horizontal, clip);
        return;
    }

    const int32 target_w = static_cast<int32>(target.width());
    const int32 target_h = static_cast<int32>(target.height());

    int32 left   = std::max({0, static_cast<int32>(std::floor(clip.left())), static_cast<int32>(std::floor(rect.left()))});
    int32 top    = std::max({0, static_cast<int32>(std::floor(clip.top())), static_cast<int32>(std::floor(rect.top()))});
    int32 right  = std::min({target_w, static_cast<int32>(std::ceil(clip.right())), static_cast<int32>(std::ceil(rect.right()))});
    int32 bottom = std::min({target_h, static_cast<int32>(std::ceil(clip.bottom())), static_cast<int32>(std::ceil(rect.bottom()))});

    if (left >= right || top >= bottom) return;

    const double rx1 = rect.left()  + rad;
    const double ry1 = rect.top()   + rad;
    const double rx2 = rect.right() - rad;
    const double ry2 = rect.bottom()- rad;

    const double span = horizontal
        ? std::max(1.0, rect.width())
        : std::max(1.0, rect.height());

    uint32* buffer = target.data();
    for (int32 y = top; y < bottom; ++y) {
        uint32* row = buffer + static_cast<size_t>(y) * static_cast<size_t>(target_w);
        const double cy = y + 0.5;
        const double vy = horizontal ? 0.0 : (cy - rect.top()) / span;

        for (int32 x = left; x < right; ++x) {
            const double cx = x + 0.5;
            double dist = 0.0;

            if      (cx < rx1 && cy < ry1) dist = std::hypot(rx1 - cx, ry1 - cy);
            else if (cx > rx2 && cy < ry1) dist = std::hypot(cx - rx2, ry1 - cy);
            else if (cx < rx1 && cy > ry2) dist = std::hypot(rx1 - cx, cy - ry2);
            else if (cx > rx2 && cy > ry2) dist = std::hypot(cx - rx2, cy - ry2);

            double cov = 1.0;
            if (dist > 0.0) {
                if      (dist >= rad + 0.7071) { cov = 0.0; }
                else if (dist >  rad - 0.7071) {
                    double d = rad - dist;
                    cov = std::clamp((d + 0.7071) / 1.4142, 0.0, 1.0);
                    cov = cov * cov * (3.0 - 2.0 * cov);
                }
            }
            if (cov <= 0.0) continue;

            const double t = horizontal
                ? std::clamp((cx - rect.left()) / span, 0.0, 1.0)
                : std::clamp(vy, 0.0, 1.0);

            const uint32 r = static_cast<uint32>(cs.r() + t * (static_cast<double>(ce.r()) - cs.r()));
            const uint32 g = static_cast<uint32>(cs.g() + t * (static_cast<double>(ce.g()) - cs.g()));
            const uint32 b = static_cast<uint32>(cs.b() + t * (static_cast<double>(ce.b()) - cs.b()));
            uint32 a       = static_cast<uint32>(cs.a() + t * (static_cast<double>(ce.a()) - cs.a()));
            a = static_cast<uint32>(static_cast<double>(a) * cov);

            Color px(static_cast<uint8>(r), static_cast<uint8>(g),
                     static_cast<uint8>(b), static_cast<uint8>(a));
            row[x] = blend_argb32_premultiplied(row[x], px.to_argb32_premultiplied());
        }
    }
}

// ---------------------------------------------------------------------------
// Circle Rasterizer (filled or stroke) with sub-pixel AA
// ---------------------------------------------------------------------------
void rasterize_circle(RenderTarget& target, const Rect& clip, const Point& center, double radius,
                      const Color& color, double stroke_width) noexcept {
    const int32 target_w = static_cast<int32>(target.width());
    const int32 target_h = static_cast<int32>(target.height());

    const double outer_r  = radius;
    const double inner_r  = (stroke_width > 0.0) ? std::max(0.0, radius - stroke_width) : 0.0;

    int32 left   = std::max({0, static_cast<int32>(std::floor(clip.left())), static_cast<int32>(std::floor(center.x - outer_r - 1.0))});
    int32 top    = std::max({0, static_cast<int32>(std::floor(clip.top())), static_cast<int32>(std::floor(center.y - outer_r - 1.0))});
    int32 right  = std::min({target_w, static_cast<int32>(std::ceil(clip.right())), static_cast<int32>(std::ceil(center.x + outer_r + 1.0))});
    int32 bottom = std::min({target_h, static_cast<int32>(std::ceil(clip.bottom())), static_cast<int32>(std::ceil(center.y + outer_r + 1.0))});

    if (left >= right || top >= bottom) {
        return;
    }

    uint32* buffer = target.data();

    for (int32 y = top; y < bottom; ++y) {
        uint32* row = buffer + static_cast<size_t>(y) * static_cast<size_t>(target_w);
        const double dy = (y + 0.5) - center.y;

        for (int32 x = left; x < right; ++x) {
            const double dx = (x + 0.5) - center.x;
            const double dist = std::sqrt(dx * dx + dy * dy);

            // Coverage on outer edge (AA)
            double outer_cov = std::clamp(outer_r - dist + 0.7071, 0.0, 1.4142) / 1.4142;
            outer_cov = outer_cov * outer_cov * (3.0 - 2.0 * outer_cov); // smoothstep

            if (outer_cov <= 0.0) continue;

            double cov = outer_cov;
            if (inner_r > 0.0) {
                // Hollow: subtract inner region with AA
                double inner_cov = std::clamp(inner_r - dist + 0.7071, 0.0, 1.4142) / 1.4142;
                inner_cov = inner_cov * inner_cov * (3.0 - 2.0 * inner_cov);
                cov = outer_cov - inner_cov;
                cov = std::clamp(cov, 0.0, 1.0);
            }

            if (cov <= 0.0) continue;

            const uint32 a = static_cast<uint32>(static_cast<double>(color.a()) * cov);
            if (a == 0) continue;

            Color px(color.r(), color.g(), color.b(), static_cast<uint8>(a));
            row[x] = blend_argb32_premultiplied(row[x], px.to_argb32_premultiplied());
        }
    }
}

void rasterize_image(RenderTarget& target, const Rect& rect,
                     const std::shared_ptr<std::vector<uint32_t>>& pixels,
                     uint32_t src_w, uint32_t src_h,
                     const Rect& clip) noexcept {
    if (!pixels || src_w == 0 || src_h == 0 || rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    const int32 target_w = static_cast<int32>(target.width());
    const int32 target_h = static_cast<int32>(target.height());

    int32 left   = std::max({0, static_cast<int32>(std::floor(clip.left())), static_cast<int32>(std::floor(rect.left()))});
    int32 top    = std::max({0, static_cast<int32>(std::floor(clip.top())), static_cast<int32>(std::floor(rect.top()))});
    int32 right  = std::min({target_w, static_cast<int32>(std::ceil(clip.right())), static_cast<int32>(std::ceil(rect.right()))});
    int32 bottom = std::min({target_h, static_cast<int32>(std::ceil(clip.bottom())), static_cast<int32>(std::ceil(rect.bottom()))});

    if (left >= right || top >= bottom) {
        return;
    }

    const double scale_x = static_cast<double>(src_w) / rect.width();
    const double scale_y = static_cast<double>(src_h) / rect.height();
    const double rect_x  = rect.left();
    const double rect_y  = rect.top();

    const uint32_t* src_buf = pixels->data();
    uint32* dst_buf = target.data();

    for (int32 y = top; y < bottom; ++y) {
        uint32_t src_y = static_cast<uint32_t>(std::clamp((y - rect_y) * scale_y, 0.0, static_cast<double>(src_h - 1)));
        const uint32_t* src_row = src_buf + (src_y * src_w);
        uint32* dst_row = dst_buf + (static_cast<std::size_t>(y) * static_cast<std::size_t>(target_w));

        for (int32 x = left; x < right; ++x) {
            uint32_t src_x = static_cast<uint32_t>(std::clamp((x - rect_x) * scale_x, 0.0, static_cast<double>(src_w - 1)));
            uint32_t src_pixel = src_row[src_x];
            dst_row[x] = blend_argb32_premultiplied(dst_row[x], src_pixel);
        }
    }
}

} // namespace (anonymous)

void PixmanBackend::execute(const CommandBuffer& buffer, RenderTarget& target) {
    TXUI_ASSERT(!buffer.is_empty(), "Cannot execute empty CommandBuffer");

    // Stateless renderer execution loop
    Color current_color = Color::white();
    std::vector<Rect> clip_stack;
    Rect current_clip(0, 0, target.width(), target.height());

    for (const auto& command : buffer.commands()) {
        std::visit([&](auto&& cmd) {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (::std::is_same_v<T, BeginFrameCommand>) {
                // Begin frame command processing
            } else if constexpr (::std::is_same_v<T, EndFrameCommand>) {
                // End frame command processing
            } else if constexpr (::std::is_same_v<T, SetBrushCommand>) {
                std::visit([&](const SolidBrush& solid) {
                    current_color = solid.color();
                }, cmd.brush);
            } else if constexpr (::std::is_same_v<T, ClearCommand>) {
                uint32 argb = cmd.color.to_argb32_premultiplied();
                uint32* pixel_buf = target.data();
                std::fill_n(pixel_buf, target.width() * target.height(), argb);
            } else if constexpr (::std::is_same_v<T, DrawRectCommand>) {
                Color rect_color = current_color;
                std::visit([&](const SolidBrush& solid) {
                    rect_color = solid.color();
                }, cmd.brush);

                rasterize_solid_rect(target, cmd.rect, rect_color, current_clip);
            } else if constexpr (::std::is_same_v<T, DrawRoundedRectCommand>) {
                Color rrect_color = current_color;
                std::visit([&](const SolidBrush& solid) {
                    rrect_color = solid.color();
                }, cmd.brush);

                rasterize_rounded_rect(target, cmd.rect, cmd.radius, rrect_color, current_clip);
            } else if constexpr (::std::is_same_v<T, DrawGradientRectCommand>) {
                rasterize_gradient_rect(target, cmd.rect, cmd.color_start, cmd.color_end, cmd.horizontal, current_clip);
            } else if constexpr (::std::is_same_v<T, DrawGradientRoundedRectCommand>) {
                rasterize_gradient_rounded_rect(target, cmd.rect, cmd.radius, cmd.color_start, cmd.color_end, cmd.horizontal, current_clip);
            } else if constexpr (::std::is_same_v<T, DrawCircleCommand>) {
                rasterize_circle(target, current_clip, cmd.center, cmd.radius, cmd.color, cmd.stroke_width);
            } else if constexpr (::std::is_same_v<T, DrawTextCommand>) {
                rasterize_text(target, cmd.pos, cmd.text, cmd.color, cmd.scale, cmd.bold, cmd.italic, current_clip, cmd.font_family);
            } else if constexpr (::std::is_same_v<T, DrawLineCommand>) {
                if (cmd.p1.x == cmd.p2.x) {
                    // Vertical line
                    double y1 = std::min(cmd.p1.y, cmd.p2.y);
                    double y2 = std::max(cmd.p1.y, cmd.p2.y);
                    double half_w = cmd.thickness * 0.5;
                    rasterize_solid_rect(target, Rect(cmd.p1.x - half_w, y1, cmd.thickness, y2 - y1), cmd.color, current_clip);
                } else if (cmd.p1.y == cmd.p2.y) {
                    // Horizontal line
                    double x1 = std::min(cmd.p1.x, cmd.p2.x);
                    double x2 = std::max(cmd.p1.x, cmd.p2.x);
                    double half_w = cmd.thickness * 0.5;
                    rasterize_solid_rect(target, Rect(x1, cmd.p1.y - half_w, x2 - x1, cmd.thickness), cmd.color, current_clip);
                } else {
                    // Proper arbitrary line drawing using Bresenham algorithm with thickness and alpha blending
                    int x0 = static_cast<int>(std::round(cmd.p1.x));
                    int y0 = static_cast<int>(std::round(cmd.p1.y));
                    int x1 = static_cast<int>(std::round(cmd.p2.x));
                    int y1 = static_cast<int>(std::round(cmd.p2.y));

                    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
                    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
                    int err = dx + dy, e2;

                    int rad = static_cast<int>(std::max(0.0, std::round(cmd.thickness * 0.5 - 0.5)));
                    const int tw = static_cast<int>(target.width());
                    const int th = static_cast<int>(target.height());
                    uint32* buf = target.data();
                    const uint32 src = cmd.color.to_argb32_premultiplied();
                    const uint32 sa = (src >> 24) & 0xFF;
                    const uint32 inv_sa = 255 - sa;

                    while (true) {
                        for (int rx = -rad; rx <= rad; ++rx) {
                            for (int ry = -rad; ry <= rad; ++ry) {
                                int px = x0 + rx, py = y0 + ry;
                                if (px >= current_clip.left() && px < current_clip.right() &&
                                    py >= current_clip.top() && py < current_clip.bottom() &&
                                    px >= 0 && px < tw && py >= 0 && py < th) {
                                    if (sa == 255) {
                                        buf[py * tw + px] = src;
                                    } else if (sa > 0) {
                                        uint32 dst = buf[py * tw + px];
                                        uint32 da = (dst >> 24) & 0xFF;
                                        uint32 r = ((src >> 16) & 0xFF) + (((dst >> 16) & 0xFF) * inv_sa + 127) / 255;
                                        uint32 g = ((src >> 8) & 0xFF) + (((dst >> 8) & 0xFF) * inv_sa + 127) / 255;
                                        uint32 b = (src & 0xFF) + ((dst & 0xFF) * inv_sa + 127) / 255;
                                        uint32 a = sa + ((da * inv_sa + 127) / 255);
                                        buf[py * tw + px] = (a << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
                                    }
                                }
                            }
                        }
                        if (x0 == x1 && y0 == y1) break;
                        e2 = 2 * err;
                        if (e2 >= dy) { err += dy; x0 += sx; }
                        if (e2 <= dx) { err += dx; y0 += sy; }
                    }
                }
            } else if constexpr (::std::is_same_v<T, DrawImageCommand>) {
                rasterize_image(target, cmd.rect, cmd.pixels, cmd.width, cmd.height, current_clip);
            } else if constexpr (::std::is_same_v<T, PushClipCommand>) {
                clip_stack.push_back(current_clip);
                // Compute intersection of two Rects manually (Rect has no intersection() method)
                {
                    const Rect& a = current_clip;
                    const Rect& b = cmd.rect;
                    double ix = std::max(a.left(), b.left());
                    double iy = std::max(a.top(),  b.top());
                    double iw = std::min(a.right(),  b.right())  - ix;
                    double ih = std::min(a.bottom(), b.bottom()) - iy;
                    current_clip = (iw > 0 && ih > 0) ? Rect(ix, iy, iw, ih) : Rect(0,0,0,0);
                }
            } else if constexpr (::std::is_same_v<T, PopClipCommand>) {
                if (!clip_stack.empty()) {
                    current_clip = clip_stack.back();
                    clip_stack.pop_back();
                }
            } else if constexpr (::std::is_same_v<T, PushTransformCommand>) {
                // Not needed for MVP, coordinates are absolute
            } else if constexpr (::std::is_same_v<T, PopTransformCommand>) {
                // Not needed for MVP
            }
        }, command);
    }
}

} // namespace txui
