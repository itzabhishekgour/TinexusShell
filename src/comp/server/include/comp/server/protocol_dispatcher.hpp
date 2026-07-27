#ifndef TINEXUS_COMP_PROTOCOL_DISPATCHER_HPP
#define TINEXUS_COMP_PROTOCOL_DISPATCHER_HPP

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <wayland-server-core.h>

namespace tinexus::comp {

class ProtocolModule {
public:
    virtual ~ProtocolModule() = default;
    virtual std::string name() const = 0;
    virtual bool register_globals(struct wl_display* display) = 0;
    virtual void destroy() = 0;
};

class ProtocolDispatcher {
public:
    static ProtocolDispatcher& instance() noexcept;

    ProtocolDispatcher() = default;
    ~ProtocolDispatcher() = default;

    bool register_module(std::shared_ptr<ProtocolModule> module);
    std::shared_ptr<ProtocolModule> get_module(const std::string& name) const;
    bool has_module(const std::string& name) const;

    bool init_all(struct wl_display* display);
    void destroy_all();
    std::vector<std::string> registered_modules() const;

private:
    std::unordered_map<std::string, std::shared_ptr<ProtocolModule>> m_modules;
    std::vector<std::string> m_order;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_PROTOCOL_DISPATCHER_HPP
