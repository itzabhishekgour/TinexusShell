#ifndef TINEXUS_TERMINAL_EMULATOR_HPP
#define TINEXUS_TERMINAL_EMULATOR_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
#include <functional>
#include <vterm.h>

namespace tinexus::terminal {

struct CellAttrs {
    bool bold{false};
    bool italic{false};
    bool underline{false};
    bool blink{false};
    bool reverse{false};
    uint8_t fg_red{0}, fg_green{0}, fg_blue{0};
    uint8_t bg_red{0}, bg_green{0}, bg_blue{0};
};

struct TerminalCell {
    uint32_t codepoint{0};
    CellAttrs attrs;
    uint8_t width{1};
};

class TerminalEmulator {
public:
    TerminalEmulator(int rows, int cols);
    ~TerminalEmulator();

    void resize(int rows, int cols);
    void write_input(const char* data, size_t len);

    void send_key(VTermKey key, VTermModifier mod);
    void send_unichar(uint32_t c, VTermModifier mod);

    std::function<void(const char*, size_t)> on_output;
    std::function<void()> on_damage;
    std::function<void(int row, int col)> on_cursor_move;  // fires when cursor position changes

    TerminalCell get_cell(int row, int col) const;
    void get_cursor_pos(int& row, int& col) const;
    
    const std::vector<VTermRect>& dirty_rects() const { return m_dirty_rects; }
    void clear_dirty_rects() { m_dirty_rects.clear(); }
    // Push a single rect directly — used by TerminalWidget for cursor/selection dirty tracking
    void dirty_rects_push(const VTermRect& r) { m_dirty_rects.push_back(r); }
    void mark_all_dirty();

    // Scrollback buffer navigation.
    // scroll_up(n): move viewport n lines into history (positive = older output).
    // scroll_down(n): move viewport n lines toward live screen.
    // scroll_offset(): current number of lines scrolled above live screen (0 = live view).
    void scroll_up(int lines);
    void scroll_down(int lines);
    int scroll_offset() const noexcept { return m_scroll_offset; }
    void reset_scroll() noexcept;
    int scrollback_size() const noexcept { return static_cast<int>(m_scrollback.size()); }

private:
    static constexpr int MAX_SCROLLBACK = 1000;

    std::vector<VTermRect> m_dirty_rects;
    VTerm* m_vterm{nullptr};
    VTermScreen* m_screen{nullptr};
    VTermState* m_state{nullptr};

    // Scrollback buffer: each entry is one full screen-row of VTermScreenCells.
    // Front of deque = most recently scrolled-off line (just above live screen).
    // Back of deque = oldest line.
    std::deque<std::vector<VTermScreenCell>> m_scrollback;
    // How many lines above the live screen we are currently viewing.
    // 0 = live view, m_scrollback.size() = oldest visible.
    int m_scroll_offset{0};
    int m_cols_at_push{0};  // column count when lines were pushed (for consistency)

    static int damage_cb(VTermRect rect, void* user);
    static int moverect_cb(VTermRect dest, VTermRect src, void* user);
    static int movecursor_cb(VTermPos pos, VTermPos oldpos, int visible, void* user);
    static int settermprop_cb(VTermProp prop, VTermValue* val, void* user);
    static int sb_pushline_cb(int cols, const VTermScreenCell* cells, void* user);
    static int sb_popline_cb(int cols, VTermScreenCell* cells, void* user);
    static void output_cb(const char *s, size_t len, void *user);

    // Helper to read a cell from the scrollback deque (no libvterm involved)
    TerminalCell get_scrollback_cell(int scrollback_row, int col) const;
};

} // namespace tinexus::terminal

#endif // TINEXUS_TERMINAL_EMULATOR_HPP
