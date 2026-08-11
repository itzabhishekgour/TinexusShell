#include "TerminalWidget.hpp"
#include <txui/math/Rect.hpp>
#include <txui/math/Size.hpp>
#include <cmath>
#include <algorithm>

namespace tinexus::terminal {

TerminalWidget::TerminalWidget() noexcept 
    : m_default_bg(30, 30, 30, 255), m_default_fg(200, 200, 200, 255) {
    m_emulator = std::make_unique<TerminalEmulator>(m_rows, m_cols);
    m_emulator->on_output = [this](const char* data, size_t len) {
        m_pty.write_bytes(data, len);
    };
    m_emulator->on_damage = [this]() {
        mark_needs_paint();
    };
    m_emulator->on_cursor_move = [this](int row, int col) {
        on_cursor_move(row, col);
    };
    m_pty.spawn("/bin/sh", {"-i", "-l"});
}

TerminalWidget::~TerminalWidget() {
    m_pty.terminate();
}

txui::Size TerminalWidget::measure_override(const txui::Constraints& constraints) noexcept {
    // Simply request space for our grid
    double desired_width = m_cols * m_cell_width;
    double desired_height = m_rows * m_cell_height;
    
    // Optional: if constraints allow more, we could expand and resize emulator
    // But for Stage 3 MVP, we'll just return desired size and constrain it.
    
    txui::Size size(desired_width, desired_height);
    return constraints.constrain(size);
}

void TerminalWidget::paint_override(txui::Painter& painter) const noexcept {
    // Immediate mode renderer: we must redraw everything because the render target is cleared.
    const_cast<TerminalEmulator*>(m_emulator.get())->clear_dirty_rects();

    // --- Rounded frame background + drop shadow (Stage 7) ---
    // Draw shadow slightly offset, then draw the rounded terminal background.
    const txui::Rect& f = frame();
    
    // Drop shadow (offset 2px down, semi-transparent black)
    txui::Rect shadow_rect(f.x() + 1.0, f.y() + 3.0, f.width(), f.height());
    painter.fill_rounded_rect(shadow_rect, 8.0, txui::Color(0, 0, 0, 55));
    // Main terminal background
    painter.fill_rounded_rect(f, 8.0, m_default_bg);
    // 1px inner border (slightly lighter than bg)
    painter.fill_rounded_rect(f, 8.0, txui::Color(70, 70, 70, 180));
    // Re-fill interior to restore bg (border is just the 1px edge)
    txui::Rect inner(f.x() + 1.0, f.y() + 1.0, f.width() - 2.0, f.height() - 2.0);
    painter.fill_rounded_rect(inner, 7.0, m_default_bg);
    
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            auto cell = m_emulator->get_cell(r, c);
            
            // Calculate cell bounds
            double x = frame().x() + c * m_cell_width;
            double y = frame().y() + r * m_cell_height;
            txui::Rect cell_rect(x, y, m_cell_width, m_cell_height);
            
            uint8_t bg_r = cell.attrs.bg_red, bg_g = cell.attrs.bg_green, bg_b = cell.attrs.bg_blue;
            uint8_t fg_r = cell.attrs.fg_red, fg_g = cell.attrs.fg_green, fg_b = cell.attrs.fg_blue;

            if (cell.attrs.reverse) {
                std::swap(bg_r, fg_r);
                std::swap(bg_g, fg_g);
                std::swap(bg_b, fg_b);
            }

            // Draw background if not default
            if (bg_r != 0 || bg_g != 0 || bg_b != 0) {
                painter.fill_rect(cell_rect, get_txui_color(bg_r, bg_g, bg_b));
            }
            
            if (cell.codepoint != 0 && cell.codepoint != ' ') {
                txui::Color fg_color = get_txui_color(fg_r, fg_g, fg_b);
                
                std::string text;
                uint32_t cp = cell.codepoint;
                if (cp <= 0x7F) {
                    text += static_cast<char>(cp);
                } else if (cp <= 0x7FF) {
                    text += static_cast<char>(0xC0 | (cp >> 6));
                    text += static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp <= 0xFFFF) {
                    text += static_cast<char>(0xE0 | (cp >> 12));
                    text += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    text += static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp <= 0x10FFFF) {
                    text += static_cast<char>(0xF0 | (cp >> 18));
                    text += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    text += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    text += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    text += '?';
                }
                
                txui::Point text_pos(x, y);
                bool bold = cell.attrs.bold != 0;
                bool italic = cell.attrs.italic != 0;
                
                painter.draw_text(text_pos, text, fg_color, 1.0, bold, italic);
            }

            // Selection highlight overlay (live-view only)
            if (m_emulator->scroll_offset() == 0 && cell_in_selection(r, c)) {
                painter.fill_rect(cell_rect, txui::Color(100, 180, 255, 80));
            }
        }
    }

    // Block cursor (live-view only, blink-aware)
    if (m_emulator->scroll_offset() == 0 && m_blink_state
            && m_cursor_row >= 0 && m_cursor_row < m_rows
            && m_cursor_col >= 0 && m_cursor_col < m_cols) {
        double cx = frame().x() + m_cursor_col * m_cell_width;
        double cy = frame().y() + m_cursor_row * m_cell_height;
        // Draw block cursor using inverted foreground color
        auto cursor_cell = m_emulator->get_cell(m_cursor_row, m_cursor_col);
        txui::Color cursor_color = get_txui_color(
            cursor_cell.attrs.fg_red, cursor_cell.attrs.fg_green, cursor_cell.attrs.fg_blue);
        painter.fill_rect(txui::Rect(cx, cy, m_cell_width, m_cell_height), cursor_color);
        // Re-draw text in bg color so the glyph is still readable under the cursor
        if (cursor_cell.codepoint != 0 && cursor_cell.codepoint != ' ') {
            std::string text;
            uint32_t cp = cursor_cell.codepoint;
            if (cp <= 0x7F) { text += static_cast<char>(cp); }
            else if (cp <= 0x7FF) {
                text += static_cast<char>(0xC0 | (cp >> 6));
                text += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp <= 0xFFFF) {
                text += static_cast<char>(0xE0 | (cp >> 12));
                text += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                text += static_cast<char>(0x80 | (cp & 0x3F));
            }
            txui::Color text_on_cursor = get_txui_color(
                cursor_cell.attrs.bg_red, cursor_cell.attrs.bg_green, cursor_cell.attrs.bg_blue);
            painter.draw_text(txui::Point(cx, cy), text, text_on_cursor, 1.0, false, false);
        }
    }
}

