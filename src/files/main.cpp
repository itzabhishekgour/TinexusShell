#include "files/column_view_model.hpp"
#include "files/trash_manager.hpp"
#include "files/file_operations.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("files");
    tinexus::log::info("Starting Tinexus Files (Finder-Style Reference Application)...");

    // Initialize SDK client for platform services (Search, Actions, Notifications)
    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Files: Connected to Tinexus Platform IPC broker via SDK.");
    }

    std::filesystem::path target_path = (argc > 1) ? std::filesystem::path(argv[1]) : std::filesystem::current_path();
    tinexus::files::ColumnViewModel column_model;
    column_model.initialize(target_path);

    std::cout << "--- Tinexus Files Column View Navigation ---\n";
    for (size_t col_idx = 0; col_idx < column_model.columns().size(); ++col_idx) {
        const auto& col = column_model.columns()[col_idx];
        std::cout << "Column [" << col_idx << "] Path: " << col.directory_path.string() << "\n";
        for (size_t item_idx = 0; item_idx < col.items.size(); ++item_idx) {
            const auto& item = col.items[item_idx];
            std::cout << "  (" << item_idx << ") " << item.name
                      << (item.type == tinexus::files::FileType::Directory ? "/" : "")
                      << " [" << item.mime_type << "]\n";
        }
    }

    sdk_client.disconnect();
    tinexus::log::info("Tinexus Files exiting cleanly.");
    return 0;
}
