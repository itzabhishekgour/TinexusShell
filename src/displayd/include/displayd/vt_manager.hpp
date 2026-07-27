#ifndef TINEXUS_DISPLAYD_VT_MANAGER_HPP
#define TINEXUS_DISPLAYD_VT_MANAGER_HPP

#include <cstdint>

namespace tinexus::displayd {

class VtManager {
public:
    VtManager() = default;
    ~VtManager() = default;

    bool allocate_vt(uint32_t vt_num = 7);
    bool switch_vt(uint32_t vt_num);
    [[nodiscard]] uint32_t active_vt() const noexcept { return m_active_vt; }

private:
    uint32_t m_active_vt{7};
};

} // namespace tinexus::displayd

#endif // TINEXUS_DISPLAYD_VT_MANAGER_HPP
