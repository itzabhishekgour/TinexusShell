#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "iso/rootfs_stager.hpp"
#include "iso/initramfs_builder.hpp"
#include "iso/squashfs_builder.hpp"
#include "iso/boot_config.hpp"
#include "iso/iso_builder.hpp"

void test_rootfs_stager() {
    tinexus::iso::RootfsStager stager;
    assert(stager.stage_rootfs("/tmp/rootfs", {"manifest.json"}, true));
    std::cout << "[PASS] test_rootfs_stager\n";
}

void test_initramfs_and_squashfs_builders() {
    assert(tinexus::iso::InitramfsBuilder::generate_initramfs("/tmp/initramfs.img", true));
    assert(tinexus::iso::SquashfsBuilder::compress_rootfs("/tmp/rootfs", "/tmp/rootfs.squashfs", true));
    std::cout << "[PASS] test_initramfs_and_squashfs_builders\n";
}

void test_boot_config() {
    assert(tinexus::iso::BootConfig::generate_grub_config("/tmp/grub.cfg", true));
    assert(tinexus::iso::BootConfig::generate_efi_image("/tmp/efi.img", true));
    std::cout << "[PASS] test_boot_config\n";
}

void test_iso_builder_pipeline() {
    tinexus::iso::IsoBuilder builder;
    assert(builder.build_iso("Tinexus-0.1.0-alpha.iso", true));
    std::cout << "[PASS] test_iso_builder_pipeline\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_iso");
    tinexus::log::info("Running Integration Test Suite for Tinexus ISO Builder...");

    test_rootfs_stager();
    test_initramfs_and_squashfs_builders();
    test_boot_config();
    test_iso_builder_pipeline();

    tinexus::log::info("All Tinexus ISO Builder integration tests passed 100%!");
    return 0;
}
