#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include <string.h>
#include <thread>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "../wallpaper/include/wallpaper/stb_image.h"

// Color definitions (BGRA format, commonly used in Linux framebuffers)
struct Color {
    uint8_t b, g, r, a;
};

const Color BG_COLOR = {0, 0, 0, 255}; // Black
const Color BAR_BG = {255, 255, 255, 255}; // White
const Color BAR_FG = {144, 238, 144, 255}; // Light Green

void draw_rect(uint8_t* fb, int fb_width, int fb_height, int bpp, int x, int y, int w, int h, Color c) {
    int bytes_per_pixel = bpp / 8;
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= fb_height) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= fb_width) continue;
            
            size_t index = (j * fb_width + i) * bytes_per_pixel;
            if (bytes_per_pixel == 4) {
                fb[index] = c.b;
                fb[index + 1] = c.g;
                fb[index + 2] = c.r;
                fb[index + 3] = c.a;
            } else if (bytes_per_pixel == 3) {
                fb[index] = c.b;
                fb[index + 1] = c.g;
                fb[index + 2] = c.r;
            }
        }
    }
}

void draw_image(uint8_t* fb, int fb_width, int fb_height, int bpp, int fb_x, int fb_y, uint8_t* img_data, int img_w, int img_h, int img_channels) {
    int bytes_per_pixel = bpp / 8;
    for (int j = 0; j < img_h; j++) {
        int screen_y = fb_y + j;
        if (screen_y < 0 || screen_y >= fb_height) continue;

        for (int i = 0; i < img_w; i++) {
            int screen_x = fb_x + i;
            if (screen_x < 0 || screen_x >= fb_width) continue;

            size_t fb_index = (screen_y * fb_width + screen_x) * bytes_per_pixel;
            size_t img_index = (j * img_w + i) * img_channels;

            uint8_t r = img_data[img_index];
            uint8_t g = img_data[img_index + 1];
            uint8_t b = img_data[img_index + 2];
            uint8_t a = (img_channels == 4) ? img_data[img_index + 3] : 255;

            // Simple alpha blending over black background
            if (a < 255) {
                float alpha = a / 255.0f;
                r = static_cast<uint8_t>(r * alpha);
                g = static_cast<uint8_t>(g * alpha);
                b = static_cast<uint8_t>(b * alpha);
            }

            if (bytes_per_pixel == 4) {
                fb[fb_index] = b;
                fb[fb_index + 1] = g;
                fb[fb_index + 2] = r;
                fb[fb_index + 3] = 255;
            } else if (bytes_per_pixel == 3) {
                fb[fb_index] = b;
                fb[fb_index + 1] = g;
                fb[fb_index + 2] = r;
            }
        }
    }
}

int main() {
    const char* fb_path = "/dev/fb0";
    int fbfd = open(fb_path, O_RDWR);
    if (fbfd == -1) {
        std::cerr << "Error: cannot open framebuffer device." << std::endl;
        return 1;
    }

    struct fb_var_screeninfo vinfo;
    for (int attempts = 0; attempts < 50; attempts++) {
        if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
            std::cerr << "Error reading variable information." << std::endl;
            close(fbfd);
            return 1;
        }
        if (vinfo.xres > 0 && vinfo.yres > 0 && vinfo.bits_per_pixel > 0) {
            break;
        }
        usleep(100000); // Wait 100ms
    }

    if (vinfo.xres == 0 || vinfo.yres == 0) {
        std::cerr << "Framebuffer resolution is 0x0. Aborting." << std::endl;
        close(fbfd);
        return 1;
    }
    size_t screensize = vinfo.yres_virtual * vinfo.xres_virtual * vinfo.bits_per_pixel / 8;
    uint8_t* fbp = (uint8_t*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((intptr_t)fbp == -1) {
        std::cerr << "Error: failed to map framebuffer device to memory." << std::endl;
        close(fbfd);
        return 1;
    }

    // Clear screen to black
    memset(fbp, 0, screensize);

    // Load logo — initialize to 0 to avoid UB if stbi_load fails
    int img_w = 0, img_h = 0, img_channels = 0;
    uint8_t* img_data = stbi_load("/tinexus-logo.png", &img_w, &img_h, &img_channels, 4);
    if (!img_data) img_data = stbi_load("/usr/share/tinexus/tinexus-logo.png", &img_w, &img_h, &img_channels, 4);
    if (!img_data) img_data = stbi_load("assets/logo/tinexus-logo.png", &img_w, &img_h, &img_channels, 4);
    if (img_data) {
        int x = (static_cast<int>(vinfo.xres) - img_w) / 2;
        int y = (static_cast<int>(vinfo.yres) - img_h) / 2 - 50; // Shift up slightly for progress bar
        draw_image(fbp, vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, x, y, img_data, img_w, img_h, 4);
        stbi_image_free(img_data);
    } else {
        std::cerr << "Warning: Could not load /tinexus-logo.png" << std::endl;
        FILE* f = fopen("/dev/ttyS0", "w");
        if (f) { fprintf(f, "SPLASH: Could not load /tinexus-logo.png\\n"); fclose(f); }
    }

    // Progress bar geometry — safe even if img_h is 0 (logo failed to load)
    const int bar_w = 200;
    const int bar_h = 6;
    const int bar_x = (static_cast<int>(vinfo.xres) - bar_w) / 2;
    // Place bar 40px below where the logo bottom would be, or at 65% screen height as fallback
    const int logo_bottom = (static_cast<int>(vinfo.yres) - img_h) / 2 - 50 + img_h;
    const int bar_y_from_logo = logo_bottom + 40;
    const int bar_y_fallback  = static_cast<int>(vinfo.yres * 65 / 100);
    const int bar_y = (img_h > 0) ? bar_y_from_logo : bar_y_fallback;

    // Draw white progress bar track
    draw_rect(fbp, vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, bar_x, bar_y, bar_w, bar_h, BAR_BG);

    // ── Indeterminate animation: fill 0→100% over 50 × 50ms = 2.5 seconds ────
    // Exits when either:
    //   (a) 100% animation completes naturally, or
    //   (b) SIGTERM is received from serviced (compositor is up)
    constexpr int TOTAL_FRAMES = 50;
    for (int frame = 0; frame <= TOTAL_FRAMES; ++frame) {
        int fill_w = (bar_w * frame) / TOTAL_FRAMES;
        if (fill_w > 0) {
            draw_rect(fbp, vinfo.xres, vinfo.yres, vinfo.bits_per_pixel,
                      bar_x, bar_y, fill_w, bar_h, BAR_FG);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Hold at 100% briefly before compositor takes over
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    munmap(fbp, screensize);
    close(fbfd);

    return 0;
}

