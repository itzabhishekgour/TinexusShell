#include <txui/render/CanvasRenderTarget.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/ImageWriter.hpp>
#include <txui/render/ImageReader.hpp>
#include <iostream>
#include <cstdlib>
#include <cstdio>

int main() {
    std::cout << "=== Tinexus Golden PNG Roundtrip Verification ===" << std::endl;

    txui::CanvasRenderTarget target1(256, 256);
    target1.clear(txui::Color(20, 30, 40, 255));

    txui::CommandBuffer buffer;
    txui::Painter painter(buffer);
    painter.begin_frame();
    painter.fill_rect(txui::Rect(20.0, 20.0, 100.0, 100.0), txui::Color(220, 50, 80, 255));
    painter.fill_rect(txui::Rect(80.0, 80.0, 120.0, 120.0), txui::Color(50, 180, 220, 180));
    painter.end_frame();

    txui::PixmanBackend backend;
    backend.execute(buffer, target1);

    txui::uint64 orig_hash = target1.hash_fnv1a();

    const char* golden_file = "test_golden_roundtrip.png";
    if (!txui::ImageWriter::save_png(target1, golden_file)) {
        std::cerr << "FAIL: Could not write golden PNG file to " << golden_file << std::endl;
        return EXIT_FAILURE;
    }

    auto loaded_opt = txui::ImageReader::load_png(golden_file);
    if (!loaded_opt.has_value()) {
        std::cerr << "FAIL: Could not load golden PNG file from " << golden_file << std::endl;
        std::remove(golden_file);
        return EXIT_FAILURE;
    }

    const txui::CanvasRenderTarget& target2 = loaded_opt.value();
    txui::uint64 reloaded_hash = target2.hash_fnv1a();

    std::remove(golden_file); // Clean up temp file

    if (orig_hash != reloaded_hash) {
        std::cerr << "FAIL: Golden PNG roundtrip hash mismatch! Orig=" << std::hex << orig_hash
                  << " != Reloaded=" << reloaded_hash << std::dec << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "PASS: Golden PNG roundtrip verified! Pixel buffer FNV-1a hash matches exactly: 0x"
              << std::hex << orig_hash << std::dec << std::endl;
    return EXIT_SUCCESS;
}
