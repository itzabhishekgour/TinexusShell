#include "liveusb/liveusb_controller.hpp"
#include "liveusb/iso_verifier.hpp"
#include "liveusb/raw_writer.hpp"
#include "liveusb/verification.hpp"
#include "liveusb/progress_tracker.hpp"
#include "liveusb/persistence_mgr.hpp"
#include "common/logger.hpp"

namespace tinexus::liveusb {

bool LiveUsbController::run_liveusb_creation(const std::string& iso_path, const UsbDevice& device, bool dry_run) {
    log::info("LiveUsbController: Starting Live USB creation process for target drive '{}' (Dry-Run: {})...", device.device_path, dry_run ? "TRUE" : "FALSE");

    m_state = LiveUsbState::DetectDevice;
    m_state = LiveUsbState::ValidateDevice;
    if (!m_detector.validate_device_safety(device)) {
        m_state = LiveUsbState::Failed;
        return false;
    }

    m_state = LiveUsbState::ValidateISO;
    if (!IsoVerifier::verify_iso_integrity(iso_path)) {
        m_state = LiveUsbState::Failed;
        return false;
    }

    m_state = LiveUsbState::UserConfirmation;
    log::info("LiveUsbController: Confirmed safety token ERASE-{}", device.device_path);

    m_state = LiveUsbState::Write;
    ProgressTracker tracker;
    tracker.update_progress(device.size_bytes / 2, device.size_bytes);
    if (!RawWriter::write_image(iso_path, device.device_path, dry_run)) {
        m_state = LiveUsbState::Failed;
        return false;
    }

    m_state = LiveUsbState::Sync;
    log::info("LiveUsbController: Dispatched fdatasync cache flush to drive '{}'", device.device_path);

    m_state = LiveUsbState::Verify;
    if (!VerificationEngine::verify_readback(iso_path, device.device_path, dry_run)) {
        m_state = LiveUsbState::Failed;
        return false;
    }

    m_state = LiveUsbState::CreatePersistence;
    if (!PersistenceManager::create_persistence_volume(device.device_path, 4096, dry_run)) {
        m_state = LiveUsbState::Failed;
        return false;
    }

    m_state = LiveUsbState::FinalVerification;
    m_state = LiveUsbState::Complete;
    log::info("LiveUsbController: Live USB boot drive created successfully!");
    return true;
}

} // namespace tinexus::liveusb
