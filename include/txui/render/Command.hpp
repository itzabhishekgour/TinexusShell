#pragma once

#include <txui/math/Rect.hpp>
#include <txui/graphics/Brush.hpp>
#include <variant>

namespace txui {

enum class CommandType : uint16 {
    BeginFrame,
    EndFrame,

    SetBrush,
    SetPen,

    DrawRect,
    DrawRoundedRect,
    DrawLine,
    DrawImage,
    DrawPath,

    PushClip,
    PopClip,

    PushTransform,
    PopTransform
};

struct BeginFrameCommand {};
struct EndFrameCommand {};

struct SetBrushCommand {
    Brush brush;
};

struct DrawRectCommand {
    Rect rect;
    Brush brush;
};

struct DrawRoundedRectCommand {
    Rect rect;
    Coordinate radius;
    Brush brush;
};

// Phase 4.2.2 Vertical Slice: Solid Rectangle + Rounded Rectangle exposed in Command variant.
using Command = std::variant<
    BeginFrameCommand,
    EndFrameCommand,
    SetBrushCommand,
    DrawRectCommand,
    DrawRoundedRectCommand
>;

[[nodiscard]] inline constexpr CommandType command_type(const Command& cmd) noexcept {
    return std::visit([](auto&& arg) -> CommandType {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, BeginFrameCommand>) return CommandType::BeginFrame;
        else if constexpr (std::is_same_v<T, EndFrameCommand>) return CommandType::EndFrame;
        else if constexpr (std::is_same_v<T, SetBrushCommand>) return CommandType::SetBrush;
        else if constexpr (std::is_same_v<T, DrawRectCommand>) return CommandType::DrawRect;
        else if constexpr (std::is_same_v<T, DrawRoundedRectCommand>) return CommandType::DrawRoundedRect;
    }, cmd);
}

} // namespace txui
