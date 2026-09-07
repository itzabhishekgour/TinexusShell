#include <txui/widgets/TextInput.hpp>
#include <txui/input/Event.hpp>
#include <algorithm>

namespace txui {

TextInput::TextInput(std::string placeholder) noexcept
    : m_placeholder(std::move(placeholder)) {}

void TextInput::set_text(std::string text) noexcept {
    if (m_text != text) {
        m_text = std::move(text);
        if (m_caret_pos > m_text.size()) {
            m_caret_pos = m_text.size();
        }
        clear_selection();
        mark_needs_measure();
        mark_needs_paint();
        if (m_on_text_changed) {
            m_on_text_changed(m_text);
        }
    }
}

void TextInput::set_placeholder(std::string placeholder) noexcept {
    if (m_placeholder != placeholder) {
        m_placeholder = std::move(placeholder);
        mark_needs_paint();
    }
}

void TextInput::set_font_size(double size) noexcept {
    if (m_font_size != size) {
        m_font_size = size;
        mark_needs_measure();
        mark_needs_paint();
    }
}

void TextInput::set_focused(bool focused) noexcept {
    if (m_focused != focused) {
        m_focused = focused;
        if (!m_focused) {
            clear_selection();
        }
        mark_needs_paint();
    }
}

void TextInput::set_caret_position(size_t pos) noexcept {
    size_t new_pos = std::min(pos, m_text.size());
    if (m_caret_pos != new_pos) {
        m_caret_pos = new_pos;
        mark_needs_paint();
    }
}

void TextInput::select_all() noexcept {
    m_sel_start = 0;
    m_sel_end = m_text.size();
    mark_needs_paint();
}

void TextInput::clear_selection() noexcept {
    m_sel_start = 0;
    m_sel_end = 0;
    mark_needs_paint();
}

void TextInput::set_secure_mode(bool secure) noexcept {
    if (m_secure_mode != secure) {
        m_secure_mode = secure;
        mark_needs_measure();
        mark_needs_paint();
    }
}

void TextInput::set_corner_radius(double radius) noexcept {
    if (m_corner_radius != radius) {
        m_corner_radius = radius;
        mark_needs_paint();
    }
}

void TextInput::set_error(bool error) noexcept {
    if (m_error != error) {
        m_error = error;
        mark_needs_paint();
    }
}

void TextInput::set_enabled(bool enabled) noexcept {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!m_enabled) {
            set_focused(false);
            m_hovered = false;
        }
        mark_needs_paint();
    }
}

std::string TextInput::display_text() const noexcept {
    if (!m_secure_mode) {
        return m_text;
    }
    // Security & Visual Requirement:
    // In secure mode, return identical Unicode Bullet glyphs (U+2022, UTF-8: \xE2\x80\xA2).
    // Plaintext password characters are NEVER returned or leaked to painter or display commands.
    std::string masked;
    masked.reserve(m_text.size() * 3);
    for (size_t i = 0; i < m_text.size(); ++i) {
        masked += "\xE2\x80\xA2";
    }
    return masked;
}

std::string TextInput::display_substring(size_t count) const noexcept {
    if (!m_secure_mode) {
        return m_text.substr(0, count);
    }
    // Security & Visual Requirement:
    // When measuring prefixes for caret positioning, selection ranges, or click hit testing,
    // we MUST measure the masked bullets rather than the real password's characters.
    // Measuring plaintext characters would create a side-channel timing and width leak
    // where character widths (e.g. 'i' vs 'w') would reveal password character classes.
    const size_t n = std::min(count, m_text.size());
    std::string masked;
    masked.reserve(n * 3);
    for (size_t i = 0; i < n; ++i) {
        masked += "\xE2\x80\xA2";
    }
    return masked;
}

size_t TextInput::char_index_at_x(double local_x) const noexcept {
    if (local_x <= 0.0 || m_text.empty()) {
        return 0;
    }

    // Security & Visual Requirement:
    // Calculate click-to-caret index using display_substring(i) (masked bullets in secure mode).
    // This ensures clicks map to the visual bullet positions on screen rather than
    // plaintext glyph advances, preventing width-based visual mismatch and information leaks.
    double prev_w = 0.0;
    for (size_t i = 1; i <= m_text.size(); ++i) {
        double cur_w = FontMetrics::measure(display_substring(i), m_font_size).width;
        if (local_x <= cur_w) {
            if ((local_x - prev_w) < (cur_w - local_x)) {
                return i - 1;
            } else {
                return i;
            }
        }
        prev_w = cur_w;
    }
    return m_text.size();
}