bool TerminalWidget::handle_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerScroll) {
        // Mid-drag scroll: cancel any in-progress selection before scrolling
        // to prevent a selection spanning live-view and scrollback simultaneously.
        if (m_selecting) {
            cancel_selection();
        }
        const double delta = event.pointer.scroll_delta_y;
        // Mouse wheel: negative delta_y = scroll up (into history), positive = scroll down
        const int lines = static_cast<int>(std::abs(delta));
        if (lines > 0) {
            if (delta < 0.0) {
                m_emulator->scroll_up(lines);
            } else {
                m_emulator->scroll_down(lines);
            }
        }
        return true;
    }

    if (event.type == txui::EventType::WindowResize) {
        // Resize while scrolled: reset to live view to avoid reflow complexity.
        m_emulator->reset_scroll();
        return false;  // let base class also handle resize
    }

    if (event.type == txui::EventType::KeyDown) {
        // Any keystroke while scrolled jumps back to live view first.
        m_emulator->reset_scroll();

        int mods = VTERM_MOD_NONE;
        if (txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Shift)) mods |= VTERM_MOD_SHIFT;
        if (txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Ctrl))  mods |= VTERM_MOD_CTRL;
        if (txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Alt))   mods |= VTERM_MOD_ALT;
        
        VTermModifier vmod = static_cast<VTermModifier>(mods);
        txui::Key key = event.keyboard.key;
        VTermKey vkey = VTERM_KEY_NONE;
        
        switch (key) {
            case txui::Key::Enter:     vkey = VTERM_KEY_ENTER; break;
            case txui::Key::Tab:       vkey = VTERM_KEY_TAB; break;
            case txui::Key::Backspace: vkey = VTERM_KEY_BACKSPACE; break;
            case txui::Key::Escape:    vkey = VTERM_KEY_ESCAPE; break;
            case txui::Key::Up:        vkey = VTERM_KEY_UP; break;
            case txui::Key::Down:      vkey = VTERM_KEY_DOWN; break;
            case txui::Key::Left:      vkey = VTERM_KEY_LEFT; break;
            case txui::Key::Right:     vkey = VTERM_KEY_RIGHT; break;
            case txui::Key::F1:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(1)); break;
            case txui::Key::F2:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(2)); break;
            case txui::Key::F3:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(3)); break;
            case txui::Key::F4:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(4)); break;
            case txui::Key::F5:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(5)); break;
            case txui::Key::F6:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(6)); break;
            case txui::Key::F7:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(7)); break;
            case txui::Key::F8:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(8)); break;
            case txui::Key::F9:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(9)); break;
            case txui::Key::F10:       vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(10)); break;
            case txui::Key::F11:       vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(11)); break;
            case txui::Key::F12:       vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(12)); break;
            default: break;
        }

        if (vkey != VTERM_KEY_NONE) {
            m_emulator->send_key(vkey, vmod);
            return true;
        }

        uint32_t c = 0;
        if (key >= txui::Key::A && key <= txui::Key::Z) {
            c = (mods & VTERM_MOD_SHIFT) ? 'A' + (static_cast<uint16_t>(key) - static_cast<uint16_t>(txui::Key::A))
                                         : 'a' + (static_cast<uint16_t>(key) - static_cast<uint16_t>(txui::Key::A));
        } else if (key >= txui::Key::N0 && key <= txui::Key::N9) {
            if (mods & VTERM_MOD_SHIFT) {
                const char shift_nums[] = ")!@#$%^&*(";
                c = shift_nums[static_cast<uint16_t>(key) - static_cast<uint16_t>(txui::Key::N0)];
            } else {
                c = '0' + (static_cast<uint16_t>(key) - static_cast<uint16_t>(txui::Key::N0));
            }
        } else if (key == txui::Key::Space) {
            c = ' ';
        } else if (key == txui::Key::Slash) {
            c = (mods & VTERM_MOD_SHIFT) ? '?' : '/';
        } else if (key == txui::Key::Period) {
            c = (mods & VTERM_MOD_SHIFT) ? '>' : '.';
        } else if (key == txui::Key::Minus) {
            c = (mods & VTERM_MOD_SHIFT) ? '_' : '-';
        }

        if (c != 0) {
            // Remove SHIFT if used solely for case conversion, per libvterm docs
            if ((c >= 'A' && c <= 'Z') || (c >= '!' && c <= ')') || c == '?' || c == '>' || c == '_') {
                vmod = static_cast<VTermModifier>(vmod & ~VTERM_MOD_SHIFT);
            }
            m_emulator->send_unichar(c, vmod);
            return true;
        }
    }

    // --- Pointer selection (live-view only) ---
    if (event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            if (event.pointer.x >= frame().left() && event.pointer.x <= frame().right() &&
                event.pointer.y >= frame().top() && event.pointer.y <= frame().bottom()) {
                if (m_emulator->scroll_offset() == 0) {
                    double rel_x = event.pointer.x - frame().x();
                    double rel_y = event.pointer.y - frame().y();
                    int col = std::max(0, std::min(m_cols - 1, static_cast<int>(rel_x / m_cell_width)));
                    int row = std::max(0, std::min(m_rows - 1, static_cast<int>(rel_y / m_cell_height)));
                    m_selecting = true;
                    m_sel_start_row = m_sel_end_row = row;
                    m_sel_start_col = m_sel_end_col = col;
                    mark_needs_paint();
                }
                return true; // Consume if inside frame
            }
        }
    }

    if (event.type == txui::EventType::PointerMove && m_selecting) {
        // Guard: if we somehow got scrolled mid-drag (shouldn't happen — cancel is in PointerScroll)
        if (m_emulator->scroll_offset() != 0) {
            cancel_selection();
        } else {
            double rel_x = event.pointer.x - frame().x();
            double rel_y = event.pointer.y - frame().y();
            m_sel_end_col = std::max(0, std::min(m_cols - 1, static_cast<int>(rel_x / m_cell_width)));
            m_sel_end_row = std::max(0, std::min(m_rows - 1, static_cast<int>(rel_y / m_cell_height)));
            mark_needs_paint();
        }
        return true;
    }

    if (event.type == txui::EventType::PointerButtonRelease && m_selecting) {
        m_selecting = false;
        // Selection stays visible (m_sel_start/end remain valid) for copy operations.
        mark_needs_paint();
        return true;
    }

    return txui::Widget::handle_event(event);
}

