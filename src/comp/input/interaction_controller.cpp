#include "comp/input/interaction_controller.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

void InteractionController::start_move(uint64_t surface_id, int32_t start_x, int32_t start_y) {
    m_state = InteractionState::Moving;
    m_surface_id = surface_id;
    m_edge = ResizeEdge::None;
    m_start_x = start_x;
    m_start_y = start_y;
    log::info("InteractionController: Started interactive move for Surface #{} at ({},{})", surface_id, start_x, start_y);
}

void InteractionController::start_resize(uint64_t surface_id, ResizeEdge edge, int32_t start_x, int32_t start_y) {
    m_state = InteractionState::Resizing;
    m_surface_id = surface_id;
    m_edge = edge;
    m_start_x = start_x;
    m_start_y = start_y;
    log::info("InteractionController: Started interactive resize for Surface #{} at ({},{})", surface_id, start_x, start_y);
}

void InteractionController::update_drag(int32_t current_x, int32_t current_y, int32_t& out_x, int32_t& out_y, int32_t& out_w, int32_t& out_h) {
    int32_t dx = current_x - m_start_x;
    int32_t dy = current_y - m_start_y;

    if (m_state == InteractionState::Moving) {
        out_x += dx;
        out_y += dy;
        m_start_x = current_x;
        m_start_y = current_y;
    } else if (m_state == InteractionState::Resizing) {
        out_w += dx;
        out_h += dy;
        m_start_x = current_x;
        m_start_y = current_y;
    }
}

void InteractionController::finish_operation() {
    log::info("InteractionController: Finished interactive operation for Surface #{}", m_surface_id);
    m_state = InteractionState::Idle;
    m_surface_id = 0;
    m_edge = ResizeEdge::None;
}

void InteractionController::cancel_operation() {
    log::info("InteractionController: Cancelled interactive operation for Surface #{}", m_surface_id);
    m_state = InteractionState::Idle;
    m_surface_id = 0;
    m_edge = ResizeEdge::None;
}

} // namespace tinexus::comp
