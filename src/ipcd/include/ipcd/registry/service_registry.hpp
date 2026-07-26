#ifndef TINEXUS_IPCD_REGISTRY_SERVICE_REGISTRY_HPP
#define TINEXUS_IPCD_REGISTRY_SERVICE_REGISTRY_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <vector>

namespace tinexus::ipcd::registry {

struct ServiceInfo {
    std::string name;       // e.g. "searchd"
    int provider_fd;        // Socket FD of the service provider
    uint32_t capabilities;  // Bitmask of supported features
};

class ServiceRegistry {
public:
    static ServiceRegistry& instance() {
        static ServiceRegistry s_instance;
        return s_instance;
    }

    bool register_service(const std::string& name, int fd, uint32_t capabilities = 0) {
        std::lock_guard lock(m_mutex);
        if (m_services.find(name) != m_services.end()) {
            return false; // Already registered
        }
        m_services[name] = {name, fd, capabilities};
        return true;
    }

    bool unregister_service(const std::string& name) {
        std::lock_guard lock(m_mutex);
        return m_services.erase(name) > 0;
    }

    void unregister_by_fd(int fd) {
        std::lock_guard lock(m_mutex);
        for (auto it = m_services.begin(); it != m_services.end(); ) {
            if (it->second.provider_fd == fd) {
                it = m_services.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::optional<ServiceInfo> lookup_service(const std::string& name) const {
        std::lock_guard lock(m_mutex);
        auto it = m_services.find(name);
        if (it != m_services.end()) {
            return it->second;
        }
        return std::nullopt;
    }

private:
    ServiceRegistry() = default;
    mutable std::mutex m_mutex; // Fine to use mutex here as registry is infrequent
    std::unordered_map<std::string, ServiceInfo> m_services;
};

} // namespace tinexus::ipcd::registry

#endif // TINEXUS_IPCD_REGISTRY_SERVICE_REGISTRY_HPP
