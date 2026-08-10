#pragma once

#include "comp/animation/animation.hpp"
#include "comp/window/window_node.hpp"
#include <memory>

namespace tinexus::comp {

class MinimizeAnimation : public BaseAnimation {
public:
    MinimizeAnimation(std::shared_ptr<WindowNode> win, float start_opacity, float start_scale, int32_t start_x, int32_t start_y);

    void update(double p) override;
    void on_complete() override;

private:
    std::shared_ptr<WindowNode> m_win;
    float m_start_opacity;
    float m_start_scale;
    int32_t m_start_x;
    int32_t m_start_y;
};

} // namespace tinexus::comp
