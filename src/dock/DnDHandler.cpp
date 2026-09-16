// ============================================================================
// DnDHandler.cpp — Wayland wl_data_device Drag-and-Drop Handler (Slice 6)
// Ref: Architecture Blueprint §4.2, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
#include "dock/DnDHandler.hpp"
#include "dock/DockBridge.hpp"
#include <common/logger.hpp>
#include <QtCore/QUrl>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cmath>

namespace tinexus::dock {

DnDHandler::DnDHandler(QObject* parent)
    : QObject(parent)
{
}

DnDHandler::~DnDHandler() {
    shutdown();
}

bool DnDHandler::init(struct wl_display* display, struct wl_seat* seat, DockBridge* bridge) {
    m_display = display;
    m_seat = seat;
    m_bridge = bridge;

    if (!m_display) {
        tinexus::log::error("[DnDHandler] init called with null wl_display");
        return false;
    }

    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        tinexus::log::error("[DnDHandler] Failed to get Wayland registry");
        return false;
    }

    static const struct wl_registry_listener registry_listener = {
        .global        = handleRegistryGlobal,
        .global_remove = handleRegistryGlobalRemove,
    };
    wl_registry_add_listener(m_registry, &registry_listener, this);

    wl_display_roundtrip(m_display);

    if (!m_manager) {
        tinexus::log::warn("[DnDHandler] wl_data_device_manager global not found on display — running in fallback mode");
        return false;
    }

    if (!m_seat) {
        tinexus::log::warn("[DnDHandler] wl_seat not available for data device");
        return false;
    }

    m_dataDevice = wl_data_device_manager_get_data_device(m_manager, m_seat);
    if (!m_dataDevice) {
        tinexus::log::error("[DnDHandler] Failed to create wl_data_device from seat");
        return false;
    }

    static const struct wl_data_device_listener device_listener = {
        .data_offer = handleDeviceDataOffer,
        .enter      = handleDeviceEnter,
        .leave      = handleDeviceLeave,
        .motion     = handleDeviceMotion,
        .drop       = handleDeviceDrop,
        .selection  = handleDeviceSelection,
    };
    wl_data_device_add_listener(m_dataDevice, &device_listener, this);

    wl_display_flush(m_display);
    tinexus::log::info("[DnDHandler] Initialized wl_data_device successfully (manager version={})", m_managerVersion);
    return true;
}

void DnDHandler::shutdown() {
    cleanupPipe();

    if (m_currentOffer) {
        wl_data_offer_destroy(m_currentOffer);
        m_currentOffer = nullptr;
    }

    if (m_dataDevice) {
        wl_data_device_destroy(m_dataDevice);
        m_dataDevice = nullptr;
    }

    if (m_manager) {
        wl_data_device_manager_destroy(m_manager);
        m_manager = nullptr;
    }

    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }

    clearTarget();
    setState(DnDState::IDLE);
}

void DnDHandler::setState(DnDState state) {
    if (m_state != state) {
        m_state = state;
        emit stateChanged(static_cast<int>(m_state));
    }
}

void DnDHandler::updateTargetFromCoords(double x, double) {
    if (!m_bridge) return;

    const auto& icons = m_bridge->rawIcons();
    const double baseSize = m_bridge->baseSize();

    for (size_t i = 0; i < icons.size(); ++i) {
        const auto& icon = icons[i];
        if (icon.iconType == QStringLiteral("separator")) continue;

        const double scale = icon.scaleSpring.value + icon.bounceSpring.value;
        const double half = (baseSize * scale) / 2.0;

        if (std::abs(x - icon.centerX) <= half) {
            const int idx = static_cast<int>(i);
            if (m_targetIndex != idx) {
                m_targetIndex = idx;
                m_targetAppId = icon.appId;
                m_bridge->setDropTargetIndex(m_targetIndex);
                emit targetIndexChanged(m_targetIndex);
                emit targetAppIdChanged(m_targetAppId);
                tinexus::log::debug("[DnDHandler] Target icon under cursor: index={} appId='{}'",
                                    m_targetIndex, m_targetAppId.toStdString());
            }
            return;
        }
    }

    clearTarget();
}

void DnDHandler::clearTarget() {
    if (m_targetIndex != -1) {
        m_targetIndex = -1;
        m_targetAppId.clear();
        if (m_bridge) m_bridge->setDropTargetIndex(-1);
        emit targetIndexChanged(-1);
        emit targetAppIdChanged(QString());
    }
}

void DnDHandler::cleanupPipe() {
    if (m_pipeNotifier) {
        m_pipeNotifier->setEnabled(false);
        m_pipeNotifier.reset();
    }
    if (m_pipeReadFd >= 0) {
        ::close(m_pipeReadFd);
        m_pipeReadFd = -1;
    }
}

