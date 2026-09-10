#ifndef TINEXUS_WALLPAPER_PROVIDER_HPP
#define TINEXUS_WALLPAPER_PROVIDER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace tinexus::wallpaper {

struct WallpaperBuffer {
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint32_t> pixels; // ARGB8888
};

class IWallpaperProvider {
public:
    virtual ~IWallpaperProvider() = default;

    [[nodiscard]] virtual std::string type_name() const noexcept = 0;
    [[nodiscard]] virtual bool load(const std::string& uri) = 0;
    [[nodiscard]] virtual WallpaperBuffer render_buffer(uint32_t target_width, uint32_t target_height) = 0;
};

class ImageProvider : public IWallpaperProvider {
public:
    ImageProvider() = default;
    ~ImageProvider() override = default;

    std::string type_name() const noexcept override { return "ImageProvider"; }
    bool load(const std::string& path) override;
    WallpaperBuffer render_buffer(uint32_t target_width, uint32_t target_height) override;

private:
    std::string m_path;
    uint32_t m_src_width{0};
    uint32_t m_src_height{0};
    std::vector<uint32_t> m_src_pixels;
};

} // namespace tinexus::wallpaper

#endif // TINEXUS_WALLPAPER_PROVIDER_HPP
