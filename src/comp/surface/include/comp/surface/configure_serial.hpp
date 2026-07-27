#ifndef TINEXUS_COMP_CONFIGURE_SERIAL_HPP
#define TINEXUS_COMP_CONFIGURE_SERIAL_HPP

#include <cstdint>
#include <unordered_set>

namespace tinexus::comp {

class ConfigureSerialManager {
public:
    ConfigureSerialManager() = default;
    ~ConfigureSerialManager() = default;

    uint32_t generate();
    bool validate(uint32_t serial);
    void expire(uint32_t serial);
    [[nodiscard]] size_t pending_count() const noexcept;

private:
    uint32_t m_next_serial{100};
    std::unordered_set<uint32_t> m_pending_serials;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_CONFIGURE_SERIAL_HPP
