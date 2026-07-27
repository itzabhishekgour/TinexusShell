#ifndef TINEXUS_COMP_SEAT_MANAGER_HPP
#define TINEXUS_COMP_SEAT_MANAGER_HPP

#include "comp/input/keymap_engine.hpp"
#include "comp/input/interaction_controller.hpp"
#include <string>
#include <cstdint>

namespace tinexus::comp {

class SeatManager {
public:
    SeatManager() = default;
    ~SeatManager() = default;

    bool bind_seat(const std::string& seat_name = "seat0");

    void send_pointer_enter(uint64_t surface_id, int32_t x, int32_t y);
    void send_pointer_motion(int32_t x, int32_t y);
    void send_pointer_leave(uint64_t surface_id);
    void send_button_click(uint32_t button, uint32_t state);

    void send_keyboard_enter(uint64_t surface_id);
    void send_keyboard_leave(uint64_t surface_id);
    void send_key_event(uint32_t key, uint32_t state);

    [[nodiscard]] KeymapEngine& keymap() noexcept { return m_keymap; }
    [[nodiscard]] InteractionController& interaction() noexcept { return m_interaction; }
    [[nodiscard]] uint64_t focused_surface_id() const noexcept { return m_focused_surface_id; }
    [[nodiscard]] bool seat_bound() const noexcept { return m_seat_bound; }

private:
    std::string m_seat_name{"seat0"};
    bool m_seat_bound{false};
    uint64_t m_focused_surface_id{0};
    int32_t m_pointer_x{0};
    int32_t m_pointer_y{0};

    KeymapEngine m_keymap;
    InteractionController m_interaction;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SEAT_MANAGER_HPP