// --- Stage 7 helper implementations ---

void TerminalWidget::tick_blink() noexcept {
    // Toggle blink state and dirty only the cursor cell
    m_blink_state = !m_blink_state;
    if (m_cursor_row >= 0 && m_cursor_row < m_rows
            && m_cursor_col >= 0 && m_cursor_col < m_cols) {
        // Push a 1×1 dirty rect for just the cursor cell
        VTermRect cursor_rect{
            m_cursor_row,
            m_cursor_row + 1,
            m_cursor_col,
            m_cursor_col + 1
        };
        m_emulator->dirty_rects_push(cursor_rect);
        mark_needs_paint();
    }
}

void TerminalWidget::on_cursor_move(int new_row, int new_col) noexcept {
    if (new_row == m_cursor_row && new_col == m_cursor_col) return;
    // Mark old cursor cell dirty (to erase old block)
    if (m_cursor_row >= 0 && m_cursor_row < m_rows
            && m_cursor_col >= 0 && m_cursor_col < m_cols) {
        VTermRect old_rect{
            m_cursor_row,
            m_cursor_row + 1,
            m_cursor_col,
            m_cursor_col + 1
        };
        m_emulator->dirty_rects_push(old_rect);
    }
    m_cursor_row = new_row;
    m_cursor_col = new_col;
    // Mark new cursor cell dirty (to draw new block)
    if (m_cursor_row >= 0 && m_cursor_row < m_rows
            && m_cursor_col >= 0 && m_cursor_col < m_cols) {
        VTermRect new_rect{
            m_cursor_row,
            m_cursor_row + 1,
            m_cursor_col,
            m_cursor_col + 1
        };
        m_emulator->dirty_rects_push(new_rect);
    }
    // Reset to visible so cursor is always shown immediately after move
    m_blink_state = true;
    mark_needs_paint();
}

