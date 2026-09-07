#include <common/HardwareConfig.hpp>
#include <common/BacklightUtils.hpp>
#include <common/AudioUtils.hpp>
#include <common/DisplayUtils.hpp>
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

using namespace tinexus::hardware;

void test_hardware_config() {
    std::cout << "[TEST] Running HardwareConfig test..." << std::endl;
    std::string test_path = "/tmp/test_hardware_config.toml";
    std::filesystem::remove(test_path);

    HardwareSettings s;
    s.volume = 65;
    s.muted = true;
    s.brightness = 90;

    assert(HardwareConfig::save(s, test_path));
    assert(std::filesystem::exists(test_path));

    auto loaded = HardwareConfig::load(test_path);
    assert(loaded.volume == 65);
    assert(loaded.muted == true);
    assert(loaded.brightness == 90);

    // Test out of bounds clamping
    s.volume = 150;
    s.brightness = 2; // below 5 min
    assert(HardwareConfig::save(s, test_path));
    auto clamped = HardwareConfig::load(test_path);
    assert(clamped.volume == 100);
    assert(clamped.brightness == 5);

    std::filesystem::remove(test_path);
    std::cout << "  -> HardwareConfig test PASSED" << std::endl;
}

void test_backlight_utils_mock() {
    std::cout << "[TEST] Running BacklightUtils with mock sysfs..." << std::endl;
    std::string mock_root = "/tmp/mock_sysfs_backlight";
    std::string device_dir = mock_root + "/intel_backlight";
    std::filesystem::remove_all(mock_root);
    std::filesystem::create_directories(device_dir);

    // Create max_brightness and brightness files
    {
        std::ofstream max_f(device_dir + "/max_brightness");
        max_f << "1000\n";
    }
    {
        std::ofstream cur_f(device_dir + "/brightness");
        cur_f << "500\n";
    }

    // 1. Read brightness percentage
    int pct = BacklightUtils::get_brightness_percent(mock_root);
    assert(pct == 50);

    // 2. Write brightness percentage
    assert(BacklightUtils::set_brightness_percent(75, /*persist=*/false, /*throttle=*/false, mock_root));
    {
        std::ifstream cur_f(device_dir + "/brightness");
        int val = 0;
        cur_f >> val;
        assert(val == 750);
    }
    assert(BacklightUtils::get_brightness_percent(mock_root) == 75);

    // 3. Step brightness
    assert(BacklightUtils::step_brightness(10, /*persist=*/false, mock_root));
    assert(BacklightUtils::get_brightness_percent(mock_root) == 85);

    // 4. Minimum clamp at 5%
    assert(BacklightUtils::set_brightness_percent(1, /*persist=*/false, /*throttle=*/false, mock_root));
    assert(BacklightUtils::get_brightness_percent(mock_root) == 5);

    // Cleanup
    std::filesystem::remove_all(mock_root);
    std::cout << "  -> BacklightUtils mock test PASSED" << std::endl;
}

void test_audio_utils_mock() {
    std::cout << "[TEST] Running AudioUtils with mock proc asound..." << std::endl;
    std::string mock_root = "/tmp/mock_proc_asound";
    std::filesystem::remove_all(mock_root);
    std::filesystem::create_directories(mock_root);

    // Test 1: Single Card
    {
        std::ofstream cards_f(mock_root + "/cards");
        cards_f << " 0 [PCH            ]: HDA-Intel - HDA Intel PCH\n";
        cards_f << "                      HDA Intel PCH at 0x82210000 irq 145\n";
    }

    auto cards = AudioUtils::get_sound_cards(mock_root);
    assert(!cards.empty());
    assert(cards[0].id == 0);
    assert(cards[0].name.find("HDA Intel PCH") != std::string::npos);
    assert(AudioUtils::detect_primary_card_id(mock_root) == 0);

    // Test 2: Dual GPU Laptop (Card 0 is NVidia HDMI, Card 1 is Analog Intel PCH)
    {
        std::ofstream cards_f(mock_root + "/cards");
        cards_f << " 0 [NVidia         ]: HDA-Intel - HDA NVidia\n";
        cards_f << "                      HDA NVidia at 0xa4080000 irq 17\n";
        cards_f << " 1 [PCH            ]: HDA-Intel - HDA Intel PCH\n";
        cards_f << "                      HDA Intel PCH at 0xa4318000 irq 145\n";
    }

    auto dual_cards = AudioUtils::get_sound_cards(mock_root);
    assert(dual_cards.size() == 2);
    // Must select Card 1 (Analog) rather than Card 0 (HDMI)
    int primary_card = AudioUtils::detect_primary_card_id(mock_root);
    std::cout << "  -> Dual card test: selected primary analog card ID: " << primary_card << std::endl;
    assert(primary_card == 1);

    // Primary control detection with mock root
    std::string ctrl = AudioUtils::detect_primary_control(mock_root);
    assert(!ctrl.empty());
    std::cout << "  -> Detected mock/system control: " << ctrl << std::endl;

    // Test volume percent with mock root
    int current = AudioUtils::get_volume_percent(mock_root);
    assert(current >= 0 && current <= 100);

    // Cleanup
    std::filesystem::remove_all(mock_root);
    std::cout << "  -> AudioUtils mock test PASSED" << std::endl;
}

void test_display_utils_mock() {
    std::cout << "[TEST] Running DisplayUtils with mock sysfs drm..." << std::endl;
    std::string mock_drm = "/tmp/mock_sysfs_drm";
    std::filesystem::remove_all(mock_drm);

    std::string edp_dir = mock_drm + "/card1-eDP-1";
    std::filesystem::create_directories(edp_dir);
    {
        std::ofstream sf(edp_dir + "/status");
        sf << "connected\n";
    }
    {
        std::ofstream mf(edp_dir + "/modes");
        mf << "1920x1080\n1600x900\n";
    }

    auto disp = DisplayUtils::get_primary_display(mock_drm);
    assert(disp.connected);
    assert(disp.connector_name == "eDP-1");
    assert(disp.resolution == "1920x1080");
    assert(disp.formatted_line.find("1920 × 1080") != std::string::npos);
    std::cout << "  -> Discovered display: " << disp.connector_name << " (" << disp.formatted_line << ")" << std::endl;

    std::string comp_ver = DisplayUtils::get_compositor_version_string();
    assert(!comp_ver.empty());
    assert(comp_ver.find("tinexus-comp (wlroots") != std::string::npos);
    std::cout << "  -> Compositor version string: " << comp_ver << std::endl;

    std::filesystem::remove_all(mock_drm);
    std::cout << "  -> DisplayUtils mock test PASSED" << std::endl;
}

int main() {
    std::cout << "=== Tinexus Hardware Utils Unit Test Suite ===" << std::endl;
    test_hardware_config();
    test_backlight_utils_mock();
    test_audio_utils_mock();
    test_display_utils_mock();
    std::cout << "=== All hardware tests PASSED successfully! ===" << std::endl;
    return 0;
}
