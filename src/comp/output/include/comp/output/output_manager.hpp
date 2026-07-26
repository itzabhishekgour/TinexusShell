#ifndef TINEXUS_COMP_OUTPUT_MANAGER_HPP
#define TINEXUS_COMP_OUTPUT_MANAGER_HPP

#include <string>
#include <vector>
#include <memory>

namespace tinexus::comp {

struct OutputConfig {
    std::string name;
    int width{1920};
    int height{1080};
    int refresh_rate_mhz{60000};
    float scale{1.0f};
    int x{0};
    int y{0};
    bool enabled{true};
};

class OutputManager {
public:
    static OutputManager& instance() noexcept;

    OutputManager() = default;
    ~OutputManager() = default;

    void add_output(const OutputConfig& config);
    void remove_output(const std::string& name);

    [[nodiscard]] std::vector<OutputConfig> get_active_outputs() const;

private:
    std::vector<OutputConfig> m_outputs;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_OUTPUT_MANAGER_HPP
