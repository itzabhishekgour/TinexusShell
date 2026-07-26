#include <iostream>
#include <cassert>
#include <fstream>
#include "common/logger.hpp"
#include "files/file_model.hpp"
#include "files/column_view_model.hpp"
#include "files/trash_manager.hpp"
#include "files/file_operations.hpp"

void test_file_model_scanning() {
    auto current_dir = std::filesystem::current_path();
    auto items = tinexus::files::FileModel::scan_directory(current_dir);
    assert(!items.empty());

    for (const auto& item : items) {
        assert(!item.name.empty());
    }

    std::cout << "[PASS] test_file_model_scanning\n";
}

void test_column_view_model_navigation() {
    tinexus::files::ColumnViewModel model;
    auto current_dir = std::filesystem::current_path();
    model.initialize(current_dir);

    assert(model.columns().size() == 1);
    assert(!model.columns()[0].items.empty());

    // Select first directory if available
    for (size_t i = 0; i < model.columns()[0].items.size(); ++i) {
        if (model.columns()[0].items[i].type == tinexus::files::FileType::Directory) {
            assert(model.select_item(0, i));
            assert(model.columns().size() == 2);
            break;
        }
    }

    std::cout << "[PASS] test_column_view_model_navigation\n";
}

void test_freedesktop_trash_compliance() {
    auto temp_file = std::filesystem::current_path() / "test_trash_file.tmp";
    std::ofstream out(temp_file);
    out << "Testing Trash Spec Compliance";
    out.close();

    assert(std::filesystem::exists(temp_file));

    auto& trash = tinexus::files::TrashManager::instance();
    assert(trash.move_to_trash(temp_file));
    assert(!std::filesystem::exists(temp_file));

    assert(trash.restore_from_trash("test_trash_file.tmp"));
    assert(std::filesystem::exists(temp_file));

    std::filesystem::remove(temp_file);
    std::cout << "[PASS] test_freedesktop_trash_compliance\n";
}

void test_async_file_operations() {
    auto src = std::filesystem::current_path() / "test_ops_src.tmp";
    auto dest = std::filesystem::current_path() / "test_ops_dest.tmp";

    std::ofstream out(src);
    out << "Async ops test data";
    out.close();

    auto fut_copy = tinexus::files::FileOperations::copy_async(src, dest);
    assert(fut_copy.get() == true);
    assert(std::filesystem::exists(dest));

    auto fut_del = tinexus::files::FileOperations::delete_async(src);
    assert(fut_del.get() == true);
    assert(!std::filesystem::exists(src));

    std::filesystem::remove(dest);
    std::cout << "[PASS] test_async_file_operations\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_files");
    tinexus::log::info("Running Integration Test Suite for Tinexus Files...");

    test_file_model_scanning();
    test_column_view_model_navigation();
    test_freedesktop_trash_compliance();
    test_async_file_operations();

    tinexus::log::info("All Tinexus Files integration tests passed 100%!");
    return 0;
}