Size TextInput::measure_override(const Constraints& constraints) noexcept {
    const double min_w = 180.0;
    const double h = std::max(38.0, m_font_size + 20.0);
    return constraints.constrain(Size(min_w, h));
}

void TextInput::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();

    // 1. Outer aura halo / glow (active during error or focus)
    if (m_error) {
        painter.fill_rounded_rect(Rect(f.x() - 2.0, f.y() - 2.0, f.width() + 4.0, f.height() + 4.0),
                                  m_corner_radius + 2.0, Color(255, 70, 70, 60));
    } else if (m_focused) {
        painter.fill_rounded_rect(Rect(f.x() - 2.0, f.y() - 2.0, f.width() + 4.0, f.height() + 4.0),
                                  m_corner_radius + 2.0, Color(0, 195, 255, 50));
    }

    // 2. Border fill (outer rounded rect conforming exactly to m_corner_radius)
    Color border_col;
    if (m_error) {
        border_col = Color(255, 90, 90, 240);
    } else if (m_focused) {
        border_col = Color(0, 205, 255, 240);
    } else if (!m_enabled) {
        border_col = Color(255, 255, 255, 12);
    } else {
        const uint8 alpha = m_hovered ? 45 : 22;
        border_col = Color(255, 255, 255, alpha);
    }
    painter.fill_rounded_rect(f, m_corner_radius, border_col);

    // 3. Background fill (inner rounded rect inset by stroke thickness)
    const double stroke = (m_focused || m_error) ? 1.5 : 1.0;
    const Color bg_col = m_enabled ? Color(15, 17, 23, 230) : Color(10, 11, 16, 230);
    painter.fill_rounded_rect(Rect(f.x() + stroke, f.y() + stroke, f.width() - stroke * 2.0, f.height() - stroke * 2.0),
                              std::max(0.0, m_corner_radius - stroke), bg_col);

    // Clip contents inside padding
    const double pad_x = 16.0;
    const Rect text_clip(f.x() + pad_x, f.y(), std::max(0.0, f.width() - (pad_x * 2.0)), f.height());
    painter.push_clip(text_clip);

    const double text_x = f.x() + pad_x;
    const double text_y = f.y() + (f.height() - m_font_size) * 0.5;

    // Selection highlight
    if (m_sel_start < m_sel_end && m_sel_end <= m_text.size()) {
        const double x1 = FontMetrics::measure(display_substring(m_sel_start), m_font_size).width;
        const double x2 = FontMetrics::measure(display_substring(m_sel_end), m_font_size).width;
        const double sel_h = m_font_size + 4.0;
        painter.fill_rect(Rect(text_x + x1, text_y - 2.0, x2 - x1, sel_h), Color(0, 122, 255, 110));
    }

    // Text / Placeholder
    // SECURITY AUDIT: Zero plaintext password characters are ever passed to Painter in secure mode.
    if (m_text.empty()) {
        const Color ph_col = m_enabled ? Color(130, 135, 150, 180) : Color(90, 95, 110, 140);
        painter.draw_text(Point(text_x, text_y), m_placeholder, ph_col, m_font_size);
    } else {
        const Color tx_col = m_enabled ? Color(240, 245, 250, 255) : Color(160, 165, 175, 180);
        painter.draw_text(Point(text_x, text_y), display_text(), tx_col, m_font_size);
    }

    // Blinking Caret
    if (m_focused && m_caret_visible && m_enabled) {
        const double caret_offset_x = FontMetrics::measure(display_substring(m_caret_pos), m_font_size).width;
        const double caret_h = m_font_size + 4.0;
        const double caret_y = f.y() + (f.height() - caret_h) * 0.5;
        painter.fill_rect(Rect(text_x + caret_offset_x, caret_y, 1.5, caret_h), Color(0, 210, 255, 255));
    }

    painter.pop_clip();
}

