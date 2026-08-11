#pragma once

#include "terminal/TerminalEmulator.hpp"
#include "terminal/pty_process.hpp"
#include <txui/widgets/Widget.hpp>
#include <txui/math/Rect.hpp>
#include <txui/render/Painter.hpp>
#include <memory>
#include <string>
#include <vector>

namespace tinexus::terminal {

class TerminalWidget : public txui::Widget {
private:
    std::unique_ptr<TerminalEmulator> m_emulator;
    PtyProcess m_pty;
    
    // Configurable visuals
    txui::Color m_default_bg;
    txui::Color m_default_fg;
    
    // Grid sizes
    int m_rows{24};
    int m_cols{80};
    
    double m_cell_width{10.0};
    double m_cell_height{20.0};

    // Cursor state (updated from movecursor_cb via on_cursor_move)
    int m_cursor_row{0};
    int m_cursor_col{0};
    mutable bool m_blink_state{true};   // true = cursor visible on this blink phase

    // Text selection state (live-view only — guard: scroll_offset must be 0)
    bool m_selecting{false};
    int m_sel_start_row{-1};
    int m_sel_start_col{-1};
    int m_sel_end_row{-1};
    int m_sel_end_col{-1};
    
    txui::Color get_txui_color(uint8_t r, uint8_t g, uint8_t b) const {
        return txui::Color(r, g, b, 255);
    }

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

public:
    bool handle_event(const txui::Event& event) noexcept override;
    TerminalWidget() noexcept;
    ~TerminalWidget() override;

    // Direct access to Pty to wire up FD
    PtyProcess& pty() { return m_pty; }
    TerminalEmulator& emulator() { return *m_emulator; }
    int rows() const noexcept { return m_rows; }
    int cols() const noexcept { return m_cols; }

    // Called by the event loop or a blink timer on each blink interval (~530ms).
    // Toggles the cursor blink state and marks only the cursor cell dirty.
    void tick_blink() noexcept;

private:
    mutable bool m_first_paint{true};

    // Called from TerminalEmulator::movecursor_cb. Updates cursor pos and marks
    // both old and new cursor cells dirty (to erase old + draw new).
    void on_cursor_move(int new_row, int new_col) noexcept;

    // Returns true if cell (row, col) falls within the current selection range.
    // Handles multi-row selections and normalized start/end order.
    [[nodiscard]] bool cell_in_selection(int row, int col) const noexcept;

    // Cancels any in-progress selection and marks the selected area dirty.
    void cancel_selection() noexcept;
};

} // namespace tinexus::terminal
