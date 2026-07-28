#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/wayland/WaylandConnection.hpp>
#include <txui/wayland/WaylandBuffer.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <iostream>
#include <cstdlib>

int main() {
    std::cout << "=== Phase 4.2.3 Unit Test: WaylandRenderTarget & Double Buffering ===" << std::endl;

    // 1. Verify anonymous memfd_create SHM buffer allocation and pixel memory access
    auto shm_buf_opt = txui::wayland::WaylandBuffer::create(nullptr, 256, 256);
    if (!shm_buf_opt.has_value() || !shm_buf_opt->is_valid()) {
        std::cerr << "FAIL: Could not allocate anonymous memfd SHM buffer" << std::endl;
        return EXIT_FAILURE;
    }

    shm_buf_opt->clear(0xFFFF0000U); // Clear with red
    if (shm_buf_opt->data()[0] != 0xFFFF0000U || shm_buf_opt->data()[256 * 256 - 1] != 0xFFFF0000U) {
        std::cerr << "FAIL: memfd SHM buffer memory read/write verification failed" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: Linux memfd_create anonymous SHM buffer verification succeeded" << std::endl;

    // 2. Attempt connection to real Wayland server
    auto conn_opt = txui::wayland::WaylandConnection::connect();
    if (!conn_opt.has_value()) {
        std::cout << "SKIP: No running Wayland compositor detected in current environment. "
                  << "memfd SHM buffer & memory layer verified." << std::endl;
        std::cout << "=== Phase 4.2.3 SHM CORE VERIFIED SUCCESSFULLY ===" << std::endl;
        return EXIT_SUCCESS;
    }

    txui::wayland::WaylandConnection& conn = conn_opt.value();
    std::cout << "PASS: Connected to Wayland display server" << std::endl;

    // 3. Verify double-buffered WaylandRenderTarget creation and ping-pong swap
    auto target_opt = txui::WaylandRenderTarget::create(conn, 300, 300);
    if (!target_opt.has_value() || !target_opt->is_valid()) {
        std::cerr << "FAIL: Could not create double-buffered WaylandRenderTarget" << std::endl;
        return EXIT_FAILURE;
    }

    txui::WaylandRenderTarget& target = target_opt.value();
    if (target.back_index() != 0 || target.front_index() != 1) {
        std::cerr << "FAIL: Initial buffer indices incorrect" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: Double-buffered WaylandRenderTarget created (Front=1, Back=0)" << std::endl;

    // 4. Render frame to back buffer using PixmanBackend and present
    txui::CommandBuffer buffer;
    txui::Painter painter(buffer);
    painter.begin_frame();
    painter.fill_rounded_rect(txui::Rect(20.0, 20.0, 260.0, 260.0), 16.0, txui::Color(255, 0, 0, 255));
    painter.end_frame();

    txui::PixmanBackend backend;
    backend.execute(buffer, target);

    target.present();

    if (target.back_index() != 1 || target.front_index() != 0) {
        std::cerr << "FAIL: Buffer indices did not swap after present!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: Wayland surface commit & double-buffer ping-pong swap verified (Front=0, Back=1)" << std::endl;

    std::cout << "=== Phase 4.2.3 ALL WAYLAND RENDERTARGET CRITERIA PASSED ===" << std::endl;
    return EXIT_SUCCESS;
}
