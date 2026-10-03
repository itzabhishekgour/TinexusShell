#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

#include <QtCore/QCoreApplication>
#include "common/AudioUtils.hpp"
#include "common/logger.hpp"
#include "settings-ui/qt/SettingsBridge.hpp"
#include "shell/ShellBridge.hpp"

namespace fs = std::filesystem;

void test_audio_hardware_independence() {
    std::cout << "[+] Step 1: Testing Hardware Independence & Dynamic Device Discovery...\n";
    
    // Create isolated virtual proc_asound test directory
    fs::path temp_asound = "/tmp/test_proc_asound";
    fs::create_directories(temp_asound);
    
    // Scenario A: Multi-vendor audio devices (Intel + USB Audio + DisplayPort)
    {
        std::ofstream ofs(temp_asound / "cards");
        ofs << " 0 [PCH            ]: HDA-Intel - HDA Intel PCH\n"
            << "                      HDA Intel PCH at 0x51410000 irq 145\n"
            << " 1 [Headset        ]: USB-Audio - Corsair Gaming Headset\n"
            << "                      Corsair Corsair Gaming Headset at usb-0000:00:14.0-1, full speed\n"
            << " 2 [NVidia         ]: HDA-Intel - HDA NVidia\n"
            << "                      HDA NVidia at 0x51080000 irq 17\n";
    }

    auto devices = tinexus::hardware::AudioUtils::get_output_devices(temp_asound.string());
    assert(devices.size() == 3);
    assert(devices[0].find("HDA Intel PCH") != std::string::npos);
    assert(devices[1].find("Corsair Gaming Headset") != std::string::npos);
    assert(devices[2].find("HDA NVidia") != std::string::npos);
    
    // Ensure NO hardcoded ASUS ALC294 anywhere in returned output
    for (const auto& d : devices) {
        assert(d.find("ALC294") == std::string::npos);
    }
    std::cout << "  [PASS] Dynamic multi-vendor devices discovered: " << devices.size() << " devices\n";

    // Scenario B: Zero audio devices present (headless VM / container fallback)
    {
        std::ofstream ofs(temp_asound / "cards");
        // empty file
    }
    auto empty_devs = tinexus::hardware::AudioUtils::get_output_devices(temp_asound.string());
    assert(empty_devs.size() == 1);
    assert(empty_devs[0] == "No Audio Output Devices Available");
    std::cout << "  [PASS] Graceful fallback on zero hardware: '" << empty_devs[0] << "'\n";

    fs::remove_all(temp_asound);
}

void test_chime_sound_resolution() {
    std::cout << "[+] Step 1b: Testing Sound Chime Resolution & Path Discovery...\n";
    auto chime = tinexus::hardware::AudioUtils::resolve_chime_path();
    std::cout << "  Resolved chime path: '" << chime << "'\n";
    assert(!chime.empty());
    assert(fs::exists(chime));
    std::cout << "  [PASS] Test sound chime exists and is accessible for playback\n";
}

void test_topbar_settings_bidirectional_sync(int argc, char* argv[]) {
    std::cout << "[+] Step 2: Testing Topbar <-> Settings Bidirectional Synchronization...\n";

    QCoreApplication app(argc, argv);

    tinexus::shell::ShellBridge topbarBridge;
    tinexus::settings_ui::SettingsBridge settingsBridge;

    // A. Change volume from Topbar -> Verify Settings updates
    std::cout << "  - Setting Topbar volume to 84%...\n";
    topbarBridge.setVolume(84);
    assert(topbarBridge.volume() == 84);
    assert(tinexus::hardware::AudioUtils::get_volume_percent() == 84);

    // Sync Settings
    settingsBridge.syncAudioState();
    std::cout << "    Topbar volume: " << topbarBridge.volume() 
              << "% | Settings volume: " << settingsBridge.volume() << "%\n";
    assert(settingsBridge.volume() == 84);
    std::cout << "  [PASS] Volume changed from Topbar reflected immediately in Settings\n";

    // B. Change volume from Settings -> Verify Topbar updates
    std::cout << "  - Setting Settings volume to 36%...\n";
    settingsBridge.setVolume(36);
    assert(settingsBridge.volume() == 36);
    assert(tinexus::hardware::AudioUtils::get_volume_percent() == 36);

    // Sync Topbar
    topbarBridge.syncAudioState();
    std::cout << "    Settings volume: " << settingsBridge.volume() 
              << "% | Topbar volume: " << topbarBridge.volume() << "%\n";
    assert(topbarBridge.volume() == 36);
    std::cout << "  [PASS] Volume changed from Settings reflected immediately in Topbar\n";

    // C. Mute from Topbar -> Verify Settings updates
    std::cout << "  - Muting audio from Topbar...\n";
    topbarBridge.setSoundMuted(true);
    assert(topbarBridge.soundMuted() == true);
    assert(tinexus::hardware::AudioUtils::is_muted() == true);

    settingsBridge.syncAudioState();
    assert(settingsBridge.muted() == true);
    std::cout << "  [PASS] Mute from Topbar reflected immediately in Settings\n";

    // D. Unmute from Settings -> Verify Topbar updates
    std::cout << "  - Unmuting audio from Settings...\n";
    settingsBridge.setMuted(false);
    assert(settingsBridge.muted() == false);
    assert(tinexus::hardware::AudioUtils::is_muted() == false);

    topbarBridge.syncAudioState();
    assert(topbarBridge.soundMuted() == false);
    std::cout << "  [PASS] Unmute from Settings reflected immediately in Topbar\n";

    // E. Dynamic Output Device Selection in Settings
    std::cout << "  - Testing output device selection in Settings...\n";
    settingsBridge.refreshAudioDevices();
    QStringList devList = settingsBridge.outputDevices();
    assert(!devList.isEmpty());
    std::cout << "    Detected Output Devices (" << devList.size() << "):\n";
    for (int i = 0; i < devList.size(); ++i) {
        std::cout << "      [" << i << "] " << devList[i].toStdString() << "\n";
    }
    
    settingsBridge.setOutputDevice(0);
    assert(settingsBridge.currentOutputIndex() == 0);
    std::cout << "  [PASS] Output device selection functional and verified\n";

    // F. External audio change (multimedia keys / external daemon) resynchronization
    std::cout << "  - Testing external audio resynchronization...\n";
    tinexus::hardware::AudioUtils::set_volume_percent(62, /*persist=*/true, /*throttle=*/false);
    settingsBridge.syncAudioState();
    topbarBridge.syncAudioState();
    assert(settingsBridge.volume() == 62);
    assert(topbarBridge.volume() == 62);
    std::cout << "  [PASS] External audio volume resynchronizes across both UI bridges\n";
}

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("test_audio_e2e");
    std::cout << "=================================================================\n";
    std::cout << "   TINEXUS AUDIO UI & BACKEND END-TO-END VERIFICATION SUITE       \n";
    std::cout << "=================================================================\n";

    test_audio_hardware_independence();
    test_chime_sound_resolution();
    test_topbar_settings_bidirectional_sync(argc, argv);

    std::cout << "\n=================================================================\n";
    std::cout << "   ALL AUDIO UI & HARDWARE-AGNOSTIC TESTS PASSED CLEANLY!         \n";
    std::cout << "=================================================================\n";
    return 0;
}
