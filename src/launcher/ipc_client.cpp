#include "launcher/ipc_client.hpp"
#include "common/logger.hpp"

// POSIX networking
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <sstream>

namespace tinexus::launcher {

// ---------------------------------------------------------------------------
// Wire-protocol constants — must stay in sync with ipcd/protocol/header.hpp
// ---------------------------------------------------------------------------
namespace {
    constexpr uint32_t MAGIC      = 0x544E5853; // "TNXS"
    constexpr uint16_t VER_1      = 0x0100;
    constexpr uint16_t MT_REGISTER = 0;        // SYS_REGISTER_SERVICE
    constexpr uint16_t MT_QUERY    = 2000;     // SEARCH_QUERY
    constexpr uint16_t MT_RESULT   = 2001;     // SEARCH_RESULT
    constexpr uint16_t MT_PING     = 10;
    constexpr uint16_t MT_PONG     = 11;

#pragma pack(push, 1)
    struct WireHeader {
        uint32_t magic;
        uint16_t version;
        uint16_t msg_type;
        uint16_t flags;
        uint32_t sequence_id;
        uint32_t payload_len;
        uint32_t checksum;
    };
#pragma pack(pop)
    static_assert(sizeof(WireHeader) == 22, "WireHeader must be 22 bytes");

    // Monotonic sequence ID for this client
    std::atomic<uint32_t> g_seq{0};
    uint32_t next_seq() noexcept { return ++g_seq; }
}

// ---------------------------------------------------------------------------
// Low-level socket helpers
// ---------------------------------------------------------------------------
static int connect_to_ipcd() {
    uid_t uid = getuid();
    std::string path = "/run/user/" + std::to_string(uid) + "/tinexus/ipc.sock";

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd == -1) return -1;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
        log::info("[launcher-ipc] Connected to ipcd at {}", path);
        return fd;
    }
    close(fd);
    return -1;
}

