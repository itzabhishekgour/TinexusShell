#pragma once

#include <txui/widgets/Widget.hpp>

namespace txui {

enum class IconType {
    Folder,
    File,
    Executable,
    Image,
    Archive
};

class Icon : public Widget {
private:
    IconType m_type{IconType::File};
    double m_size{24.0};

protected:
    Size measure_override(const Constraints& constraints) noexcept override;
    void paint_override(Painter& painter) const noexcept override;

public:
    Icon() noexcept = default;
    explicit Icon(IconType type, double size = 24.0) noexcept;
    ~Icon() override = default;

    void set_type(IconType type) noexcept;
    void set_size(double size) noexcept;
    [[nodiscard]] IconType type() const noexcept { return m_type; }
};

} // namespace txui
