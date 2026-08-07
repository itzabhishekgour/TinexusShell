#include <iostream>
#include "wallpaper/wallpaper_provider.hpp"

int main() {
    tinexus::wallpaper::ImageProvider provider;
    if (provider.load("Temp/tinexus-default.jpg")) {
        auto buf = provider.render_buffer(1920, 1080);
        std::cout << "SUCCESS RENDERED BUFFER: " << buf.width << "x" << buf.height << " pixels=" << buf.pixels.size() << std::endl;
    } else {
        std::cout << "LOAD FAILED!" << std::endl;
    }
    return 0;
}
