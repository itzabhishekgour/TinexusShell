#include "TerminalBridge.hpp"
#include <common/logger.hpp>
#include <QGuiApplication>
#include <QClipboard>
#include <unistd.h>
#include <cstdlib>

namespace tinexus::terminal {

TerminalBridge::TerminalBridge(QObject* parent)
    : QObject(parent) {
    m_emulator = std::make_unique<TerminalEmulator>(m_rows, m_cols);

    m_emulator->on_output = [this](const char* data, size_t len) {
        m_pty.write_bytes(data, len);
    };

    m_emulator->on_damage = [this]() {
        emit screenDamaged();
    };

    m_emulator->on_cursor_move = [this](int r, int c) {
        m_cursor_row = r;
        m_cursor_col = c;
        emit cursorMoved(r, c);
    };

    m_blink_timer = new QTimer(this);
    connect(m_blink_timer, &QTimer::timeout, this, &TerminalBridge::onBlinkTimerTimeout);
    m_blink_timer->start(500);

    const char* env_shell = std::getenv("SHELL");
    QString shell = (env_shell && env_shell[0] != '\0') ? QString::fromUtf8(env_shell) : QStringLiteral("/bin/sh");
    startShell(shell);
}

TerminalBridge::~TerminalBridge() {
    if (m_blink_timer) {
        m_blink_timer->stop();
    }
    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }
    m_pty.terminate();
}

void TerminalBridge::startShell(const QString& shellPath) {
    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }
    m_pty.terminate();

    std::string path = shellPath.toStdString();
    if (!m_pty.spawn(path, {"-i", "-l"}, static_cast<uint16_t>(m_cols), static_cast<uint16_t>(m_rows))) {
        // Fallback to /bin/sh if preferred shell failed
        m_pty.spawn("/bin/sh", {"-i", "-l"}, static_cast<uint16_t>(m_cols), static_cast<uint16_t>(m_rows));
    }

    if (m_pty.master_fd() >= 0) {
        m_notifier = new QSocketNotifier(m_pty.master_fd(), QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &TerminalBridge::onPtyReadActivated);
    }
}

void TerminalBridge::onPtyReadActivated() {
    if (m_pty.master_fd() < 0) return;

    char buffer[4096];
    ssize_t n = m_pty.read_bytes(buffer, sizeof(buffer));
    if (n > 0) {
        if (m_emulator) {
            m_emulator->write_input(buffer, static_cast<size_t>(n));
            emit screenDamaged();
        }
    } else if (n <= 0) {
        if (m_notifier) {
            m_notifier->setEnabled(false);
        }
        emit childExited();
    }
}

void TerminalBridge::onBlinkTimerTimeout() {
    m_blink_state = !m_blink_state;
    emit blinkStateChanged(m_blink_state);
}

void TerminalBridge::resizeTerminal(int rows, int cols) {
    if (rows <= 0 || cols <= 0) return;
    if (rows == m_rows && cols == m_cols) return;

    m_rows = rows;
    m_cols = cols;

    if (m_emulator) {
        m_emulator->resize(rows, cols);
    }
    m_pty.set_window_size(static_cast<uint16_t>(cols), static_cast<uint16_t>(rows));
    emit terminalResized(rows, cols);
    emit screenDamaged();
}

