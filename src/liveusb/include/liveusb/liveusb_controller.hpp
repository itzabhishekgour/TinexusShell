#ifndef TINEXUS_LIVEUSB_LIVEUSB_CONTROLLER_HPP
#define TINEXUS_LIVEUSB_LIVEUSB_CONTROLLER_HPP

#include "liveusb/device_detector.hpp"

namespace tinexus::liveusb {

enum class LiveUsbState {
    Idle,
    DetectDevice,
    ValidateDevice,
    ValidateISO,
    UserConfirmation,
    Write,
    Sync,
    Verify,
    CreatePersistence,
    FinalVerification,
    Complete,
    Failed
};

class LiveUsbController {
public:
    LiveUsbController() = default;
    ~LiveUsbController() = default;

    bool run_liveusb_creation(const std::string& iso_path, const UsbDevice& device, bool dry_run = false);
    [[nodiscard]] LiveUsbState state() const noexcept { return m_state; }

private:
    LiveUsbState m_state{LiveUsbState::Idle};
    DeviceDetector m_detector;
};

} // namespace tinexus::liveusb

#endif // TINEXUS_LIVEUSB_LIVEUSB_CONTROLLER_HPP
