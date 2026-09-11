// ============================================================================
// IpcBridge.hpp + IpcBridge.cpp (combined header for the gate test)
//
// Qt6 equivalent of the TxUI dock's:
//     window->event_loop()->add_fd(ipc_fd, [ipc_fd, ...](int, uint32_t) { ... })
//
// Uses QSocketNotifier inside a QObject-derived bridge, wired to Q_PROPERTY
// signals so QML can bind to live IPC data without polling.
//
// Pattern proven here becomes the template for tinexus-dock, tinexus-shell,
// tinexus-launcher Qt6 migrations.
// ============================================================================
#pragma once
#include <QtCore/QObject>
#include <QtCore/QSocketNotifier>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <memory>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>   // recv, MSG_PEEK, MSG_DONTWAIT

// ipcd wire protocol (copied inline to avoid txui linkage in this gate test)
namespace gate::ipc_proto {
    constexpr uint32_t MAGIC   = 0x544E5853; // "TNXS"
    constexpr uint16_t VER_1   = 0x0100;

    #pragma pack(push, 1)
    struct Header {
        uint32_t magic;
        uint16_t version;
        uint16_t msg_type;
        uint16_t flags;
        uint32_t sequence_id;
        uint32_t payload_len;
        uint32_t checksum;
    };
    #pragma pack(pop)
    static_assert(sizeof(Header) == 22, "Header must be 22 bytes");
}

// ── IpcBridge ─────────────────────────────────────────────────────────────────
class IpcBridge : public QObject {
    Q_OBJECT

    // Properties exposed to QML
    Q_PROPERTY(int  messagesReceived READ messagesReceived NOTIFY statsChanged)
    Q_PROPERTY(int  bytesReceived    READ bytesReceived    NOTIFY statsChanged)
    Q_PROPERTY(bool connected        READ isConnected      NOTIFY connectedChanged)
    Q_PROPERTY(QString lastAppId     READ lastAppId        NOTIFY appStateChanged)

public:
    explicit IpcBridge(QObject* parent = nullptr) : QObject(parent) {}

    // Called from main after socket pair is created
    void connectToFd(int read_fd) {
        m_fd = read_fd;
        m_notifier = std::make_unique<QSocketNotifier>(read_fd, QSocketNotifier::Read, this);
        connect(m_notifier.get(), &QSocketNotifier::activated,
                this, &IpcBridge::onSocketReadable);
        m_connected = true;
        emit connectedChanged();
    }

    // Q_PROPERTY getters
    int     messagesReceived() const { return m_messages_received; }
    int     bytesReceived()    const { return m_bytes_received; }
    bool    isConnected()      const { return m_connected; }
    QString lastAppId()        const { return m_last_app_id; }

    // QML invokable: inject test IPC message into the write end (for load testing)
    Q_INVOKABLE bool injectTestMessage(int write_fd, uint16_t msg_type,
                                       const QString& app_id)
    {
        struct __attribute__((packed)) TestMsg {
            gate::ipc_proto::Header hdr;
            char app_id[128];
        } msg{};
        msg.hdr.magic       = gate::ipc_proto::MAGIC;
        msg.hdr.version     = gate::ipc_proto::VER_1;
        msg.hdr.msg_type    = msg_type;
        msg.hdr.payload_len = sizeof(msg.app_id);
        QByteArray id_bytes = app_id.toUtf8();
        std::strncpy(msg.app_id, id_bytes.constData(),
                     sizeof(msg.app_id) - 1);

        const char* ptr = reinterpret_cast<const char*>(&msg);
        size_t rem = sizeof(msg);
        while (rem > 0) {
            ssize_t n = ::write(write_fd, ptr, rem);
            if (n <= 0) {
                if (errno == EINTR) continue;
                return false;
            }
            ptr += n;
            rem -= static_cast<size_t>(n);
        }
        return true;
    }

signals:
    void statsChanged();
    void connectedChanged();
    void appStateChanged(QString appId, uint16_t msgType);
    void frameDropDetected();

private slots:
    void onSocketReadable() {
        // Mirror of dock's socket read loop — drain all pending messages
        using H = gate::ipc_proto::Header;
        while (true) {
            H hdr{};
            ssize_t peek_n = ::recv(m_fd, &hdr, sizeof(hdr), MSG_PEEK | MSG_DONTWAIT);
            if (peek_n == 0) {
                // Peer closed write end
                if (m_notifier) m_notifier->setEnabled(false);
                break;
            }
            if (peek_n < static_cast<ssize_t>(sizeof(hdr))) break;
            if (hdr.magic != gate::ipc_proto::MAGIC) {
                char c;
                if (::recv(m_fd, &c, 1, 0) <= 0) break;
                continue;
            }

            const size_t total = sizeof(hdr) + hdr.payload_len;
            QByteArray buf(static_cast<int>(total), '\0');
            ssize_t peek_total = ::recv(m_fd, buf.data(), total, MSG_PEEK | MSG_DONTWAIT);
            if (peek_total < static_cast<ssize_t>(total)) {
                // Incomplete message payload in buffer, wait for more data
                break;
            }

            ssize_t n = ::recv(m_fd, buf.data(), total, MSG_DONTWAIT);
            if (n != static_cast<ssize_t>(total)) break;

            m_messages_received++;
            m_bytes_received += static_cast<int>(n);

            // Extract app_id from payload when present
            if (hdr.payload_len >= 128) {
                const char* payload = buf.constData() + sizeof(hdr);
                m_last_app_id = QString::fromUtf8(payload, 127);
                emit appStateChanged(m_last_app_id, hdr.msg_type);
            }
            emit statsChanged();
        }
    }

private:
    int     m_fd                  {-1};
    bool    m_connected           {false};
    int     m_messages_received   {0};
    int     m_bytes_received      {0};
    QString m_last_app_id;
    std::unique_ptr<QSocketNotifier> m_notifier;
};