bool TerminalWidget::cell_in_selection(int row, int col) const noexcept {
    if (m_sel_start_row < 0 || m_sel_end_row < 0) return false;
    // Normalize start < end
    int r0 = m_sel_start_row, c0 = m_sel_start_col;
    int r1 = m_sel_end_row,   c1 = m_sel_end_col;
    if (r0 > r1 || (r0 == r1 && c0 > c1)) {
        std::swap(r0, r1);
        std::swap(c0, c1);
    }
    if (row < r0 || row > r1) return false;
    if (r0 == r1) return col >= c0 && col <= c1;
    if (row == r0) return col >= c0;
    if (row == r1) return col <= c1;
    return true;  // fully interior row
}

void TerminalWidget::cancel_selection() noexcept {
    m_selecting = false;
    if (m_sel_start_row >= 0) {
        // Mark the selected area dirty so it repaints without the highlight
        int r0 = std::min(m_sel_start_row, m_sel_end_row);
        int r1 = std::max(m_sel_start_row, m_sel_end_row);
        VTermRect sel_rect{r0, r1 + 1, 0, m_cols};
        m_emulator->dirty_rects_push(sel_rect);
        m_sel_start_row = m_sel_start_col = m_sel_end_row = m_sel_end_col = -1;
        mark_needs_paint();
    }
}

} // namespace tinexus::terminal
