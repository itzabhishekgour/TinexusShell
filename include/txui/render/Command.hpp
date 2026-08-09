#pragma once

#include <txui/math/Rect.hpp>
#include <txui/graphics/Brush.hpp>
#include <variant>

#include <txui/math/Point.hpp>
#include <string>

namespace txui {

enum class CommandType : uint16 {
    BeginFrame,
    EndFrame,

    SetBrush,
    SetPen,

    DrawRect,
    DrawRoundedRect,
    DrawGradientRect,
    DrawGradientRoundedRect,
    DrawCircle,
    DrawText,
    DrawLine,
    DrawImage,
    DrawPath,

    PushClip,
    PopClip,

    PushTransform,
    PopTransform,
    
    Clear
};

struct BeginFrameCommand {};
struct EndFrameCommand {};
struct ClearCommand {
    Color color;
};

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

// Linear gradient rect: color_start at top, color_end at bottom (or left->right if horizontal)
struct DrawGradientRectCommand {
    Rect   rect;
    Color  color_start;
    Color  color_end;
    bool   horizontal{false};   // false = top->bottom, true = left->right
};

// Rounded rect with linear gradient fill
struct DrawGradientRoundedRectCommand {
    Rect   rect;
    Coordinate radius;
    Color  color_start;
    Color  color_end;
    bool   horizontal{false};
};

// Filled or stroked circle with anti-aliasing
struct DrawCircleCommand {
    Point  center;
    double radius;
    Color  color;
    double stroke_width{0.0};   // 0 = filled, >0 = outline only
};

struct DrawTextCommand {
    Point position;
    std::string text;
    Color color;
    double scale{1.0};
};

// Phase 4.2.2+ : Solid + Rounded + Gradient + Circle + Text
using Command = std::variant<
    BeginFrameCommand,
    EndFrameCommand,
    SetBrushCommand,
    DrawRectCommand,
    DrawRoundedRectCommand,
    DrawGradientRectCommand,
    DrawGradientRoundedRectCommand,
    DrawCircleCommand,
    DrawTextCommand,
    ClearCommand
>;

[[nodiscard]] inline constexpr CommandType command_type(const Command& cmd) noexcept {
    return std::visit([](auto&& arg) -> CommandType {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, BeginFrameCommand>)                   return CommandType::BeginFrame;
        else if constexpr (std::is_same_v<T, EndFrameCommand>)                return CommandType::EndFrame;
        else if constexpr (std::is_same_v<T, SetBrushCommand>)                return CommandType::SetBrush;
        else if constexpr (std::is_same_v<T, DrawRectCommand>)                return CommandType::DrawRect;
        else if constexpr (std::is_same_v<T, DrawRoundedRectCommand>)         return CommandType::DrawRoundedRect;
        else if constexpr (std::is_same_v<T, DrawGradientRectCommand>)        return CommandType::DrawGradientRect;
        else if constexpr (std::is_same_v<T, DrawGradientRoundedRectCommand>) return CommandType::DrawGradientRoundedRect;
        else if constexpr (std::is_same_v<T, DrawCircleCommand>)              return CommandType::DrawCircle;
        else if constexpr (std::is_same_v<T, DrawTextCommand>)                return CommandType::DrawText;
        else if constexpr (std::is_same_v<T, ClearCommand>)                   return CommandType::Clear;
    }, cmd);
}

} // namespace txui
