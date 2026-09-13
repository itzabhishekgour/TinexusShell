#include "installer/bootloader_config.hpp"
#include "common/logger.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>

namespace tinexus::installer {

static bool run_cmd(const std::vector<std::string>& args) {
    if (args.empty()) return false;
    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> c_args;
        for (const auto& a : args) c_args.push_back(const_cast<char*>(a.c_str()));
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        _exit(127);
    }
    if (pid < 0) return false;
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool BootloaderConfig::install_grub_efi(const std::string& target_mount, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would execute grub-install for target EFI mount '{}'", target_mount);
        return true;
    }
    log::info("BootloaderConfig: Installing GRUB EFI on target mount '{}'...", target_mount);

    // Ensure target /boot, kernel, and initrd exist
    std::filesystem::create_directories(target_mount + "/boot");
    if (!std::filesystem::exists(target_mount + "/boot/vmlinuz")) {
        for (const auto& cand : {"/boot/vmlinuz", "/rofs/boot/vmlinuz", "/live/boot/vmlinuz", "/mnt/boot/vmlinuz"}) {
            if (std::filesystem::exists(cand)) {
                std::error_code ec;
                std::filesystem::copy_file(cand, target_mount + "/boot/vmlinuz", 
                                           std::filesystem::copy_options::overwrite_existing, ec);
                if (!ec) {
                    log::info("BootloaderConfig: Copied kernel from '{}' to '{}/boot/vmlinuz'", cand, target_mount);
                    break;
                }
            }
        }
    }

    if (!std::filesystem::exists(target_mount + "/boot/initrd.img")) {
        for (const auto& cand : {"/boot/initrd.img", "/boot/initramfs.img", "/rofs/boot/initrd.img", "/live/boot/initrd.img", "/live/boot/initramfs.img", "/mnt/boot/initrd.img", "/mnt/boot/initramfs.img"}) {
            if (std::filesystem::exists(cand)) {
                std::error_code ec;
                std::filesystem::copy_file(cand, target_mount + "/boot/initrd.img", 
                                           std::filesystem::copy_options::overwrite_existing, ec);
                if (!ec) {
                    log::info("BootloaderConfig: Copied initrd from '{}' to '{}/boot/initrd.img'", cand, target_mount);
                    break;
                }
            }
        }
    }

    // 1. Ensure target EFI directory exists
    std::filesystem::create_directories(target_mount + "/boot/efi");

    // 2. Bind-mount virtual filesystems for chroot
    run_cmd({"mount", "--bind", "/proc", target_mount + "/proc"});
    run_cmd({"mount", "--bind", "/sys", target_mount + "/sys"});
    run_cmd({"mount", "--bind", "/dev", target_mount + "/dev"});
    run_cmd({"mount", "--bind", "/dev/pts", target_mount + "/dev/pts"});

    // 3. Install GRUB EFI in chroot with --removable for universal UEFI boot
    bool ok = run_cmd({
        "chroot", target_mount,
        "grub-install",
        "--target=x86_64-efi",
        "--efi-directory=/boot/efi",
        "--bootloader-id=Tinexus",
        "--removable",
        "--recheck"
    });

    // Write robust direct grub.cfg to both standard boot path and EFI fallback paths
    for (const auto& dir : {target_mount + "/boot/grub", target_mount + "/boot/efi/EFI/BOOT", target_mount + "/boot/efi/EFI/Tinexus"}) {
        std::filesystem::create_directories(dir);
        std::ofstream gcfg(dir + "/grub.cfg");
        if (gcfg.is_open()) {
            gcfg << "set default=0\n"
                 << "set timeout=2\n"
                 << "insmod part_gpt\n"
                 << "insmod ext2\n"
                 << "insmod all_video\n"
                 << "insmod gfxterm\n\n"
                 << "menuentry \"Tinexus OS\" {\n"
                 << "    search --no-floppy --label --set=root TINEXUS_ROOT\n"
                 << "    linux /boot/vmlinuz root=LABEL=TINEXUS_ROOT rw rootwait quiet splash console=tty0 console=ttyS0,115200n8\n"
                 << "    if [ -f /boot/initrd.img ]; then\n"
                 << "        initrd /boot/initrd.img\n"
                 << "    fi\n"
                 << "}\n"
                 << "menuentry \"Tinexus OS (PARTLABEL)\" {\n"
                 << "    search --no-floppy --label --set=root TINEXUS_ROOT\n"
                 << "    linux /boot/vmlinuz root=PARTLABEL=TinexusRoot rw rootwait quiet splash console=tty0 console=ttyS0,115200n8\n"
                 << "    if [ -f /boot/initrd.img ]; then\n"
                 << "        initrd /boot/initrd.img\n"
                 << "    fi\n"
                 << "}\n"
                 << "menuentry \"Tinexus OS (Fallback /dev/sda2)\" {\n"
                 << "    search --no-floppy --label --set=root TINEXUS_ROOT\n"
                 << "    linux /boot/vmlinuz root=/dev/sda2 rw rootwait console=tty0 console=ttyS0,115200n8\n"
                 << "    if [ -f /boot/initrd.img ]; then\n"
                 << "        initrd /boot/initrd.img\n"
                 << "    fi\n"
                 << "}\n";
            gcfg.close();
            log::info("BootloaderConfig: Wrote direct grub.cfg configuration to '{}'", dir);
        }
    }

    // Clean up mounts
    run_cmd({"umount", target_mount + "/dev/pts"});
    run_cmd({"umount", target_mount + "/dev"});
    run_cmd({"umount", target_mount + "/sys"});
    run_cmd({"umount", target_mount + "/proc"});

    if (!ok) {
        log::error("BootloaderConfig: grub-install failed on '{}'", target_mount);
        return false;
    }

    log::info("BootloaderConfig: Installed GRUB EFI bootloader successfully at '{}'", target_mount);
    return true;
}

bool BootloaderConfig::generate_fstab(const std::string& target_mount, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would generate /etc/fstab with UUID entries for target mount '{}'", target_mount);
        return true;
    }
    log::info("BootloaderConfig: Generating /etc/fstab on target '{}'...", target_mount);

    std::filesystem::create_directories(target_mount + "/etc");
    std::string fstab_file = target_mount + "/etc/fstab";
    std::ofstream ofs(fstab_file);
    if (!ofs.is_open()) {
        log::error("BootloaderConfig: Failed to open '{}' for writing", fstab_file);
        return false;
    }

    ofs << "# /etc/fstab: static file system information (generated by Tinexus Installer)\n";
    ofs << "LABEL=TINEXUS_ROOT  /          ext4   defaults,errors=remount-ro  0  1\n";
    ofs << "LABEL=TINEXUS_EFI   /boot/efi  vfat   umask=0077                  0  1\n";
    ofs << "tmpfs               /tmp       tmpfs  defaults,nosuid,nodev       0  0\n";
    ofs.close();

    log::info("BootloaderConfig: Generated /etc/fstab with filesystem labels");
    return true;
}

} // namespace tinexus::installer