void DnDHandler::onPipeReadable() {
    if (m_pipeReadFd < 0) return;

    char buf[4096];
    while (true) {
        ssize_t n = ::read(m_pipeReadFd, buf, sizeof(buf));
        if (n > 0) {
            m_readBuffer.append(buf, static_cast<int>(n));
        } else if (n == 0) {
            // EOF reached: sender finished writing and closed pipe
            tinexus::log::info("[DnDHandler] Async pipe EOF reached. Total bytes read: {}", m_readBuffer.size());
            finishReceiving();
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Buffer drained, waiting for more data via QSocketNotifier
                break;
            } else {
                tinexus::log::error("[DnDHandler] Pipe read error: {}", std::strerror(errno));
                cleanupPipe();
                clearTarget();
                setState(DnDState::IDLE);
                break;
            }
        }
    }
}

void DnDHandler::finishReceiving() {
    cleanupPipe();

    if (m_currentOffer) {
        if (m_managerVersion >= 3) {
            wl_data_offer_finish(m_currentOffer);
        }
        wl_data_offer_destroy(m_currentOffer);
        m_currentOffer = nullptr;
    }

    setState(DnDState::LAUNCHING);

    // Parse text/uri-list (RFC 2483 format: CRLF separated list of URIs)
    QString rawPayload = QString::fromUtf8(m_readBuffer);
    QStringList lines = rawPayload.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
    QStringList validUris;

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) continue;

        QUrl url(trimmed);
        QString localPath = url.isLocalFile() ? url.toLocalFile() : trimmed;
        if (localPath.startsWith(QStringLiteral("file://"))) {
            localPath = localPath.mid(7);
        }
        validUris.append(localPath);
    }

    tinexus::log::info("[DnDHandler] Extracted {} valid file URIs for target appId='{}'",
                       validUris.size(), m_targetAppId.toStdString());

    if (!validUris.isEmpty() && m_bridge && !m_targetAppId.isEmpty()) {
        m_bridge->launchWithUris(m_targetAppId, validUris);
        emit dropReceived(m_targetAppId, validUris);
    }

    clearTarget();
    setState(DnDState::IDLE);
}

void DnDHandler::simulateHover(int index) {
    if (!m_bridge) return;
    m_targetIndex = index;
    if (index >= 0 && index < static_cast<int>(m_bridge->rawIcons().size())) {
        m_targetAppId = m_bridge->rawIcons()[static_cast<size_t>(index)].appId;
    } else {
        m_targetAppId.clear();
    }
    m_bridge->setDropTargetIndex(index);
    emit targetIndexChanged(m_targetIndex);
    emit targetAppIdChanged(m_targetAppId);
    setState(index >= 0 ? DnDState::HOVERING : DnDState::IDLE);
}

void DnDHandler::simulateDrop(const QString& appId, const QStringList& uris) {
    tinexus::log::info("[DnDHandler] Simulating file drop for appId='{}' with {} files",
                       appId.toStdString(), uris.size());
    setState(DnDState::DROPPED);
    setState(DnDState::RECEIVING);
    setState(DnDState::LAUNCHING);
    if (m_bridge) {
        m_bridge->launchWithUris(appId, uris);
    }
    emit dropReceived(appId, uris);
    clearTarget();
    setState(DnDState::IDLE);
}

// ── Static Registry Callbacks ────────────────────────────────────────────────
void DnDHandler::handleRegistryGlobal(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    auto* self = static_cast<DnDHandler*>(data);
    if (std::strcmp(interface, "wl_data_device_manager") == 0) {
        const uint32_t bindVersion = (version >= 3 ? 3 : version);
        self->m_manager = static_cast<wl_data_device_manager*>(
            wl_registry_bind(registry, name, &wl_data_device_manager_interface, bindVersion));
        self->m_managerGlobalName = name;
        self->m_managerVersion = bindVersion;
        tinexus::log::info("[DnDHandler] Bound wl_data_device_manager version {}", bindVersion);
    } else if (std::strcmp(interface, "wl_seat") == 0) {
        if (!self->m_seat) {
            self->m_seat = static_cast<wl_seat*>(
                wl_registry_bind(registry, name, &wl_seat_interface, version >= 7 ? 7 : version));
            tinexus::log::info("[DnDHandler] Bound default wl_seat (global={})", name);
        }
    }
}

void DnDHandler::handleRegistryGlobalRemove(void* data, struct wl_registry*, uint32_t name) {
    auto* self = static_cast<DnDHandler*>(data);
    if (self->m_managerGlobalName == name && self->m_manager) {
        tinexus::log::warn("[DnDHandler] wl_data_device_manager global removed");
        wl_data_device_manager_destroy(self->m_manager);
        self->m_manager = nullptr;
        self->m_managerGlobalName = 0;
    }
}

// ── Static wl_data_device Callbacks ──────────────────────────────────────────
void DnDHandler::handleDeviceDataOffer(void* data, struct wl_data_device*, struct wl_data_offer* id) {
    auto* self = static_cast<DnDHandler*>(data);
    if (!id) return;

    static const struct wl_data_offer_listener offer_listener = {
        .offer          = handleOfferOffer,
        .source_actions = handleOfferSourceActions,
        .action         = handleOfferAction,
    };
    wl_data_offer_add_listener(id, &offer_listener, self);
    self->m_offerHasUris = false;
}

