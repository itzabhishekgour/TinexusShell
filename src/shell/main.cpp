#include "client/client_state.hpp"
#include "client/layer_surface.hpp"
#include "common/logger.hpp"

#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    tinexus::log::info("Tinexus Shell (Wayland Client) starting...");

    ClientState state;
    bool connected = false;
    for (int attempt = 1; attempt <= 20; ++attempt) {
        if (state.init()) {
            connected = true;
            break;
        }
        tinexus::log::info("Waiting for Wayland compositor display (attempt {}/20)...", attempt);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (!connected) {
        tinexus::log::error("Failed to initialize Wayland client state after 20 attempts.");
        return 1;
    }

    // Create the Top Panel
    LayerSurface top_panel(state);
    top_panel.create_top_panel();

    tinexus::log::info("Entering Wayland event loop...");

    // Dispatch events forever
    while (wl_display_dispatch(state.display) != -1) {
        // Events are handled in callbacks
    }

    state.cleanup();
    tinexus::log::info("Tinexus Shell exiting.");
    return 0;
}
