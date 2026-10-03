#include "common/GraphicsProbe.hpp"
#include "common/logger.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <cerrno>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace fs = std::filesystem;

namespace tinexus::hardware {

namespace {

// RAII wrappers guaranteeing zero resource leaks under any probe exit path
struct ScopedFd {
    int fd{-1};
    explicit ScopedFd(int f) noexcept : fd(f) {}
    ~ScopedFd() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;
};

struct ScopedGbm {
    struct gbm_device* gbm{nullptr};
    explicit ScopedGbm(struct gbm_device* g) noexcept : gbm(g) {}
    ~ScopedGbm() {
        if (gbm != nullptr) {
            gbm_device_destroy(gbm);
        }
    }
    ScopedGbm(const ScopedGbm&) = delete;
    ScopedGbm& operator=(const ScopedGbm&) = delete;
};

struct ScopedEglDisplay {
    EGLDisplay display{EGL_NO_DISPLAY};
    bool initialized{false};
    explicit ScopedEglDisplay(EGLDisplay d) noexcept : display(d) {}
    ~ScopedEglDisplay() {
        if (display != EGL_NO_DISPLAY && initialized) {
            eglTerminate(display);
        }
    }
    ScopedEglDisplay(const ScopedEglDisplay&) = delete;
    ScopedEglDisplay& operator=(const ScopedEglDisplay&) = delete;
};

struct ScopedEglContext {
    EGLDisplay display{EGL_NO_DISPLAY};
    EGLContext context{EGL_NO_CONTEXT};
    ScopedEglContext(EGLDisplay d, EGLContext c) noexcept : display(d), context(c) {}
    ~ScopedEglContext() {
        if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
        }
    }
    ScopedEglContext(const ScopedEglContext&) = delete;
    ScopedEglContext& operator=(const ScopedEglContext&) = delete;
};

std::string read_trimmed_file(const fs::path& p) {
    if (!fs::exists(p)) return {};
    std::ifstream ifs(p);
    std::string line;
    if (std::getline(ifs, line)) {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        return line;
    }
    return {};
}

} // namespace

std::vector<GpuDeviceInfo> GraphicsProbe::enumerate_gpus(const std::string& base_drm) {
    std::vector<GpuDeviceInfo> devices;

    if (!fs::exists(base_drm)) {
        return devices;
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(base_drm, ec)) {
        if (ec) break;
        std::string filename = entry.path().filename().string();

        // Match card nodes (e.g. card0, card1) and ignore connector dirs (card0-eDP-1, etc.)
        if (filename.rfind("card", 0) == 0 && filename.find('-') == std::string::npos) {
            GpuDeviceInfo info;
            info.card_name = filename;
            info.card_path = "/dev/dri/" + filename;

            // 1. Read PCI Vendor ID
            fs::path vendor_path = entry.path() / "device" / "vendor";
            std::string vendor = read_trimmed_file(vendor_path);
            std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);
            info.vendor_id = vendor;

            if (vendor.find("0x8086") != std::string::npos) {
                info.is_intel = true;
            } else if (vendor.find("0x10de") != std::string::npos) {
                info.is_nvidia = true;
            } else if (vendor.find("0x1002") != std::string::npos) {
                info.is_amd = true;
            }

            // 2. Read Kernel Driver Name
            fs::path driver_link = entry.path() / "device" / "driver";
            if (fs::exists(driver_link, ec)) {
                fs::path target = fs::read_symlink(driver_link, ec);
                if (!ec) {
                    info.driver_name = target.filename().string();
                }
            }

            // 3. Discover Associated Render Node (/sys/class/drm/cardX/device/drm/renderD*)
            fs::path drm_subsystem = entry.path() / "device" / "drm";
            if (fs::exists(drm_subsystem, ec)) {
                for (const auto& drm_entry : fs::directory_iterator(drm_subsystem, ec)) {
                    std::string dname = drm_entry.path().filename().string();
                    if (dname.rfind("renderD", 0) == 0) {
                        info.render_path = "/dev/dri/" + dname;
                        break;
                    }
                }
            }

            // If not found in device/drm, check if standard render node exists
            if (info.render_path.empty()) {
                fs::path dev_dri("/dev/dri");
                if (fs::exists(dev_dri, ec)) {
                    for (const auto& dev_entry : fs::directory_iterator(dev_dri, ec)) {
                        std::string dname = dev_entry.path().filename().string();
                        if (dname.rfind("renderD", 0) == 0) {
                            info.render_path = dev_entry.path().string();
                            break;
                        }
                    }
                }
            }

            // 4. Check for Connected Display Connectors on this Card (e.g. card0-eDP-1)
            for (const auto& conn_entry : fs::directory_iterator(base_drm, ec)) {
                std::string cname = conn_entry.path().filename().string();
                if (cname.rfind(filename + "-", 0) == 0) {
                    fs::path status_file = conn_entry.path() / "status";
                    std::string status = read_trimmed_file(status_file);
                    if (status == "connected") {
                        info.display_connected = true;
                        auto dash_pos = cname.find('-');
                        if (dash_pos != std::string::npos) {
                            info.connector_name = cname.substr(dash_pos + 1);
                        }
                        break;
                    }
                }
            }

            devices.push_back(std::move(info));
        }
    }

    return devices;
}

