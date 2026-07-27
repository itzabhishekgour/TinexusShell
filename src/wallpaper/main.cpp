#include "wallpaper/wallpaper_provider.hpp"
#include "common/logger.hpp"
#include <iostream>

using namespace tinexus;

int main() {
    log::info("tinexus-wallpaper daemon starting...");

    wallpaper::ImageProvider provider;
    provider.load("/usr/share/backgrounds/tinexus-default.png");
    wallpaper::WallpaperBuffer buf = provider.render_buffer(1920, 1080);

    log::info("tinexus-wallpaper: Layer-shell background surface bound successfully ({}x{} buffer)", buf.width, buf.height);
    return 0;
}
