#pragma once

#include <txui/render/Command.hpp>
#include <vector>
#include <span>

namespace txui {

class CommandBuffer {
private:
    std::vector<Command> m_commands;

public:
    CommandBuffer() = default;

    void push(const Command& cmd) {
        m_commands.push_back(cmd);
    }

    void push(Command&& cmd) {
        m_commands.push_back(std::move(cmd));
    }

    void clear() noexcept {
        m_commands.clear();
    }

    [[nodiscard]] std::span<const Command> commands() const noexcept {
        return m_commands;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return m_commands.size();
    }

    [[nodiscard]] bool is_empty() const noexcept {
        return m_commands.empty();
    }
};

} // namespace txui
