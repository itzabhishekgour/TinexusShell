#ifndef TINEXUS_COMP_ANIMATION_HPP
#define TINEXUS_COMP_ANIMATION_HPP

#include <chrono>

namespace tinexus::comp {

enum class AnimationCurve {
    Linear,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic
};

class IAnimation {
public:
    virtual ~IAnimation() = default;

    virtual void start() = 0;
    virtual void update(double progress) = 0;
    virtual void on_complete() = 0;
    [[nodiscard]] virtual bool is_running() const noexcept = 0;
};

class BaseAnimation : public IAnimation {
public:
    explicit BaseAnimation(std::chrono::milliseconds duration, AnimationCurve curve = AnimationCurve::EaseOutCubic);
    ~BaseAnimation() override = default;

    void start() override;
    void update(double progress) override = 0;
    void on_complete() override = 0;
    [[nodiscard]] bool is_running() const noexcept override;

    void tick(std::chrono::steady_clock::time_point now);

protected:
    [[nodiscard]] double interpolate(double t) const noexcept;

private:
    std::chrono::milliseconds m_duration;
    AnimationCurve m_curve;
    std::chrono::steady_clock::time_point m_start_time{};
    bool m_running{false};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_ANIMATION_HPP