bool GraphicsProbe::probe_egl_gbm(const std::string& node_path, std::string& out_egl_version, std::string& out_error) {
    if (node_path.empty()) {
        out_error = "DRM device node path is empty";
        return false;
    }

    // Step 1: Open DRM node (MUST be primary card node /dev/dri/cardX for GBM scanout)
    int fd = ::open(node_path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        out_error = "Failed to open DRM node '" + node_path + "': " + std::strerror(errno);
        return false;
    }
    ScopedFd scoped_fd(fd);

    // Step 2: Validate DRM driver via libdrm ioctl
    drmVersionPtr ver = drmGetVersion(fd);
    if (!ver) {
        out_error = "drmGetVersion failed on node '" + node_path + "'";
        return false;
    }
    std::string drv_name = (ver->name != nullptr) ? ver->name : "";
    drmFreeVersion(ver);

    if (drv_name.empty()) {
        out_error = "DRM driver name is empty on '" + node_path + "'";
        return false;
    }
    log::info("[graphics] DRM kernel driver identified on '{}': {}", node_path, drv_name);

    // Step 3: Instantiate GBM device
    struct gbm_device* gbm = gbm_create_device(fd);
    if (!gbm) {
        out_error = "gbm_create_device failed on node '" + node_path + "'";
        return false;
    }
    ScopedGbm scoped_gbm(gbm);

    // Step 4: Obtain EGL platform display using GBM
    EGLDisplay egl_display = EGL_NO_DISPLAY;
#if defined(EGL_PLATFORM_GBM_KHR)
    egl_display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, gbm, nullptr);
#endif
    if (egl_display == EGL_NO_DISPLAY) {
        egl_display = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(gbm));
    }

    if (egl_display == EGL_NO_DISPLAY) {
        out_error = "eglGetPlatformDisplay failed for GBM device on '" + node_path + "'";
        return false;
    }
    ScopedEglDisplay scoped_egl(egl_display);

    // Step 5: Initialize EGL display
    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(egl_display, &major, &minor)) {
        EGLint err = eglGetError();
        std::ostringstream ss;
        ss << "eglInitialize failed on EGLDisplay for '" << node_path << "' (eglError: 0x" << std::hex << err << ")";
        out_error = ss.str();
        return false;
    }
    scoped_egl.initialized = true;
    out_egl_version = std::to_string(major) + "." + std::to_string(minor);

    // Step 6: Test EGL OpenGL ES API binding
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        out_error = "eglBindAPI(EGL_OPENGL_ES_API) failed";
        return false;
    }

    // Step 7: Probe GLES2 frame buffer config
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config = nullptr;
    EGLint num_configs = 0;
    if (!eglChooseConfig(egl_display, config_attribs, &config, 1, &num_configs) || num_configs < 1) {
        EGLint err = eglGetError();
        std::ostringstream ss;
        ss << "eglChooseConfig found no matching GLES2 8888 configs on '" << node_path << "' (eglError: 0x" << std::hex << err << ")";
        out_error = ss.str();
        return false;
    }

    // Step 8: Verify GLES2 context creation
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLContext context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        EGLint err = eglGetError();
        std::ostringstream ss;
        ss << "eglCreateContext failed to allocate GLES2 context on '" << node_path << "' (eglError: 0x" << std::hex << err << ")";
        out_error = ss.str();
        return false;
    }
    ScopedEglContext scoped_ctx(egl_display, context);

    return true;
}

