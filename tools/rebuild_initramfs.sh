#!/bin/bash
# ==============================================================================
# Tinexus Platform — Rebuild Initramfs with /sbin/init (systemd) Target
# ==============================================================================
set -euo pipefail

PROJECT_DIR="/workspace"
BUILD_DIR="$PROJECT_DIR/build"
ORIG_ISO="$BUILD_DIR/Tinexus-x86_64.iso"
if mountpoint -q /mnt/rootfs/tmp 2>/dev/null; then
    WORK_DIR="/mnt/rootfs/tmp/initramfs_rebuild"
    CHROOT_WORK_DIR="/tmp/initramfs_rebuild"
else
    WORK_DIR="/mnt/workdisk/initramfs_rebuild"
    CHROOT_WORK_DIR=""
fi

[ -f "$BUILD_DIR/workdisk.img" ] || { echo "workdisk.img not found!"; exit 1; }
mkdir -p /mnt/workdisk
mountpoint -q /mnt/workdisk || mount -o loop "$BUILD_DIR/workdisk.img" /mnt/workdisk

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/extracted" "$WORK_DIR/isomnt_init"

echo "[INFO] Mounting original ISO to obtain base initramfs..."
mount -o loop,ro "$ORIG_ISO" "$WORK_DIR/isomnt_init"
cp -f "$WORK_DIR/isomnt_init/boot/initramfs.img" "$WORK_DIR/initramfs_orig.img"
umount "$WORK_DIR/isomnt_init"

echo "[INFO] Unpacking initramfs..."
cd "$WORK_DIR/extracted"
gzip -dc "$WORK_DIR/initramfs_orig.img" | cpio -idmv 2>/dev/null

echo "[INFO] Verifying original /init script:"
grep -n "switch_root" init

echo "[INFO] Patching /init script using sed to target /sbin/init (systemd)..."
sed -i 's|log_step "STEP 11: ABOUT TO EXECUTE switch_root -c /dev/console /newroot /usr/bin/tinexus-serviced..."|log_step "STEP 11: ABOUT TO EXECUTE switch_root into systemd init (/sbin/init)..."|g' init
sed -i 's|exec switch_root -c /dev/console /newroot /usr/bin/tinexus-serviced|exec switch_root -c /dev/console /newroot /sbin/init|g' init
# Eliminate triplicated logging lines from log_step
sed -i '/echo ">>> \[TINEXUS-STEP\] \$1" > \/dev\/console/d' init
sed -i '/echo ">>> \[TINEXUS-STEP\] \$1" > \/dev\/tty0/d' init

chmod 0755 init
cp -f "$PROJECT_DIR/assets/logo/tinexus-logo.png" "$WORK_DIR/extracted/tinexus-logo.png"

echo "[INFO] Verifying patched /init script:"
grep -n -C 4 "switch_root" init

# Assert tinexus-serviced is NO LONGER target of switch_root
if grep "switch_root" init | grep "tinexus-serviced" >/dev/null 2>&1; then
    echo "[FATAL] tinexus-serviced is still present in switch_root call!"
    exit 1
fi

echo "[INFO] Staging full GPU firmware (Intel i915/xe, AMD amdgpu/radeon) into initramfs..."
FW_SRC_ROOT=""
if [ -d "/mnt/rootfs/lib/firmware" ]; then
    FW_SRC_ROOT="/mnt/rootfs/lib/firmware"
elif [ -d "/mnt/squashfs/lib/firmware" ]; then
    FW_SRC_ROOT="/mnt/squashfs/lib/firmware"
elif [ -d "/lib/firmware" ]; then
    FW_SRC_ROOT="/lib/firmware"
fi

if [ -n "$FW_SRC_ROOT" ]; then
    for fw_dir in i915 xe amdgpu radeon; do
        if [ -d "$FW_SRC_ROOT/$fw_dir" ]; then
            mkdir -p "$WORK_DIR/extracted/lib/firmware/$fw_dir"
            cp -rL "$FW_SRC_ROOT/$fw_dir/." "$WORK_DIR/extracted/lib/firmware/$fw_dir/" 2>/dev/null || cp -a "$FW_SRC_ROOT/$fw_dir/." "$WORK_DIR/extracted/lib/firmware/$fw_dir/"
        fi
    done

    # Decompress .zst firmware blobs to raw .bin inside initramfs so early kernel
    # request_firmware() succeeds universally (initramfs itself is gzip-9 packed below).
    if command -v zstd >/dev/null 2>&1; then
        find "$WORK_DIR/extracted/lib/firmware" -type f -name "*.zst" -exec zstd -d -f --rm {} + 2>/dev/null || true
    elif [ -n "$CHROOT_WORK_DIR" ] && [ -x "/mnt/rootfs/usr/bin/zstd" ]; then
        chroot /mnt/rootfs /usr/bin/find "$CHROOT_WORK_DIR/extracted/lib/firmware" -type f -name "*.zst" -exec /usr/bin/zstd -d -f --rm {} + 2>/dev/null || true
    elif [ -x "/mnt/rootfs/usr/bin/python3" ]; then
        chroot /mnt/rootfs /usr/bin/python3 -c "
