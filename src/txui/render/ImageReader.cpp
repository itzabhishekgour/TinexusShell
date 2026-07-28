#include <txui/render/ImageReader.hpp>
#include <fstream>
#include <vector>
#include <array>

namespace txui {

namespace {

[[nodiscard]] uint32 read_be32(const uint8* ptr) noexcept {
    return (static_cast<uint32>(ptr[0]) << 24) |
           (static_cast<uint32>(ptr[1]) << 16) |
           (static_cast<uint32>(ptr[2]) << 8)  |
           static_cast<uint32>(ptr[3]);
}

} // namespace

std::optional<CanvasRenderTarget> ImageReader::load_png(std::string_view filepath) noexcept {
    std::ifstream file(filepath.data(), std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    // 1. Verify PNG Magic Signature
    constexpr std::array<uint8, 8> signature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    std::array<uint8, 8> file_sig{};
    file.read(reinterpret_cast<char*>(file_sig.data()), file_sig.size());
    if (file_sig != signature) {
        return std::nullopt;
    }

    uint32 width = 0;
    uint32 height = 0;
    std::vector<uint8> deflated_data;

    // 2. Read Chunks
    while (file.good()) {
        uint8 len_bytes[4];
        if (!file.read(reinterpret_cast<char*>(len_bytes), 4)) {
            break;
        }
        uint32 chunk_len = read_be32(len_bytes);

        char type[5] = {0, 0, 0, 0, 0};
        file.read(type, 4);

        std::vector<uint8> chunk_data(chunk_len);
        if (chunk_len > 0) {
            file.read(reinterpret_cast<char*>(chunk_data.data()), static_cast<std::streamsize>(chunk_len));
        }

        uint8 crc_bytes[4];
        file.read(reinterpret_cast<char*>(crc_bytes), 4);

        if (std::string_view(type) == "IHDR" && chunk_len >= 8) {
            width  = read_be32(chunk_data.data());
            height = read_be32(chunk_data.data() + 4);
        } else if (std::string_view(type) == "IDAT") {
            deflated_data.insert(deflated_data.end(), chunk_data.begin(), chunk_data.end());
        } else if (std::string_view(type) == "IEND") {
            break;
        }
    }

    if (width == 0 || height == 0 || deflated_data.size() < 6) {
        return std::nullopt;
    }

    // 3. Decompress uncompressed zlib deflate blocks (FLG 0x78 0x01 written by ImageWriter)
    // Check zlib header
    if (deflated_data[0] != 0x78) {
        return std::nullopt;
    }

    std::vector<uint8> raw_data;
    std::size_t offset = 2; // Skip CMF and FLG bytes
    while (offset + 4 <= deflated_data.size()) {
        uint8 bfinal = deflated_data[offset++];
        uint16 len  = static_cast<uint16>(deflated_data[offset]) |
                      static_cast<uint16>(static_cast<uint16>(deflated_data[offset + 1]) << 8);
        offset += 4; // Skip len (2 bytes) + nlen (2 bytes)

        if (offset + len > deflated_data.size() - 4) { // Save 4 bytes for Adler32 at end
            return std::nullopt;
        }

        raw_data.insert(raw_data.end(), deflated_data.begin() + static_cast<std::ptrdiff_t>(offset),
                                        deflated_data.begin() + static_cast<std::ptrdiff_t>(offset + len));
        offset += len;
        if ((bfinal & 0x01U) != 0U) {
            break;
        }
    }

    const std::size_t expected_raw_bytes = static_cast<std::size_t>(height) * (1 + static_cast<std::size_t>(width) * 4);
    if (raw_data.size() < expected_raw_bytes) {
        return std::nullopt;
    }

    // 4. Reconstruct CanvasRenderTarget pixels
    CanvasRenderTarget target(width, height);
    std::size_t src_idx = 0;
    for (uint32 y = 0; y < height; ++y) {
        src_idx++; // Skip None filter byte (0x00)
        for (uint32 x = 0; x < width; ++x) {
            uint32 r = raw_data[src_idx++];
            uint32 g = raw_data[src_idx++];
            uint32 b = raw_data[src_idx++];
            uint32 a = raw_data[src_idx++];
            uint32 argb = (a << 24) | (r << 16) | (g << 8) | b;
            target.pixels()[static_cast<std::size_t>(y) * width + x] = argb;
        }
    }

    return target;
}

} // namespace txui
