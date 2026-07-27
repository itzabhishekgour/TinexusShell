#ifndef TINEXUS_DISPLAYD_SEAT_MANAGER_HPP
#define TINEXUS_DISPLAYD_SEAT_MANAGER_HPP

#include <string>

namespace tinexus::displayd {

class SeatManager {
public:
    SeatManager() = default;
    ~SeatManager() = default;

    bool acquire_seat(const std::string& seat_id = "seat0");
    bool release_seat();
    [[nodiscard]] bool is_seat_acquired() const noexcept { return m_acquired; }

private:
    std::string m_seat_id{"seat0"};
    bool m_acquired{false};
};

} // namespace tinexus::displayd

#endif // TINEXUS_DISPLAYD_SEAT_MANAGER_HPP