GraphicsProbeResult GraphicsProbe::evaluate(const std::string& base_drm) {
    GraphicsProbeResult result;
    result.decision = RendererDecision::Pixman;
    result.selected_renderer = "pixman";
    result.selected_card_node = ""; // Left unset by default
    result.hardware_available = false;

    // 1. Enumerate GPUs
    auto devices = enumerate_gpus(base_drm);
    if (devices.empty()) {
        result.reason = "No DRM graphics cards detected in " + base_drm;
        return result;
    }

    // 2. Universal Candidate Selection
    const GpuDeviceInfo* candidate = nullptr;

    // Priority 1: In hybrid setups (Intel/AMD integrated + NVIDIA discrete),
    // prioritize an integrated GPU with an internal connected display (eDP-1).
    for (const auto& dev : devices) {
        if ((dev.is_intel || dev.is_amd) && dev.display_connected) {
            candidate = &dev;
            break;
        }
    }

    // Priority 2: If no iGPU with display was found, look for ANY GPU with a connected display
    // (e.g. Pure NVIDIA laptop/desktop, external HDMI monitor, or single GPU system).
    if (!candidate) {
        for (const auto& dev : devices) {
            if (dev.display_connected) {
                candidate = &dev;
                break;
            }
        }
    }

    // Priority 3: Fall back to first iGPU or first detected GPU
    if (!candidate) {
        for (const auto& dev : devices) {
            if (dev.is_intel || dev.is_amd) {
                candidate = &dev;
                break;
            }
        }
        if (!candidate) {
            candidate = &devices.front();
        }
    }

    // Log ignored secondary discrete GPUs in hybrid setup
    for (const auto& dev : devices) {
        if (&dev != candidate && dev.is_nvidia) {
            log::info("[graphics] Ignoring secondary discrete GPU: NVIDIA (PCI: {}, Card: {})",
                      dev.vendor_id, dev.card_path);
        }
    }

    result.device = *candidate;
    log::info("[graphics] Evaluated primary candidate GPU: {} (Vendor: {}, Driver: {}, Connected Display: {})",
              candidate->card_path, candidate->vendor_id, candidate->driver_name,
              candidate->display_connected ? candidate->connector_name : "none");

    // 3. Verify connected display status
    if (!candidate->display_connected) {
        result.reason = candidate->card_name + " has no active connected display connector";
        return result;
    }

    // 4. Determine KMS Card Node to probe (CRITICAL: ALWAYS probe card_path, NEVER render_path)
    std::string probe_node = candidate->card_path;
    if (probe_node.empty() || !fs::exists(probe_node)) {
        result.reason = "Primary DRM card node does not exist: " + probe_node;
        return result;
    }

    // 5. Execute conservative capability check on primary KMS card node
    log::info("[graphics] Hardware renderer capability probe started on {}", probe_node);
    std::string egl_ver;
    std::string probe_err;
    bool probe_ok = probe_egl_gbm(probe_node, egl_ver, probe_err);

    if (!probe_ok) {
        result.reason = probe_err;
        return result;
    }

    // 6. Capability probe succeeded!
    result.decision = RendererDecision::HardwareGles2;
    result.selected_renderer = "gles2";
    result.selected_card_node = candidate->card_path;
    result.egl_version = egl_ver;
    result.hardware_available = true;
    result.reason = candidate->card_name + " GLES2 hardware acceleration verified (EGL " + egl_ver + ")";

    return result;
}

} // namespace tinexus::hardware