static char key_to_char(Key key, bool shift) noexcept {
    if (key >= Key::A && key <= Key::Z) {
        char base = shift ? 'A' : 'a';
        return static_cast<char>(base + (static_cast<int>(key) - static_cast<int>(Key::A)));
    }
    if (key >= Key::N0 && key <= Key::N9) {
        if (!shift) {
            return static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(Key::N0)));
        }
        // Shift + numbers
        switch (key) {
            case Key::N1: return '!';
            case Key::N2: return '@';
            case Key::N3: return '#';
            case Key::N4: return '$';
            case Key::N5: return '%';
            case Key::N6: return '^';
            case Key::N7: return '&';
            case Key::N8: return '*';
            case Key::N9: return '(';
            case Key::N0: return ')';
            default: break;
        }
    }
    switch (key) {
        case Key::Space: return ' ';
        case Key::Slash: return shift ? '?' : '/';
        case Key::Period: return shift ? '>' : '.';
        case Key::Minus: return shift ? '_' : '-';
        case Key::Equal: return shift ? '+' : '=';
        case Key::Comma: return shift ? '<' : ',';
        case Key::Semicolon: return shift ? ':' : ';';
        case Key::Apostrophe: return shift ? '"' : '\'';
        case Key::Grave: return shift ? '~' : '`';
        case Key::LeftBracket: return shift ? '{' : '[';
        case Key::RightBracket: return shift ? '}' : ']';
        case Key::Backslash: return shift ? '|' : '\\';
        default: return '\0';
    }
}

bool TextInput::handle_event(const Event& event) noexcept {
    if (!m_enabled) {
        return false;
    }
    if (event.type == EventType::PointerMove) {
        Point p(event.pointer.x, event.pointer.y);
        bool inside = frame().contains(p);
        if (m_hovered != inside) {
            m_hovered = inside;
            mark_needs_paint();
        }
        return inside;
    } else if (event.type == EventType::PointerButtonPress) {
        Point p(event.pointer.x, event.pointer.y);
        if (frame().contains(p)) {
            set_focused(true);
            double local_x = p.x - (frame().x() + 12.0);
            m_caret_pos = char_index_at_x(local_x);
            clear_selection();
            mark_needs_paint();
            return true;
        } else {
            if (m_focused) {
                set_focused(false);
            }
        }
    } else if (m_focused && (event.type == EventType::KeyDown || event.type == EventType::KeyRepeat)) {
        const auto& kb = event.keyboard;
        const bool shift = has_modifier(kb.modifiers, KeyModifier::Shift);

        if (kb.key == Key::Enter) {
            if (m_on_submit) {
                m_on_submit(m_text);
            }
            return true;
        } else if (kb.key == Key::Backspace) {
            if (m_sel_start < m_sel_end) {
                m_text.erase(m_sel_start, m_sel_end - m_sel_start);
                m_caret_pos = m_sel_start;
                clear_selection();
                mark_needs_paint();
                if (m_on_text_changed) m_on_text_changed(m_text);
            } else if (m_caret_pos > 0) {
                m_text.erase(m_caret_pos - 1, 1);
                m_caret_pos--;
                mark_needs_paint();
                if (m_on_text_changed) m_on_text_changed(m_text);
            }
            return true;
        } else if (kb.key == Key::Delete) {
            if (m_sel_start < m_sel_end) {
                m_text.erase(m_sel_start, m_sel_end - m_sel_start);
                m_caret_pos = m_sel_start;
                clear_selection();
                mark_needs_paint();
                if (m_on_text_changed) m_on_text_changed(m_text);
            } else if (m_caret_pos < m_text.size()) {
                m_text.erase(m_caret_pos, 1);
                mark_needs_paint();
                if (m_on_text_changed) m_on_text_changed(m_text);
            }
            return true;
        } else if (kb.key == Key::Left) {
            if (m_caret_pos > 0) {
                m_caret_pos--;
                clear_selection();
                mark_needs_paint();
            }
            return true;
        } else if (kb.key == Key::Right) {
            if (m_caret_pos < m_text.size()) {
                m_caret_pos++;
                clear_selection();
                mark_needs_paint();
            }
            return true;
        } else if (kb.key == Key::Home) {
            m_caret_pos = 0;
            clear_selection();
            mark_needs_paint();
            return true;
        } else if (kb.key == Key::End) {
            m_caret_pos = m_text.size();
            clear_selection();
            mark_needs_paint();
            return true;
        } else {
            char ch = key_to_char(kb.key, shift);
            if (ch != '\0') {
                if (m_sel_start < m_sel_end) {
                    m_text.erase(m_sel_start, m_sel_end - m_sel_start);
                    m_caret_pos = m_sel_start;
                    clear_selection();
                }
                m_text.insert(m_caret_pos, 1, ch);
                m_caret_pos++;
                mark_needs_paint();
                if (m_on_text_changed) {
                    m_on_text_changed(m_text);
                }
                return true;
            }
        }
    }
    return false;
}

} // namespace txui
