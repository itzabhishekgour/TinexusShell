#ifndef TINEXUS_PANEL_BAR_HPP
#define TINEXUS_PANEL_BAR_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace tinexus::panel {

class IWidget {
public:
    virtual ~IWidget() = default;

    [[nodiscard]] virtual std::string name() const noexcept = 0;
    [[nodiscard]] virtual std::string render_text() const = 0;
};

class WorkspaceWidget : public IWidget {
public:
    explicit WorkspaceWidget(uint32_t active_ws = 1) : m_active_ws(active_ws) {}
    std::string name() const noexcept override { return "WorkspaceWidget"; }
    std::string render_text() const override;

private:
    uint32_t m_active_ws{1};
};

class ClockWidget : public IWidget {
public:
    std::string name() const noexcept override { return "ClockWidget"; }
    std::string render_text() const override;
};

class CpuWidget : public IWidget {
public:
    explicit CpuWidget(float usage_pct = 12.5f) : m_usage_pct(usage_pct) {}
    std::string name() const noexcept override { return "CpuWidget"; }
    std::string render_text() const override;

private:
    float m_usage_pct{12.5f};
};

class MemoryWidget : public IWidget {
public:
    explicit MemoryWidget(float ram_used_gb = 4.2f) : m_ram_used_gb(ram_used_gb) {}
    std::string name() const noexcept override { return "MemoryWidget"; }
    std::string render_text() const override;

private:
    float m_ram_used_gb{4.2f};
};

class LauncherWidget : public IWidget {
public:
    std::string name() const noexcept override { return "LauncherWidget"; }
    std::string render_text() const override;
};

class PanelBar {
public:
    static PanelBar& instance() noexcept;

    PanelBar() = default;
    ~PanelBar() = default;

    void add_widget(std::unique_ptr<IWidget> widget);
    [[nodiscard]] std::string render_bar_content() const;
    [[nodiscard]] uint32_t exclusive_height() const noexcept { return 48; }

private:
    std::vector<std::unique_ptr<IWidget>> m_widgets;
};

} // namespace tinexus::panel

#endif // TINEXUS_PANEL_BAR_HPP
