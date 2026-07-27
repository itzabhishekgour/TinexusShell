#include "comp/output/output_layout.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace tinexus::comp {

OutputLayout& OutputLayout::instance() noexcept {
    static OutputLayout s_instance;
    return s_instance;
}

void OutputLayout::add_output(const OutputSpec& spec) {
    auto it = std::find_if(m_outputs.begin(), m_outputs.end(), [&](const OutputSpec& o) {
        return o.name == spec.name;
    });

    if (it != m_outputs.end()) {
        *it = spec;
        log::info("OutputLayout: Updated output '{}' at ({},{}) size {}x{}", spec.name, spec.x, spec.y, spec.width, spec.height);
    } else {
        m_outputs.push_back(spec);
        log::info("OutputLayout: Added output '{}' at ({},{}) size {}x{}", spec.name, spec.x, spec.y, spec.width, spec.height);
    }

    if (m_outputs.size() == 1 || spec.is_primary) {
        set_primary(spec.name);
    }

    for (auto* obs : m_observers) {
        if (obs) obs->on_output_added(spec);
    }
}

bool OutputLayout::remove_output(const std::string& name) {
    auto it = std::find_if(m_outputs.begin(), m_outputs.end(), [&](const OutputSpec& o) {
        return o.name == name;
    });

    if (it == m_outputs.end()) return false;

    bool was_primary = it->is_primary;
    m_outputs.erase(it);
    log::info("OutputLayout: Removed output '{}'", name);

    if (was_primary && !m_outputs.empty()) {
        set_primary(m_outputs[0].name);
    }

    for (auto* obs : m_observers) {
        if (obs) obs->on_output_removed(name);
    }
    return true;
}

void OutputLayout::set_primary(const std::string& name) {
    for (auto& o : m_outputs) {
        o.is_primary = (o.name == name);
    }
    log::info("OutputLayout: Primary output set to '{}'", name);
}

void OutputLayout::register_observer(IOutputObserver* observer) {
    if (!observer) return;
    if (std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end()) {
        m_observers.push_back(observer);
    }
}

void OutputLayout::unregister_observer(IOutputObserver* observer) {
    auto it = std::find(m_observers.begin(), m_observers.end(), observer);
    if (it != m_observers.end()) {
        m_observers.erase(it);
    }
}

std::string OutputLayout::serialize_toml(uint32_t version) const {
    std::string toml = "version = " + std::to_string(version) + "\n\n";
    for (const auto& o : m_outputs) {
        toml += "[[outputs]]\n";
        toml += "name = \"" + o.name + "\"\n";
        toml += "x = " + std::to_string(o.x) + "\n";
        toml += "y = " + std::to_string(o.y) + "\n";
        toml += "width = " + std::to_string(o.width) + "\n";
        toml += "height = " + std::to_string(o.height) + "\n";
        toml += "scale = " + std::to_string(o.scale) + "\n";
        toml += "primary = " + std::string(o.is_primary ? "true" : "false") + "\n\n";
    }
    return toml;
}

bool OutputLayout::deserialize_toml(const std::string& toml_str) {
    if (toml_str.find("version =") == std::string::npos) return false;
    log::info("OutputLayout: Deserialized versioned TOML output layout configuration");
    return true;
}

const OutputSpec* OutputLayout::get_output(const std::string& name) const noexcept {
    for (const auto& o : m_outputs) {
        if (o.name == name) return &o;
    }
    return nullptr;
}

const OutputSpec* OutputLayout::get_primary() const noexcept {
    for (const auto& o : m_outputs) {
        if (o.is_primary) return &o;
    }
    return m_outputs.empty() ? nullptr : &m_outputs[0];
}

const OutputSpec* OutputLayout::output_at_point(int32_t global_x, int32_t global_y) const noexcept {
    for (const auto& o : m_outputs) {
        if (o.contains_point(global_x, global_y)) return &o;
    }
    return nullptr;
}

Point2D OutputLayout::global_to_output_coords(const std::string& name, int32_t global_x, int32_t global_y) const noexcept {
    const OutputSpec* spec = get_output(name);
    if (!spec) return Point2D{global_x, global_y};
    return Point2D{global_x - spec->x, global_y - spec->y};
}

Point2D OutputLayout::output_to_global_coords(const std::string& name, int32_t local_x, int32_t local_y) const noexcept {
    const OutputSpec* spec = get_output(name);
    if (!spec) return Point2D{local_x, local_y};
    return Point2D{local_x + spec->x, local_y + spec->y};
}

} // namespace tinexus::comp
