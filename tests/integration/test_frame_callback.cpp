#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/surface/frame_callback.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_frame_callback");
    tinexus::log::info("Running Frame Callback Integration Test...");

    tinexus::comp::FrameCallbackManager cb_mgr;
    cb_mgr.request_callback(1001, 1);
    cb_mgr.request_callback(1001, 2);
    assert(cb_mgr.pending_callbacks_count() == 2);

    size_t dispatched = cb_mgr.send_frame_done_all(16700);
    assert(dispatched == 2);
    assert(cb_mgr.pending_callbacks_count() == 0);

    tinexus::log::info("[PASS] Frame Callback Integration Test");
    return 0;
}
