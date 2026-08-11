#include "terminal/TerminalEmulator.hpp"
#include "common/logger.hpp"

namespace tinexus::terminal {

static VTermScreenCallbacks s_screen_callbacks = {};

TerminalEmulator::TerminalEmulator(int rows, int cols) {
    m_vterm = vterm_new(rows, cols);
    vterm_set_utf8(m_vterm, 1);

    m_state = vterm_obtain_state(m_vterm);
    vterm_state_set_bold_highbright(m_state, 1);

    m_screen = vterm_obtain_screen(m_vterm);
    vterm_screen_enable_altscreen(m_screen, 1);

    s_screen_callbacks.damage = damage_cb;
    s_screen_callbacks.moverect = moverect_cb;
    s_screen_callbacks.movecursor = movecursor_cb;
    s_screen_callbacks.settermprop = settermprop_cb;
    s_screen_callbacks.sb_pushline = sb_pushline_cb;
    s_screen_callbacks.sb_popline = sb_popline_cb;
    
    vterm_screen_set_callbacks(m_screen, &s_screen_callbacks, this);
    vterm_screen_reset(m_screen, 1);

    // Initialize default colors
    VTermColor fg, bg;
    vterm_color_rgb(&fg, 200, 200, 200);
    vterm_color_rgb(&bg, 30, 30, 30);
    vterm_state_set_default_colors(m_state, &fg, &bg);

    // Initialize 16-color ANSI palette (standard xterm basic colors)
    const uint8_t palette[16][3] = {
        {0, 0, 0}, {205, 0, 0}, {0, 205, 0}, {205, 205, 0},
        {0, 0, 238}, {205, 0, 205}, {0, 205, 205}, {229, 229, 229},
        {127, 127, 127}, {255, 0, 0}, {0, 255, 0}, {255, 255, 0},
        {92, 92, 255}, {255, 0, 255}, {0, 255, 255}, {255, 255, 255}
    };
    for (int i = 0; i < 16; ++i) {
        VTermColor c;
        vterm_color_rgb(&c, palette[i][0], palette[i][1], palette[i][2]);
        vterm_state_set_palette_color(m_state, i, &c);
    }

    vterm_output_set_callback(m_vterm, output_cb, this);
}

TerminalEmulator::~TerminalEmulator() {
    if (m_vterm) {
        vterm_free(m_vterm);
    }
}

void TerminalEmulator::resize(int rows, int cols) {
    vterm_set_size(m_vterm, rows, cols);
    vterm_screen_flush_damage(m_screen);
}

void TerminalEmulator::write_input(const char* data, size_t len) {
    vterm_input_write(m_vterm, data, len);
    vterm_screen_flush_damage(m_screen);
}

void TerminalEmulator::send_key(VTermKey key, VTermModifier mod) {
    vterm_keyboard_key(m_vterm, key, mod);
    vterm_screen_flush_damage(m_screen);
}

void TerminalEmulator::send_unichar(uint32_t c, VTermModifier mod) {
    vterm_keyboard_unichar(m_vterm, c, mod);
    vterm_screen_flush_damage(m_screen);
}

TerminalCell TerminalEmulator::get_cell(int row, int col) const {
    // If scrolled, top rows come from scrollback history.
    // Visual row r maps to:
    //   scrollback[scroll_offset - 1 - r]  for r < scroll_offset
    //   vterm live row (r - scroll_offset)  for r >= scroll_offset
    if (m_scroll_offset > 0 && row < m_scroll_offset) {
        // scrollback index: row 0 => history[scroll_offset-1] (oldest visible),
        //                   row scroll_offset-1 => history[0] (newest scrolled-off)
        int sb_idx = m_scroll_offset - 1 - row;
        return get_scrollback_cell(sb_idx, col);
    }
    int live_row = row - m_scroll_offset;
    VTermPos pos = {live_row, col};
    VTermScreenCell vcell;
    vterm_screen_get_cell(m_screen, pos, &vcell);

    TerminalCell tcell;
    tcell.codepoint = vcell.chars[0]; // simplistic, ignoring combining chars for MVP
    tcell.width = vcell.width;
    tcell.attrs.bold = vcell.attrs.bold;
    tcell.attrs.italic = vcell.attrs.italic;
    tcell.attrs.underline = vcell.attrs.underline;
    tcell.attrs.blink = vcell.attrs.blink;
    tcell.attrs.reverse = vcell.attrs.reverse;
    
    // vterm colors
    if (VTERM_COLOR_IS_DEFAULT_FG(&vcell.fg)) {
        // default fg
        tcell.attrs.fg_red = tcell.attrs.fg_green = tcell.attrs.fg_blue = 200; 
    } else if (VTERM_COLOR_IS_INDEXED(&vcell.fg)) {
        VTermColor col = vcell.fg;
        vterm_state_get_palette_color(m_state, col.indexed.idx, &col);
        tcell.attrs.fg_red = col.rgb.red;
        tcell.attrs.fg_green = col.rgb.green;
        tcell.attrs.fg_blue = col.rgb.blue;
    } else {
        tcell.attrs.fg_red = vcell.fg.rgb.red;
        tcell.attrs.fg_green = vcell.fg.rgb.green;
        tcell.attrs.fg_blue = vcell.fg.rgb.blue;
    }

    if (VTERM_COLOR_IS_DEFAULT_BG(&vcell.bg)) {
        // default bg
        tcell.attrs.bg_red = tcell.attrs.bg_green = tcell.attrs.bg_blue = 0;
    } else if (VTERM_COLOR_IS_INDEXED(&vcell.bg)) {
        VTermColor col = vcell.bg;
        vterm_state_get_palette_color(m_state, col.indexed.idx, &col);
        tcell.attrs.bg_red = col.rgb.red;
        tcell.attrs.bg_green = col.rgb.green;
        tcell.attrs.bg_blue = col.rgb.blue;
    } else {
        tcell.attrs.bg_red = vcell.bg.rgb.red;
        tcell.attrs.bg_green = vcell.bg.rgb.green;
        tcell.attrs.bg_blue = vcell.bg.rgb.blue;
    }

    return tcell;
}

// get_cell: when scrolled, rows [0 .. scroll_offset-1] come from scrollback history.
// Row 0 = the line just above the current live screen top (most recently scrolled off).
// Row (scroll_offset) = top of live screen.  Rows >= scroll_offset come from vterm.
TerminalCell TerminalEmulator::get_scrollback_cell(int scrollback_row, int col) const {
    // scrollback_row 0 = most-recently-pushed (index 0 in deque)
    if (scrollback_row < 0 || scrollback_row >= static_cast<int>(m_scrollback.size())) {
        return {};  // blank cell
    }
    const auto& line = m_scrollback[scrollback_row];
    if (col < 0 || col >= static_cast<int>(line.size())) {
        return {};
    }
    const VTermScreenCell& vcell = line[col];

    TerminalCell tcell;
    tcell.codepoint = vcell.chars[0];
    tcell.width = vcell.width;
    tcell.attrs.bold = vcell.attrs.bold;
    tcell.attrs.italic = vcell.attrs.italic;
    tcell.attrs.underline = vcell.attrs.underline;
    tcell.attrs.blink = vcell.attrs.blink;
    tcell.attrs.reverse = vcell.attrs.reverse;

    if (VTERM_COLOR_IS_DEFAULT_FG(&vcell.fg)) {
        tcell.attrs.fg_red = tcell.attrs.fg_green = tcell.attrs.fg_blue = 200;
    } else if (VTERM_COLOR_IS_INDEXED(&vcell.fg)) {
        VTermColor col_c = vcell.fg;
        vterm_state_get_palette_color(m_state, col_c.indexed.idx, &col_c);
        tcell.attrs.fg_red   = col_c.rgb.red;
        tcell.attrs.fg_green = col_c.rgb.green;
        tcell.attrs.fg_blue  = col_c.rgb.blue;
    } else {
        tcell.attrs.fg_red   = vcell.fg.rgb.red;
        tcell.attrs.fg_green = vcell.fg.rgb.green;
        tcell.attrs.fg_blue  = vcell.fg.rgb.blue;
    }

    if (VTERM_COLOR_IS_DEFAULT_BG(&vcell.bg)) {
        tcell.attrs.bg_red = tcell.attrs.bg_green = tcell.attrs.bg_blue = 0;
    } else if (VTERM_COLOR_IS_INDEXED(&vcell.bg)) {
        VTermColor col_c = vcell.bg;
        vterm_state_get_palette_color(m_state, col_c.indexed.idx, &col_c);
        tcell.attrs.bg_red   = col_c.rgb.red;
        tcell.attrs.bg_green = col_c.rgb.green;
        tcell.attrs.bg_blue  = col_c.rgb.blue;
    } else {
        tcell.attrs.bg_red   = vcell.bg.rgb.red;
        tcell.attrs.bg_green = vcell.bg.rgb.green;
        tcell.attrs.bg_blue  = vcell.bg.rgb.blue;
    }
    return tcell;
}

void TerminalEmulator::get_cursor_pos(int& row, int& col) const {
    // Currently relying on movecursor_cb to update widget state directly.
    // For now we just return 0,0
    row = 0;
    col = 0;
}

void TerminalEmulator::mark_all_dirty() {
    VTermPos size;
    vterm_get_size(m_vterm, &size.row, &size.col);
    m_dirty_rects.push_back({0, size.row, 0, size.col});
    if (on_damage) on_damage();
}

void TerminalEmulator::scroll_up(int lines) {
    if (lines <= 0) return;
    int max_offset = static_cast<int>(m_scrollback.size());
    m_scroll_offset = std::min(m_scroll_offset + lines, max_offset);
    // Trigger full repaint — we are now compositing history + live rows
    mark_all_dirty();
}

void TerminalEmulator::scroll_down(int lines) {
    if (lines <= 0) return;
    m_scroll_offset = std::max(m_scroll_offset - lines, 0);
    mark_all_dirty();
}

void TerminalEmulator::reset_scroll() noexcept {
    if (m_scroll_offset != 0) {
        m_scroll_offset = 0;
        // on_damage is called by mark_all_dirty; use it to force repaint
        mark_all_dirty();
    }
}

int TerminalEmulator::damage_cb(VTermRect rect, void* user) {
    auto* emulator = static_cast<TerminalEmulator*>(user);
    emulator->m_dirty_rects.push_back(rect);
    if (emulator->on_damage) {
        emulator->on_damage();
    }
    return 1;
}

int TerminalEmulator::moverect_cb(VTermRect dest, VTermRect src, void* user) {
    auto* emulator = static_cast<TerminalEmulator*>(user);
    // Since we don't have a fast blit in txui, we just mark both regions dirty
    emulator->m_dirty_rects.push_back(dest);
    emulator->m_dirty_rects.push_back(src);
    if (emulator->on_damage) {
        emulator->on_damage();
    }
    return 1;
}

int TerminalEmulator::movecursor_cb(VTermPos pos, VTermPos oldpos, int visible, void* user) {
    auto* emulator = static_cast<TerminalEmulator*>(user);
    if (emulator->on_cursor_move) {
        emulator->on_cursor_move(pos.row, pos.col);
    }
    return 1;
}

int TerminalEmulator::settermprop_cb(VTermProp prop, VTermValue* val, void* user) {
    return 1;
}

// sb_pushline_cb: called by libvterm when a line scrolls off the top of the screen.
// cells[0..cols-1] is the row that just disappeared from the live screen.
int TerminalEmulator::sb_pushline_cb(int cols, const VTermScreenCell* cells, void* user) {
    auto* em = static_cast<TerminalEmulator*>(user);
    std::vector<VTermScreenCell> line(cells, cells + cols);
    em->m_scrollback.push_front(std::move(line));
    em->m_cols_at_push = cols;
    // Enforce max history size: drop oldest
    if (static_cast<int>(em->m_scrollback.size()) > MAX_SCROLLBACK) {
        em->m_scrollback.pop_back();
        // If scroll offset was pointing at the line we just evicted, clamp it.
        if (em->m_scroll_offset > static_cast<int>(em->m_scrollback.size())) {
            em->m_scroll_offset = static_cast<int>(em->m_scrollback.size());
        }
    }
    return 1;
}

// sb_popline_cb: called by libvterm when the user (or program) scrolls back into history.
// We must fill cells[0..cols-1] with the most-recent history line.
int TerminalEmulator::sb_popline_cb(int cols, VTermScreenCell* cells, void* user) {
    auto* em = static_cast<TerminalEmulator*>(user);
    if (em->m_scrollback.empty()) {
        return 0;  // no history to pop
    }
    const auto& line = em->m_scrollback.front();
    int n = std::min(cols, static_cast<int>(line.size()));
    for (int i = 0; i < n; ++i) {
        cells[i] = line[i];
    }
    // Zero-fill any remaining columns
    for (int i = n; i < cols; ++i) {
        cells[i] = {};
    }
    em->m_scrollback.pop_front();
    return 1;
}

void TerminalEmulator::output_cb(const char *s, size_t len, void *user) {
    auto* emulator = static_cast<TerminalEmulator*>(user);
    if (emulator->on_output) {
        emulator->on_output(s, len);
    }
}

} // namespace tinexus::terminal
