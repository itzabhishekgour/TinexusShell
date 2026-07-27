#include "panel/panel_bar.hpp"
#include "common/logger.hpp"
#include <sstream>

namespace tinexus::panel {

std::string WorkspaceWidget::render_text() const {
    std::ostringstream ss;
    ss << "[ WS " << m_active_ws << " ]";
    return ss.str();
}

std::string ClockWidget::render_text() const {
    return "[ 17:00:00 ]";
}

std::string CpuWidget::render_text() const {
    std::ostringstream ss;
    ss << "[ CPU: " << m_usage_pct << "% ]";
    return ss.str();
}

std::string MemoryWidget::render_text() const {
    std::ostringstream ss;
    ss << "[ RAM: " << m_ram_used_gb << " GB ]";
    return ss.str();
}

std::string LauncherWidget::render_text() const {
    return "[ 🔍 Tinexus (Ctrl+K) ]";
}

PanelBar& PanelBar::instance() noexcept {
    static PanelBar s_instance;
    return s_instance;
}

void PanelBar::add_widget(std::unique_ptr<IWidget> widget) {
    if (!widget) return;
    log::info("PanelBar: Registered widget '{}'", widget->name());
    m_widgets.push_back(std::move(widget));
}

std::string PanelBar::render_bar_content() const {
    std::ostringstream ss;
    for (size_t i = 0; i < m_widgets.size(); ++i) {
        if (i > 0) ss << " | ";
        ss << m_widgets[i]->render_text();
    }
    return ss.str();
}

} // namespace tinexus::panel
