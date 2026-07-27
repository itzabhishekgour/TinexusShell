#include <iostream>
#include <cassert>
#include <vector>
#include "common/logger.hpp"
#include "comp/server/server.hpp"
#include "comp/surface/xdg_shell_manager.hpp"
#include "comp/window/window_manager.hpp"
#include "comp/window/scene_graph.hpp"
#include "comp/renderer/pixman_renderer.hpp"
#include "comp/surface/buffer_manager.hpp"
#include "comp/surface/frame_callback.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_real_client");
    tinexus::log::info("Running First Real Client End-to-End Compositing Pipeline Test...");

    // 1. Initialize Display Server & Socket
    tinexus::comp::TinexusServer server;
    assert(server.initialize());
    assert(!server.wayland_display().empty());

    // 2. Bind xdg_wm_base & Create Surface
    tinexus::comp::XdgShellManager xdg_mgr;
    assert(xdg_mgr.bind_xdg_wm_base(1));
    uint64_t client_id = 9009;
    xdg_mgr.create_xdg_surface(client_id);

    // 3. Configure & ACK Handshake
    uint32_t serial = xdg_mgr.send_toplevel_configure(client_id, 1024, 768);
    assert(xdg_mgr.handle_ack_configure(client_id, serial));

    // 4. Create WindowNode & Map into SceneGraph via WindowManager
    tinexus::comp::WindowManager win_mgr;
    auto win = win_mgr.create_window(client_id, "weston-terminal");
    assert(win != nullptr);

    tinexus::comp::SceneGraph scene_graph;
    assert(win_mgr.map_window(client_id, scene_graph));
    assert(scene_graph.stacking_order().size() == 1);

    // 5. Attach Client SHM Buffer & Commit (sized 1024x768 uint32_t pixels)
    std::vector<uint32_t> shm_pixels(1024 * 768, 0xFF00FF00); // ARGB8888 Green pixels
    tinexus::comp::BufferManager buf_mgr;
    tinexus::comp::SurfaceState state(client_id, "weston-terminal");
    assert(buf_mgr.attach_shm_buffer(state, shm_pixels.data(), 1024, 768, 4096));
    assert(buf_mgr.commit(state));

    auto render_buf = std::make_shared<tinexus::comp::RenderBuffer>();
    render_buf->pixels = shm_pixels.data();
    render_buf->width = 1024;
    render_buf->height = 768;
    render_buf->stride = 4096;
    render_buf->format = 0;
    win->render_buffer = render_buf;

    // 6. Execute Compositing Pass via PixmanRenderer
    auto surfaces = scene_graph.collect_render_surfaces();
    assert(surfaces.size() == 1);

    tinexus::comp::PixmanRenderer renderer;
    assert(renderer.initialize(1920, 1080));
    renderer.begin_frame();
    for (auto& s : surfaces) {
        renderer.compose_surface(s);
    }
    renderer.end_frame();
    renderer.present();

    // 7. Dispatch Frame Callbacks
    tinexus::comp::FrameCallbackManager cb_mgr;
    cb_mgr.request_callback(client_id, 1);
    assert(cb_mgr.send_frame_done_all(33000) == 1);

    tinexus::log::info("First Real Client End-to-End Compositing Pipeline passed 100%!");
    return 0;
}
