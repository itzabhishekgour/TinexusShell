#include "FilesWindow.hpp"
#include <txui/core/Logger.hpp>
#include <cstdlib>

namespace tinexus::files::ui {

FilesWindow::FilesWindow() : txui::Window("tinexus-files") {
    m_browser = std::make_shared<ColumnBrowserWidget>();
    
    m_browser->set_on_execute([](const std::filesystem::path& path) {
        txui::log::info("Executing: {}", path.string());
        // For MVP, just log it. Phase 1 LaunchAuthority is used for real execution.
    });

    set_title("Files");
    set_size(1000, 600);
    set_content_widget(m_browser);
}

void FilesWindow::open_directory(const std::filesystem::path& path) {
    m_browser->navigate_to(path);
}

} // namespace tinexus::files::ui
