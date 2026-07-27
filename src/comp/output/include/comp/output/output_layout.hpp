#ifndef TINEXUS_COMP_OUTPUT_LAYOUT_HPP
#define TINEXUS_COMP_OUTPUT_LAYOUT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

namespace tinexus::comp {

struct OutputSpec {
    std::string name;
    int32_t x{0};
    int32_t y{0};
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t refresh_hz{60};
    float scale{1.0f};
    int32_t transform{0};
    bool enabled{true};
    bool is_primary{false};

    [[nodiscard]] bool contains_point(int32_t gx, int32_t gy) const noexcept {
        if (!enabled) return false;
        return (gx >= x && gx < x + static_cast<int32_t>(width) &&
                gy >= y && gy < y + static_cast<int32_t>(height));
    }
};

struct Point2D {
    int32_t x{0};
    int32_t y{0};
};

class IOutputObserver {
public:
    virtual ~IOutputObserver() = default;

    virtual void on_output_added(const OutputSpec& spec) = 0;
    virtual void on_output_removed(const std::string& name) = 0;
    virtual void on_mode_changed(const std::string& name, uint32_t width, uint32_t height, uint32_t hz) = 0;
    virtual void on_output_enabled(const std::string& name) = 0;
    virtual void on_output_disabled(const std::string& name) = 0;
    virtual void on_scale_changed(const std::string& name, float scale) = 0;
    virtual void on_transform_changed(const std::string& name, int32_t transform) = 0;
};

class OutputLayout {
public:
    static OutputLayout& instance() noexcept;

    OutputLayout() = default;
    ~OutputLayout() = default;

    void add_output(const OutputSpec& spec);
    bool remove_output(const std::string& name);
    void set_primary(const std::string& name);

    void register_observer(IOutputObserver* observer);
    void unregister_observer(IOutputObserver* observer);

    [[nodiscard]] const OutputSpec* get_output(const std::string& name) const noexcept;
    [[nodiscard]] const OutputSpec* get_primary() const noexcept;
    [[nodiscard]] const OutputSpec* output_at_point(int32_t global_x, int32_t global_y) const noexcept;

    [[nodiscard]] Point2D global_to_output_coords(const std::string& name, int32_t global_x, int32_t global_y) const noexcept;
    [[nodiscard]] Point2D output_to_global_coords(const std::string& name, int32_t local_x, int32_t local_y) const noexcept;

    [[nodiscard]] const std::vector<OutputSpec>& outputs() const noexcept { return m_outputs; }

private:
    std::vector<OutputSpec> m_outputs;
    std::vector<IOutputObserver*> m_observers;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_OUTPUT_LAYOUT_HPP
