#include <txui/render/ImageWriter.hpp>
#include <fstream>
#include <vector>
#include <array>
#include <span>

namespace txui {

namespace {

[[nodiscard]] uint32 calculate_crc32(std::span<const uint8> data) noexcept {
    static constexpr std::array<uint32, 256> crc_table = []() {
        std::array<uint32, 256> table{};
        for (uint32 i = 0; i < 256; ++i) {
            uint32 c = i;
            for (int k = 0; k < 8; ++k) {
                if (c & 1) {
                    c = 0xEDB88320U ^ (c >> 1);
                } else {
                    c = c >> 1;
                }
            }
            table[i] = c;
        }
        return table;
    }();

    uint32 crc = 0xFFFFFFFFU;
    for (uint8 byte : data) {
        crc = crc_table[(crc ^ byte) & 0xFFU] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

[[nodiscard]] uint32 calculate_adler32(std::span<const uint8> data) noexcept {
    uint32 a = 1;
    uint32 b = 0;
    for (uint8 byte : data) {
        a = (a + byte) % 65521U;
        b = (b + a)    % 65521U;
    }
    return (b << 16) | a;
}

void write_be32(std::vector<uint8>& out, uint32 value) {
    out.push_back(static_cast<uint8>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8>((value >> 8)  & 0xFF));
    out.push_back(static_cast<uint8>(value         & 0xFF));
}

void write_chunk(std::vector<uint8>& out, const char* type, std::span<const uint8> data) {
    write_be32(out, static_cast<uint32>(data.size()));
    const std::size_t type_start = out.size();
    out.push_back(static_cast<uint8>(type[0]));
    out.push_back(static_cast<uint8>(type[1]));
    out.push_back(static_cast<uint8>(type[2]));
    out.push_back(static_cast<uint8>(type[3]));
    out.insert(out.end(), data.begin(), data.end());

    uint32 crc = calculate_crc32(std::span<const uint8>(out.data() + type_start, 4 + data.size()));
    write_be32(out, crc);
}

} // namespace

bool ImageWriter::save_png(const RenderTarget& target, std::string_view filepath) noexcept {
    std::ofstream file(filepath.data(), std::ios::binary);
    if (!file) {
        return false;
    }

    const uint32 w = target.width();
    const uint32 h = target.height();

    // 1. PNG Magic Signature
    constexpr std::array<uint8, 8> signature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    file.write(reinterpret_cast<const char*>(signature.data()), signature.size());

    std::vector<uint8> png_data;

    // 2. IHDR Chunk (13 bytes)
    std::vector<uint8> ihdr;
    write_be32(ihdr, w);
    write_be32(ihdr, h);
    ihdr.push_back(8); // Bit depth
    ihdr.push_back(6); // Color type: RGBA (6)
    ihdr.push_back(0); // Compression method: deflate
    ihdr.push_back(0); // Filter method
    ihdr.push_back(0); // Interlace method
    write_chunk(png_data, "IHDR", ihdr);

    // 3. Prepare scanline filtered data (filter byte 0 + RGBA for each line)
    std::vector<uint8> raw_data;
    raw_data.reserve(static_cast<std::size_t>(h) * (1 + static_cast<std::size_t>(w) * 4));
    for (uint32 y = 0; y < h; ++y) {
        raw_data.push_back(0); // None filter
        for (uint32 x = 0; x < w; ++x) {
            uint32 argb = target.pixel_at(x, y);
            raw_data.push_back(static_cast<uint8>((argb >> 16) & 0xFF)); // R
            raw_data.push_back(static_cast<uint8>((argb >> 8)  & 0xFF)); // G
            raw_data.push_back(static_cast<uint8>(argb         & 0xFF)); // B
            raw_data.push_back(static_cast<uint8>((argb >> 24) & 0xFF)); // A
        }
    }

    // 4. Build zlib deflate stream with uncompressed blocks (max 65535 bytes per block)
    std::vector<uint8> idat;
    idat.push_back(0x78); // CMF: Deflate, 32KB window
    idat.push_back(0x01); // FLG: FCHECK=1, no dictionary

    std::size_t remaining = raw_data.size();
    std::size_t offset = 0;
    while (remaining > 0) {
        uint16 block_size = static_cast<uint16>(std::min<std::size_t>(remaining, 65535));
        bool is_final = (remaining <= 65535);
        idat.push_back(is_final ? 0x01 : 0x00);
        idat.push_back(static_cast<uint8>(block_size & 0xFF));
        idat.push_back(static_cast<uint8>((block_size >> 8) & 0xFF));
        uint16 nlen = ~block_size;
        idat.push_back(static_cast<uint8>(nlen & 0xFF));
        idat.push_back(static_cast<uint8>((nlen >> 8) & 0xFF));

        idat.insert(idat.end(), raw_data.begin() + static_cast<std::ptrdiff_t>(offset),
                                raw_data.begin() + static_cast<std::ptrdiff_t>(offset + block_size));
        offset += block_size;
        remaining -= block_size;
    }

    uint32 adler = calculate_adler32(raw_data);
    write_be32(idat, adler);
    write_chunk(png_data, "IDAT", idat);

    // 5. IEND Chunk
    write_chunk(png_data, "IEND", {});

    file.write(reinterpret_cast<const char*>(png_data.data()), static_cast<std::streamsize>(png_data.size()));
    return file.good();
}

} // namespace txui