void DnDHandler::handleDeviceEnter(void* data, struct wl_data_device*, uint32_t serial, struct wl_surface*, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer* id) {
    auto* self = static_cast<DnDHandler*>(data);
    self->m_enterSerial = serial;
    self->m_currentOffer = id;

    if (id && self->m_offerHasUris) {
        wl_data_offer_accept(id, serial, "text/uri-list");
        if (self->m_managerVersion >= 3) {
            wl_data_offer_set_actions(id, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
        }
    }

    self->setState(DnDState::HOVERING);
    self->updateTargetFromCoords(wl_fixed_to_double(x), wl_fixed_to_double(y));
    tinexus::log::debug("[DnDHandler] Pointer entered dock surface during DnD (offerHasUris={})", self->m_offerHasUris);
}

void DnDHandler::handleDeviceLeave(void* data, struct wl_data_device*) {
    auto* self = static_cast<DnDHandler*>(data);
    self->clearTarget();

    if (self->m_currentOffer && self->m_state != DnDState::RECEIVING && self->m_state != DnDState::DROPPED) {
        wl_data_offer_destroy(self->m_currentOffer);
        self->m_currentOffer = nullptr;
    }

    self->setState(DnDState::IDLE);
    tinexus::log::debug("[DnDHandler] Pointer left dock surface during DnD");
}

void DnDHandler::handleDeviceMotion(void* data, struct wl_data_device*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
    auto* self = static_cast<DnDHandler*>(data);
    self->updateTargetFromCoords(wl_fixed_to_double(x), wl_fixed_to_double(y));
}

void DnDHandler::handleDeviceDrop(void* data, struct wl_data_device*) {
    auto* self = static_cast<DnDHandler*>(data);
    if (!self->m_currentOffer || !self->m_offerHasUris || self->m_targetAppId.isEmpty()) {
        tinexus::log::warn("[DnDHandler] Drop ignored: invalid offer, no URIs, or no target icon");
        self->clearTarget();
        if (self->m_currentOffer) {
            wl_data_offer_destroy(self->m_currentOffer);
            self->m_currentOffer = nullptr;
        }
        self->setState(DnDState::IDLE);
        return;
    }

    self->setState(DnDState::DROPPED);

    int pipeFds[2];
#if defined(__linux__)
    if (::pipe2(pipeFds, O_CLOEXEC | O_NONBLOCK) != 0) {
        tinexus::log::error("[DnDHandler] pipe2 failed: {}", std::strerror(errno));
        self->clearTarget();
        return;
    }
#else
    if (::pipe(pipeFds) != 0) {
        tinexus::log::error("[DnDHandler] pipe failed: {}", std::strerror(errno));
        self->clearTarget();
        return;
    }
    ::fcntl(pipeFds[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(pipeFds[1], F_SETFD, FD_CLOEXEC);
    ::fcntl(pipeFds[0], F_SETFL, O_NONBLOCK);
    ::fcntl(pipeFds[1], F_SETFL, O_NONBLOCK);
#endif

    // Ask offer to stream text/uri-list into write end of the pipe
    wl_data_offer_receive(self->m_currentOffer, "text/uri-list", pipeFds[1]);
    wl_display_flush(self->m_display);

    // CRITICAL: Close write end immediately in this process so pipe hits EOF when source finishes!
    ::close(pipeFds[1]);

    self->m_pipeReadFd = pipeFds[0];
    self->m_readBuffer.clear();
    self->setState(DnDState::RECEIVING);

    // Monitor read end asynchronously using QSocketNotifier — NEVER block Qt main thread
    self->m_pipeNotifier = std::make_unique<QSocketNotifier>(self->m_pipeReadFd, QSocketNotifier::Read, self);
    connect(self->m_pipeNotifier.get(), &QSocketNotifier::activated, self, &DnDHandler::onPipeReadable);

    tinexus::log::info("[DnDHandler] Drop dispatched. Awaiting asynchronous pipe stream on fd={}", self->m_pipeReadFd);
}

void DnDHandler::handleDeviceSelection(void*, struct wl_data_device*, struct wl_data_offer*) {
    // Regular clipboard selection — not used for file drop
}

// ── Static wl_data_offer Callbacks ───────────────────────────────────────────
void DnDHandler::handleOfferOffer(void* data, struct wl_data_offer*, const char* mime_type) {
    auto* self = static_cast<DnDHandler*>(data);
    if (!mime_type) return;

    if (std::strcmp(mime_type, "text/uri-list") == 0 ||
        std::strcmp(mime_type, "application/x-kde-urilist") == 0) {
        self->m_offerHasUris = true;
        tinexus::log::debug("[DnDHandler] Discovered supported MIME: {}", mime_type);
    }
}

void DnDHandler::handleOfferSourceActions(void*, struct wl_data_offer*, uint32_t) {
}

void DnDHandler::handleOfferAction(void*, struct wl_data_offer*, uint32_t) {
}

} // namespace tinexus::dock

#include "moc_DnDHandler.cpp"