void TerminalBridge::sendKey(int key, int modifiers, const QString& text) {
    if (!m_emulator) return;

    // Any user typing resets scrollback to live view
    if (m_emulator->scroll_offset() > 0) {
        m_emulator->reset_scroll();
        emit scrollChanged(0);
    }

    VTermModifier vmod = VTERM_MOD_NONE;
    if (modifiers & Qt::ShiftModifier)   vmod = static_cast<VTermModifier>(vmod | VTERM_MOD_SHIFT);
    if (modifiers & Qt::ControlModifier) vmod = static_cast<VTermModifier>(vmod | VTERM_MOD_CTRL);
    if (modifiers & Qt::AltModifier)     vmod = static_cast<VTermModifier>(vmod | VTERM_MOD_ALT);

    VTermKey vkey = VTERM_KEY_NONE;
    switch (key) {
        case Qt::Key_Return:
        case Qt::Key_Enter:     vkey = VTERM_KEY_ENTER; break;
        case Qt::Key_Tab:       vkey = VTERM_KEY_TAB; break;
        case Qt::Key_Backspace: vkey = VTERM_KEY_BACKSPACE; break;
        case Qt::Key_Escape:    vkey = VTERM_KEY_ESCAPE; break;
        case Qt::Key_Up:        vkey = VTERM_KEY_UP; break;
        case Qt::Key_Down:      vkey = VTERM_KEY_DOWN; break;
        case Qt::Key_Left:      vkey = VTERM_KEY_LEFT; break;
        case Qt::Key_Right:     vkey = VTERM_KEY_RIGHT; break;
        case Qt::Key_Delete:    vkey = VTERM_KEY_DEL; break;
        case Qt::Key_Home:      vkey = VTERM_KEY_HOME; break;
        case Qt::Key_End:       vkey = VTERM_KEY_END; break;
        case Qt::Key_PageUp:    vkey = VTERM_KEY_PAGEUP; break;
        case Qt::Key_PageDown:  vkey = VTERM_KEY_PAGEDOWN; break;
        case Qt::Key_Insert:    vkey = VTERM_KEY_INS; break;
        case Qt::Key_F1:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(1)); break;
        case Qt::Key_F2:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(2)); break;
        case Qt::Key_F3:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(3)); break;
        case Qt::Key_F4:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(4)); break;
        case Qt::Key_F5:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(5)); break;
        case Qt::Key_F6:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(6)); break;
        case Qt::Key_F7:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(7)); break;
        case Qt::Key_F8:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(8)); break;
        case Qt::Key_F9:        vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(9)); break;
        case Qt::Key_F10:       vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(10)); break;
        case Qt::Key_F11:       vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(11)); break;
        case Qt::Key_F12:       vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(12)); break;
        default: break;
    }

    if (vkey != VTERM_KEY_NONE) {
        m_emulator->send_key(vkey, vmod);
        return;
    }

    // Control key sequences (Ctrl+A .. Ctrl+Z)
    if ((modifiers & Qt::ControlModifier) && key >= Qt::Key_A && key <= Qt::Key_Z) {
        char ctrl = static_cast<char>(key - Qt::Key_A + 1);
        m_pty.write_bytes(&ctrl, 1);
        return;
    }

    if (!text.isEmpty()) {
        QByteArray utf8 = text.toUtf8();
        m_pty.write_bytes(utf8.constData(), static_cast<size_t>(utf8.size()));
    }
}

void TerminalBridge::sendInput(const QString& text) {
    if (text.isEmpty()) return;
    QByteArray utf8 = text.toUtf8();
    m_pty.write_bytes(utf8.constData(), static_cast<size_t>(utf8.size()));
}

void TerminalBridge::scrollUp(int lines) {
    if (m_emulator) {
        m_emulator->scroll_up(lines);
        emit scrollChanged(m_emulator->scroll_offset());
        emit screenDamaged();
    }
}

void TerminalBridge::scrollDown(int lines) {
    if (m_emulator) {
        m_emulator->scroll_down(lines);
        emit scrollChanged(m_emulator->scroll_offset());
        emit screenDamaged();
    }
}

void TerminalBridge::resetScroll() {
    if (m_emulator && m_emulator->scroll_offset() > 0) {
        m_emulator->reset_scroll();
        emit scrollChanged(0);
        emit screenDamaged();
    }
}

void TerminalBridge::copyToClipboard(const QString& text) {
    if (text.isEmpty()) return;
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text, QClipboard::Clipboard);
        clipboard->setText(text, QClipboard::Selection);
    }
}

void TerminalBridge::pasteFromClipboard() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        QString text = clipboard->text();
        if (!text.isEmpty()) {
            sendInput(text);
        }
    }
}

QString TerminalBridge::getSelectedText(int startRow, int startCol, int endRow, int endCol) const {
    if (!m_emulator) return QString();

    if (startRow > endRow || (startRow == endRow && startCol > endCol)) {
        std::swap(startRow, endRow);
        std::swap(startCol, endCol);
    }

    QString result;
    for (int r = startRow; r <= endRow; ++r) {
        int c_start = (r == startRow) ? startCol : 0;
        int c_end   = (r == endRow)   ? endCol   : (m_cols - 1);

        for (int c = c_start; c <= c_end; ++c) {
            auto cell = m_emulator->get_cell(r, c);
            if (cell.codepoint != 0) {
                result.append(QChar(cell.codepoint));
            } else {
                result.append(QLatin1Char(' '));
            }
        }
        if (r < endRow) {
            result.append(QLatin1Char('\n'));
        }
    }
    return result;
}

} // namespace tinexus::terminal