static bool write_frame(int fd, uint16_t msg_type, uint32_t seq,
                        uint16_t flags,
                        const uint8_t* payload, uint32_t payload_len) {
    WireHeader hdr{};
    hdr.magic       = MAGIC;
    hdr.version     = VER_1;
    hdr.msg_type    = msg_type;
    hdr.flags       = flags;
    hdr.sequence_id = seq;
    hdr.payload_len = payload_len;
    hdr.checksum    = 0;

    if (write(fd, &hdr, sizeof(hdr)) != static_cast<ssize_t>(sizeof(hdr))) return false;
    if (payload_len > 0 && payload) {
        if (write(fd, payload, payload_len) != static_cast<ssize_t>(payload_len)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Deserialize WireSerializer::serialize_batch() payload from searchd
// ---------------------------------------------------------------------------
static std::vector<LauncherResultItem> deserialize_results(const std::vector<uint8_t>& data) {
    std::vector<LauncherResultItem> out;
    if (data.size() < 16) return out; // too small for SearchResultWireHeader

    size_t pos = 0;
    // SearchResultWireHeader: uint32_t version, uint32_t result_count, uint64_t latency_us
    uint32_t result_count{0};
    std::memcpy(&result_count, data.data() + 4, 4);
    pos = 16; // skip full 16-byte header

    auto read_u32 = [&](uint32_t& val) -> bool {
        if (pos + 4 > data.size()) return false;
        std::memcpy(&val, data.data() + pos, 4);
        pos += 4;
        return true;
    };
    auto read_str = [&](std::string& s) -> bool {
        uint32_t len{0};
        if (!read_u32(len)) return false;
        if (pos + len > data.size()) return false;
        s.assign(reinterpret_cast<const char*>(data.data() + pos), len);
        pos += len;
        return true;
    };

    for (uint32_t i = 0; i < result_count && pos < data.size(); ++i) {
        float score{0.0f};
        if (pos + 4 > data.size()) break;
        std::memcpy(&score, data.data() + pos, 4);
        pos += 4;

        std::string title, subtitle, action, icon;
        if (!read_str(title) || !read_str(subtitle) || !read_str(action) || !read_str(icon)) break;

        // Derive category from icon or action hint
        std::string category = "Apps";
        if (icon.find("calc") != std::string::npos) category = "Calculator";
        else if (action == "lock"  || action == "shutdown" ||
                 action == "reboot" || action == "sleep" ||
                 action == "logout")                        category = "System";

        out.push_back({action, title, subtitle, icon, category, action});
    }
    return out;
}

// ===========================================================================
// IPCClient implementation
// ===========================================================================

IPCClient& IPCClient::instance() noexcept {
    static IPCClient s_instance;
    return s_instance;
}

void IPCClient::set_results_callback(ResultsCallback cb) {
    m_results_cb = std::move(cb);
}

void IPCClient::set_toggle_callback(ToggleCallback cb) {
    m_toggle_cb = std::move(cb);
}

// ---------------------------------------------------------------------------
// send_search_query — sends a real SEARCH_QUERY to ipcd.
// Falls back to local_search_fallback() when ipcd is unavailable.
// ---------------------------------------------------------------------------
void IPCClient::send_search_query(const std::string& query) {
    // Lazy connect on first query
    if (m_fd == -1) {
        m_fd = connect_to_ipcd();
        if (m_fd != -1) {
            // Register as "launcher" with ipcd
            const std::string svc_name = "launcher";
            write_frame(m_fd, MT_REGISTER, next_seq(), 0,
                        reinterpret_cast<const uint8_t*>(svc_name.data()),
                        static_cast<uint32_t>(svc_name.size()));
            start_listener_thread();
        }
    }

    if (m_fd != -1) {
        uint32_t seq = next_seq();
        bool ok = write_frame(m_fd, MT_QUERY, seq, 0,
                              reinterpret_cast<const uint8_t*>(query.data()),
                              static_cast<uint32_t>(query.size()));
        if (ok) {
            log::debug("[launcher-ipc] SEARCH_QUERY '{}' dispatched (seq={})", query, seq);
            return; // results arrive async via listener thread → m_results_cb
        } else {
            log::warn("[launcher-ipc] Write failed — reconnecting next query");
            close(m_fd);
            m_fd = -1;
        }
    }

    // No ipcd connection — use local fallback (dev/CI mode)
    local_search_fallback(query);
}

void IPCClient::start_listener_thread() {
    if (m_listener_running.exchange(true)) return; // already running

    std::thread([this]() {
        std::vector<uint8_t> buf;
        buf.reserve(8192);

        while (m_listener_running && m_fd != -1) {
            uint8_t tmp[4096];
            ssize_t n = read(m_fd, tmp, sizeof(tmp));
            if (n <= 0) {
                if (!m_listener_running) break;
                log::warn("[launcher-ipc] ipcd connection lost");
                m_fd = -1;
                break;
            }
            buf.insert(buf.end(), tmp, tmp + n);

            // Drain all complete protocol frames
            while (buf.size() >= sizeof(WireHeader)) {
                WireHeader hdr{};
                std::memcpy(&hdr, buf.data(), sizeof(hdr));
                size_t total = sizeof(hdr) + hdr.payload_len;
                if (buf.size() < total) break; // partial frame — wait

                std::vector<uint8_t> payload(buf.begin() + sizeof(hdr),
                                             buf.begin() + static_cast<ptrdiff_t>(total));
                buf.erase(buf.begin(),
                          buf.begin() + static_cast<ptrdiff_t>(total));

                if (hdr.msg_type == MT_RESULT) {
                    auto items = deserialize_results(payload);
                    log::debug("[launcher-ipc] SEARCH_RESULT: {} results received",
                               items.size());
                    if (m_results_cb) m_results_cb(items);
                } else if (hdr.msg_type == MT_PONG) {
                    log::debug("[launcher-ipc] heartbeat PONG");
                }
            }
        }

        m_listener_running = false;
        log::info("[launcher-ipc] Listener thread exited");
    }).detach();
}

void IPCClient::send_activate_item(const std::string& result_id) {
    // In v0.2 this will send ACTION_REQUEST (msg_type 1006) through ipcd.
    // In v0.1 the launcher executes directly via fork/execvp in main.cpp.
    log::info("[launcher-ipc] Activate item: '{}'", result_id);
}

// ---------------------------------------------------------------------------
// local_search_fallback — minimal results when ipcd is unavailable
// ---------------------------------------------------------------------------
void IPCClient::local_search_fallback(const std::string& query) {
    std::vector<LauncherResultItem> results;

    // Inline calculator detection (very fast: no IPC needed)
    bool all_calc = query.size() > 1;
    for (char c : query) {
        if (!std::isdigit(static_cast<unsigned char>(c)) &&
            c != '+' && c != '-' && c != '*' && c != '/' &&
            c != '.' && c != ' ' && c != '(' && c != ')') {
            all_calc = false;
            break;
        }
    }
    if (all_calc) {
        results.push_back({"calc_result", "Calculator",
                           "Type a number expression for inline calculation",
                           "accessories-calculator", "Calculator", query});
    }

    // System actions matched against query
    struct SysAction { const char* cmd; const char* label; const char* icon; };
    constexpr SysAction sys_actions[] = {
        {"lock",     "Lock Screen",  "system-lock-screen"},
        {"shutdown", "Shut Down",    "system-shutdown"},
        {"reboot",   "Restart",      "system-reboot"},
        {"sleep",    "Sleep",        "system-suspend"},
        {"logout",   "Log Out",      "system-log-out"},
    };

    std::string lq = query;
    for (char& c : lq) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    for (const auto& sa : sys_actions) {
        std::string cmd(sa.cmd);
        std::string label(sa.label);
        if (cmd.find(lq) != std::string::npos ||
            label.find(lq) != std::string::npos) {
            results.push_back({sa.cmd, sa.label, "System Action",
                               sa.icon, "System", sa.cmd});
        }
    }

    if (m_results_cb && !results.empty()) {
        m_results_cb(results);
    }
}

void IPCClient::receive_mock_results(const std::vector<LauncherResultItem>& items) {
    if (m_results_cb) m_results_cb(items);
}

void IPCClient::receive_shortcut_toggle() {
    log::info("[launcher-ipc] Ctrl+K toggle signal received");
    if (m_toggle_cb) m_toggle_cb();
}

} // namespace tinexus::launcher
