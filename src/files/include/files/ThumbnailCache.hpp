#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <cstdint>

namespace tinexus::files {

struct ImageThumbnail {
    std::shared_ptr<std::vector<uint32_t>> pixels;
    uint32_t width{0};
    uint32_t height{0};
};

class ThumbnailCache {
public:
    static ThumbnailCache& instance();

    // Get or decode thumbnail for an image path at requested max dimension (e.g. 96 for grid, 360 for gallery/quicklook)
    std::shared_ptr<ImageThumbnail> get_thumbnail(const std::filesystem::path& path, uint32_t target_max_dim);

    // Clear memory cache
    void clear();

private:
    ThumbnailCache() = default;
    std::unordered_map<std::string, std::shared_ptr<ImageThumbnail>> m_cache;
};

} // namespace tinexus::files
