#pragma once

#include <txui/window/Window.hpp>
#include "ColumnBrowserWidget.hpp"

namespace tinexus::files::ui {

class FilesWindow : public txui::Window {
private:
    txui::Ref<ColumnBrowserWidget> m_browser;

public:
    FilesWindow();
    ~FilesWindow() override = default;

    void open_directory(const std::filesystem::path& path);
};

} // namespace tinexus::files::ui
