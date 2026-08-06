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

void rasterize_text(RenderTarget& target, const Point& pos, const std::string& text, const Color& color, double scale) noexcept {
    const int32 target_w = static_cast<int32>(target.width());
    const int32 target_h = static_cast<int32>(target.height());
    uint32* buffer = target.data();
    if (!buffer) return;

    const uint32 solid_argb = color.to_argb32_premultiplied();
    const bool is_opaque = (color.a() == 255U);
    const int32 s = std::max(1, static_cast<int32>(std::round(scale)));

    int32 start_x = static_cast<int32>(std::floor(pos.x));
    int32 start_y = static_cast<int32>(std::floor(pos.y));

    for (size_t i = 0; i < text.size(); ++i) {
        uint8_t ch = static_cast<uint8_t>(text[i]);
        if (ch > 127) ch = '?';

        int32 char_x = start_x + static_cast<int32>(i) * (8 * s);
        if (char_x >= target_w) break;

        const uint8_t* glyph = s_font8x16[ch];

        for (int32 row = 0; row < 16; ++row) {
            int32 py = start_y + row * s;
            if (py < 0 || py >= target_h) continue;

            uint8_t bits = glyph[row];
            if (bits == 0) continue;

            for (int32 col = 0; col < 8; ++col) {
                if (bits & (1 << (7 - col))) {
                    int32 px = char_x + col * s;
                    if (px < 0 || px >= target_w) continue;

                    for (int32 dy = 0; dy < s; ++dy) {
                        int32 ny = py + dy;
                        if (ny >= target_h) break;
                        uint32* row_ptr = buffer + (static_cast<size_t>(ny) * static_cast<size_t>(target_w));
                        for (int32 dx = 0; dx < s; ++dx) {
                            int32 nx = px + dx;
                            if (nx >= target_w) break;
                            if (is_opaque) {
                                row_ptr[nx] = solid_argb;
                            } else {
                                row_ptr[nx] = blend_argb32_premultiplied(row_ptr[nx], solid_argb);
                            }
                        }
                    }
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
            } else if constexpr (std::is_same_v<T, DrawTextCommand>) {
                rasterize_text(target, cmd.position, cmd.text, cmd.color, cmd.scale);
            }
        }, command);
    }
}

} // namespace txui
