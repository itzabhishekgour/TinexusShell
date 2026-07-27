#ifndef TINEXUS_DISPLAYD_VT_MANAGER_HPP
#define TINEXUS_DISPLAYD_VT_MANAGER_HPP

#include <cstdint>
#include <string>

namespace tinexus::displayd {

class VTManager {
public:
    static VTManager& instance() noexcept;

    VTManager() = default;
    ~VTManager() = default;

    [[nodiscard]] int32_t find_free_vt() noexcept;
    [[nodiscard]] bool allocate_vt(int32_t vt_num) noexcept;
    [[nodiscard]] bool activate_vt(int32_t vt_num) noexcept;
    [[nodiscard]] bool release_vt(int32_t vt_num) noexcept;
    [[nodiscard]] int32_t current_vt() const noexcept { return m_active_vt; }

private:
    int32_t m_active_vt{7};
};

} // namespace tinexus::displayd

#endif // TINEXUS_DISPLAYD_VT_MANAGER_HPP
