#ifndef TINEXUS_COMP_INTERACTION_CONTROLLER_HPP
#define TINEXUS_COMP_INTERACTION_CONTROLLER_HPP

#include <cstdint>

namespace tinexus::comp {

enum class InteractionState {
    Idle,
    Moving,
    Resizing,
    Finished
};

enum class ResizeEdge {
    None,
    Top,
    Bottom,
    Left,
    Right,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

class InteractionController {
public:
    InteractionController() = default;

    void start_move(uint64_t surface_id, int32_t start_x, int32_t start_y);
    void start_resize(uint64_t surface_id, ResizeEdge edge, int32_t start_x, int32_t start_y);
    void update_drag(int32_t current_x, int32_t current_y, int32_t& out_x, int32_t& out_y, int32_t& out_w, int32_t& out_h);
    void finish_operation();
    void cancel_operation();

    [[nodiscard]] InteractionState state() const noexcept { return m_state; }
    [[nodiscard]] uint64_t active_surface_id() const noexcept { return m_surface_id; }
    [[nodiscard]] ResizeEdge active_edge() const noexcept { return m_edge; }

private:
    InteractionState m_state{InteractionState::Idle};
    uint64_t m_surface_id{0};
    ResizeEdge m_edge{ResizeEdge::None};
    int32_t m_start_x{0};
    int32_t m_start_y{0};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_INTERACTION_CONTROLLER_HPP
