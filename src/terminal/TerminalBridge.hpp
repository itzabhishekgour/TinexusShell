#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <QString>
#include <QClipboard>
#include <QGuiApplication>
#include "terminal/pty_process.hpp"
#include "terminal/TerminalEmulator.hpp"
#include <memory>
#include <string>

namespace tinexus::terminal {

class TerminalBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(int rows READ rows NOTIFY terminalResized)
    Q_PROPERTY(int cols READ cols NOTIFY terminalResized)
    Q_PROPERTY(bool blinkState READ blinkState NOTIFY blinkStateChanged)
    Q_PROPERTY(int cursorRow READ cursorRow NOTIFY cursorMoved)
    Q_PROPERTY(int cursorCol READ cursorCol NOTIFY cursorMoved)
    Q_PROPERTY(int scrollOffset READ scrollOffset NOTIFY scrollChanged)

public:
    explicit TerminalBridge(QObject* parent = nullptr);
    ~TerminalBridge() override;

    int rows() const noexcept { return m_rows; }
    int cols() const noexcept { return m_cols; }
    bool blinkState() const noexcept { return m_blink_state; }
    int cursorRow() const noexcept { return m_cursor_row; }
    int cursorCol() const noexcept { return m_cursor_col; }
    int scrollOffset() const noexcept { return m_emulator ? m_emulator->scroll_offset() : 0; }

    TerminalEmulator* emulator() const noexcept { return m_emulator.get(); }
    PtyProcess& pty() noexcept { return m_pty; }

    Q_INVOKABLE void startShell(const QString& shellPath = QStringLiteral("/bin/sh"));
    Q_INVOKABLE void resizeTerminal(int rows, int cols);
    Q_INVOKABLE void sendKey(int key, int modifiers, const QString& text);
    Q_INVOKABLE void sendInput(const QString& text);
    Q_INVOKABLE void scrollUp(int lines);
    Q_INVOKABLE void scrollDown(int lines);
    Q_INVOKABLE void resetScroll();
    Q_INVOKABLE void copyToClipboard(const QString& text);
    Q_INVOKABLE void pasteFromClipboard();
    Q_INVOKABLE QString getSelectedText(int startRow, int startCol, int endRow, int endCol) const;

signals:
    void screenDamaged();
    void cursorMoved(int row, int col);
    void terminalResized(int rows, int cols);
    void blinkStateChanged(bool state);
    void scrollChanged(int offset);
    void childExited();

private slots:
    void onPtyReadActivated();
    void onBlinkTimerTimeout();

private:
    std::unique_ptr<TerminalEmulator> m_emulator;
    PtyProcess                        m_pty;
    QSocketNotifier*                  m_notifier{nullptr};
    QTimer*                           m_blink_timer{nullptr};

    int  m_rows{24};
    int  m_cols{80};
    int  m_cursor_row{0};
    int  m_cursor_col{0};
    bool m_blink_state{true};
};

} // namespace tinexus::terminal
