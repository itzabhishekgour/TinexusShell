// ============================================================================
// DnDHandler.hpp — Wayland wl_data_device Drag-and-Drop Handler (Slice 6)
// Ref: Architecture Blueprint §4.2, docs/05_UI_UX_GUIDELINES.md
// Manages wl_data_device_manager, wl_data_device, wl_data_offer lifecycle,
// asynchronous pipe reading via QSocketNotifier, and URI launching.
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QByteArray>
#include <QtCore/QSocketNotifier>
#include <memory>
#include <wayland-client.h>

namespace tinexus::dock {

class DockBridge;

enum class DnDState : uint8_t {
    IDLE      = 0,
    HOVERING  = 1,
    DROPPED   = 2,
    RECEIVING = 3,
    LAUNCHING = 4
};

class DnDHandler : public QObject {
    Q_OBJECT

    Q_PROPERTY(int state READ stateInt NOTIFY stateChanged)
    Q_PROPERTY(int targetIndex READ targetIndex NOTIFY targetIndexChanged)
    Q_PROPERTY(QString targetAppId READ targetAppId NOTIFY targetAppIdChanged)

public:
    explicit DnDHandler(QObject* parent = nullptr);
    ~DnDHandler() override;

    bool init(struct wl_display* display, struct wl_seat* seat, DockBridge* bridge);
    void shutdown();

    [[nodiscard]] DnDState state() const { return m_state; }
    [[nodiscard]] int stateInt() const { return static_cast<int>(m_state); }
    [[nodiscard]] int targetIndex() const { return m_targetIndex; }
    [[nodiscard]] QString targetAppId() const { return m_targetAppId; }

    void simulateHover(int index);
    void simulateDrop(const QString& appId, const QStringList& uris);

signals:
    void stateChanged(int state);
    void targetIndexChanged(int index);
    void targetAppIdChanged(const QString& appId);
    void dropReceived(const QString& appId, const QStringList& uris);

private slots:
    void onPipeReadable();

private:
    void setState(DnDState state);
    void updateTargetFromCoords(double x, double y);
    void clearTarget();
    void finishReceiving();
    void cleanupPipe();

    // ── Static Wayland Callbacks ─────────────────────────────────────────────
    static void handleRegistryGlobal(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
    static void handleRegistryGlobalRemove(void* data, struct wl_registry* registry, uint32_t name);

    // wl_data_device callbacks
    static void handleDeviceDataOffer(void* data, struct wl_data_device* device, struct wl_data_offer* id);
    static void handleDeviceEnter(void* data, struct wl_data_device* device, uint32_t serial, struct wl_surface* surface, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer* id);
    static void handleDeviceLeave(void* data, struct wl_data_device* device);
    static void handleDeviceMotion(void* data, struct wl_data_device* device, uint32_t time, wl_fixed_t x, wl_fixed_t y);
    static void handleDeviceDrop(void* data, struct wl_data_device* device);
    static void handleDeviceSelection(void* data, struct wl_data_device* device, struct wl_data_offer* id);

    // wl_data_offer callbacks
    static void handleOfferOffer(void* data, struct wl_data_offer* offer, const char* mime_type);
    static void handleOfferSourceActions(void* data, struct wl_data_offer* offer, uint32_t source_actions);
    static void handleOfferAction(void* data, struct wl_data_offer* offer, uint32_t dnd_action);

    struct wl_display*             m_display{nullptr};
    struct wl_registry*            m_registry{nullptr};
    struct wl_seat*                m_seat{nullptr};
    struct wl_data_device_manager* m_manager{nullptr};
    struct wl_data_device*         m_dataDevice{nullptr};
    struct wl_data_offer*          m_currentOffer{nullptr};

    uint32_t m_managerGlobalName{0};
    uint32_t m_managerVersion{0};
    bool     m_offerHasUris{false};
    uint32_t m_enterSerial{0};

    int                                m_pipeReadFd{-1};
    std::unique_ptr<QSocketNotifier>   m_pipeNotifier;
    QByteArray                         m_readBuffer;

    DockBridge* m_bridge{nullptr};
    DnDState    m_state{DnDState::IDLE};
    int         m_targetIndex{-1};
    QString     m_targetAppId;
};

} // namespace tinexus::dock