import ctypes, os
z = ctypes.CDLL('libzstd.so.1')
dctx = z.ZSTD_createDCtx()
z.ZSTD_decompressDCtx.restype = ctypes.c_size_t
out = (ctypes.c_char * (32 * 1024 * 1024))()
root = '$CHROOT_WORK_DIR/extracted/lib/firmware' if '$CHROOT_WORK_DIR' else ''
if root and os.path.isdir(root):
    for dp, _, fns in os.walk(root):
        for fn in fns:
            if fn.endswith('.zst'):
                p = os.path.join(dp, fn)
                raw = open(p, 'rb').read()
                n = z.ZSTD_decompressDCtx(dctx, out, len(out), raw, len(raw))
                if n > 0 and n < len(out):
                    open(p[:-4], 'wb').write(out[:n])
                    os.remove(p)
" 2>/dev/null || true
    fi

    # Stage matching GPU kernel modules (i915, xe, amdgpu, radeon, helper deps) if present in rootfs
    MOD_SRC_BASE=""
    if [ -d "/mnt/rootfs/lib/modules" ]; then
        MOD_SRC_BASE="/mnt/rootfs/lib/modules"
    elif [ -d "/mnt/squashfs/lib/modules" ]; then
        MOD_SRC_BASE="/mnt/squashfs/lib/modules"
    fi
    if [ -n "$MOD_SRC_BASE" ]; then
        KVER_DIR="$(ls -1 "$MOD_SRC_BASE" 2>/dev/null | head -n 1)"
        if [ -n "$KVER_DIR" ] && [ -d "$MOD_SRC_BASE/$KVER_DIR" ]; then
            mkdir -p "$WORK_DIR/extracted/lib/modules/$KVER_DIR" "$WORK_DIR/extracted/lib/modules"
            for mod_name in i915.ko xe.ko amdgpu.ko radeon.ko drm_suballoc_helper.ko drm_exec.ko gpu-sched.ko; do
                found_mod="$(find "$MOD_SRC_BASE/$KVER_DIR" -type f \( -name "$mod_name" -o -name "${mod_name}.zst" \) | head -n 1 || true)"
                if [ -n "$found_mod" ]; then
                    if [[ "$found_mod" == *.zst ]]; then
                        if command -v zstd >/dev/null 2>&1; then
                            zstd -d -f -c "$found_mod" > "$WORK_DIR/extracted/lib/modules/$KVER_DIR/$mod_name" 2>/dev/null || true
                        elif [ -n "$CHROOT_WORK_DIR" ] && [ -x "/mnt/rootfs/usr/bin/zstd" ]; then
                            rel_mod="${found_mod#/mnt/rootfs}"
                            chroot /mnt/rootfs /usr/bin/zstd -d -f -c "$rel_mod" > "$WORK_DIR/extracted/lib/modules/$KVER_DIR/$mod_name" 2>/dev/null || true
                        fi
                        [ -s "$WORK_DIR/extracted/lib/modules/$KVER_DIR/$mod_name" ] && cp -f "$WORK_DIR/extracted/lib/modules/$KVER_DIR/$mod_name" "$WORK_DIR/extracted/lib/modules/$mod_name" 2>/dev/null || true
                    else
                        cp -L "$found_mod" "$WORK_DIR/extracted/lib/modules/$KVER_DIR/$mod_name" 2>/dev/null || true
                        cp -L "$found_mod" "$WORK_DIR/extracted/lib/modules/$mod_name" 2>/dev/null || true
                    fi
                fi
            done
            depmod -b "$WORK_DIR/extracted" "$KVER_DIR" 2>/dev/null || true
        fi
    fi

    # Also ensure /init loads xe / amdgpu / radeon cleanly if hardware is present (ignoring absent modules)
    sed -i 's/i915 virtio_dma_buf virtio-gpu bochs/i915 xe amdgpu radeon virtio_dma_buf virtio-gpu bochs/g' init 2>/dev/null || true

    echo "[INFO] Verifying Alder Lake & AMD GPU firmware staged in initramfs:"
    ls -lh "$WORK_DIR/extracted/lib/firmware/i915/adlp_guc_70.bin" "$WORK_DIR/extracted/lib/firmware/i915/adlp_dmc.bin"
    echo "[INFO] Staged firmware counts: i915=$(ls -1 "$WORK_DIR/extracted/lib/firmware/i915" 2>/dev/null | wc -l), amdgpu=$(ls -1 "$WORK_DIR/extracted/lib/firmware/amdgpu" 2>/dev/null | wc -l), xe=$(ls -1 "$WORK_DIR/extracted/lib/firmware/xe" 2>/dev/null | wc -l), radeon=$(ls -1 "$WORK_DIR/extracted/lib/firmware/radeon" 2>/dev/null | wc -l)"
else
    echo "[WARN] Could not locate rootfs /lib/firmware directory; initramfs firmware unchanged!" >&2
fi

echo "[INFO] Repacking updated initramfs..."
mkdir -p "$BUILD_DIR/kernel"
OUTPUT_INITRAMFS="$BUILD_DIR/kernel/initramfs.img"
rm -f "$OUTPUT_INITRAMFS"
find . | cpio -o -H newc 2>/dev/null | gzip -9 > "$OUTPUT_INITRAMFS"

echo "[SUCCESS] New initramfs generated at $OUTPUT_INITRAMFS:"
ls -lh "$OUTPUT_INITRAMFS"

# Verify the new initramfs
echo "[INFO] Verifying newly generated initramfs content..."
mkdir -p "$WORK_DIR/verify"
(cd "$WORK_DIR/verify" && gzip -dc "$OUTPUT_INITRAMFS" | cpio -idmv init 2>/dev/null)
grep -n -C 3 "switch_root" "$WORK_DIR/verify/init"

cd /workspace
rm -rf "$WORK_DIR"
echo "[SUCCESS] Initramfs rebuild & verification complete."
