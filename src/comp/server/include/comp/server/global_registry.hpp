#ifndef TINEXUS_COMP_GLOBAL_REGISTRY_HPP
#define TINEXUS_COMP_GLOBAL_REGISTRY_HPP

#include <string>
#include <vector>
#include <memory>

namespace tinexus::comp {

struct GlobalInterface {
    std::string name;
    uint32_t version{1};
};

class GlobalRegistry {
public:
    GlobalRegistry();
    ~GlobalRegistry() = default;

    void register_global(const std::string& name, uint32_t version);
    [[nodiscard]] std::vector<GlobalInterface> globals() const;
    [[nodiscard]] bool has_global(const std::string& name) const;

private:
    std::vector<GlobalInterface> m_globals;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_GLOBAL_REGISTRY_HPP
