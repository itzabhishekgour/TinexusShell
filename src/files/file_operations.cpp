#include "files/file_operations.hpp"
#include "files/trash_manager.hpp"
#include <fstream>
#include <cstdlib>
#include <system_error>
#include <thread>
#include <chrono>

namespace tinexus::files {

std::filesystem::path FileOperations::get_unique_destination_path(const std::filesystem::path& dest_dir, const std::string& filename) {
    std::filesystem::path candidate = dest_dir / filename;
    if (!std::filesystem::exists(candidate)) {
        return candidate;
    }

    std::filesystem::path file_p(filename);
    std::string stem = file_p.stem().string();
    std::string ext = file_p.extension().string();

    int count = 1;
    while (true) {
        std::string new_name = stem + " (" + std::to_string(count) + ")" + ext;
        candidate = dest_dir / new_name;
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
        count++;
    }
}

OperationResult FileOperations::create_folder(const std::filesystem::path& parent_dir, const std::string& base_name) {
    std::error_code ec;
    if (!std::filesystem::exists(parent_dir, ec) || !std::filesystem::is_directory(parent_dir, ec)) {
        return {false, "Parent directory does not exist or is not a directory.", {}};
    }

    std::filesystem::path target = get_unique_destination_path(parent_dir, base_name);
    if (!std::filesystem::create_directory(target, ec)) {
        return {false, ec.message(), {}};
    }

    return {true, "", target};
}

OperationResult FileOperations::create_file(const std::filesystem::path& parent_dir, const std::string& base_name) {
    std::error_code ec;
    if (!std::filesystem::exists(parent_dir, ec) || !std::filesystem::is_directory(parent_dir, ec)) {
        return {false, "Parent directory does not exist or is not a directory.", {}};
    }

    std::filesystem::path target = get_unique_destination_path(parent_dir, base_name);
    std::ofstream ofs(target);
    if (!ofs.is_open()) {
        return {false, "Failed to create file (permission denied or read-only filesystem).", {}};
    }
    ofs.close();

    return {true, "", target};
}

OperationResult FileOperations::rename_path(const std::filesystem::path& old_path, const std::string& new_name) {
    std::error_code ec;
    if (!std::filesystem::exists(old_path, ec)) {
        return {false, "Source item does not exist.", {}};
    }

    if (new_name.empty() || new_name.find('/') != std::string::npos) {
        return {false, "Invalid file name.", {}};
    }

    std::filesystem::path target = old_path.parent_path() / new_name;
    if (std::filesystem::exists(target, ec)) {
        return {false, "An item with that name already exists.", {}};
    }

    std::filesystem::rename(old_path, target, ec);
    if (ec) {
        return {false, ec.message(), {}};
    }

    return {true, "", target};
}

uint64_t FileOperations::compute_tree_size(const std::filesystem::path& path, std::stop_token stop_token) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return 0;
    if (!std::filesystem::is_directory(path, ec)) {
        return std::filesystem::file_size(path, ec);
    }

    uint64_t total = 0;
    for (std::filesystem::recursive_directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (stop_token.stop_requested()) break;
        if (it->is_regular_file(ec)) {
            total += it->file_size(ec);
        }
    }
    return total;
}

void FileOperations::compute_dir_stats(const std::filesystem::path& path, uint64_t& out_count, uint64_t& out_size, std::stop_token stop_token) {
    out_count = 0;
    out_size = 0;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) return;

    for (std::filesystem::recursive_directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (stop_token.stop_requested()) break;
        out_count++;
        if (it->is_regular_file(ec)) {
            out_size += it->file_size(ec);
        }
    }
}

OperationResult FileOperations::copy_path_streaming(
    const std::filesystem::path& src,
    const std::filesystem::path& dest_dir,
    std::stop_token stop_token,
    ProgressCallback on_progress,
    ConflictCallback on_conflict
) {
    std::error_code ec;
    if (!std::filesystem::exists(src, ec)) {
        return {false, "Source item does not exist.", {}};
    }
    if (!std::filesystem::is_directory(dest_dir, ec)) {
        return {false, "Destination is not a directory.", {}};
    }

    std::filesystem::path target = dest_dir / src.filename();
    const bool is_dir = std::filesystem::is_directory(src, ec);

    if (std::filesystem::exists(target, ec)) {
        const bool dest_is_dir = std::filesystem::is_directory(target, ec);

        // Directory-over-directory is transparent merge; individual files inside will trigger conflict if they collide.
        if (!(is_dir && dest_is_dir)) {
            if (on_conflict) {
                ConflictInfo info;
                info.source_path = src;
                info.dest_path = target;
                info.is_directory = is_dir;
                info.source_size = is_dir ? compute_tree_size(src, stop_token) : std::filesystem::file_size(src, ec);
                info.dest_size = dest_is_dir ? 0 : std::filesystem::file_size(target, ec);
                info.is_type_mismatch = (is_dir != dest_is_dir);

                auto s_time = std::filesystem::last_write_time(src, ec);
                info.source_mtime = std::chrono::duration_cast<std::chrono::seconds>(s_time.time_since_epoch()).count();
                auto d_time = std::filesystem::last_write_time(target, ec);
                info.dest_mtime = std::chrono::duration_cast<std::chrono::seconds>(d_time.time_since_epoch()).count();

                if (info.is_type_mismatch && dest_is_dir) {
                    compute_dir_stats(target, info.dest_dir_item_count, info.dest_dir_total_size, stop_token);
                }

                auto resolution = on_conflict(info, stop_token);
                if (stop_token.stop_requested() || resolution == ConflictResolution::Abort) {
                    return {false, "Operation cancelled by user.", {}};
                }
                if (resolution == ConflictResolution::Skip) {
                    return {true, "Skipped by user.", target};
                }
                if (resolution == ConflictResolution::KeepBoth) {
                    target = get_unique_destination_path(dest_dir, src.filename().string());
                } else if (resolution == ConflictResolution::Replace) {
                    if (dest_is_dir) {
                        std::filesystem::remove_all(target, ec);
                    }
                }
            } else {
                target = get_unique_destination_path(dest_dir, src.filename().string());
            }
        }
    }

    if (!is_dir) {
        uint64_t total_bytes = std::filesystem::file_size(src, ec);
        std::ifstream in(src, std::ios::binary);
        if (!in.is_open()) {
            return {false, "Cannot open source file for reading.", {}};
        }

        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return {false, "Cannot open destination file for writing.", {}};
        }

        const size_t CHUNK_SIZE = 256 * 1024; // 256 KB buffer
        std::vector<char> buffer(CHUNK_SIZE);
        uint64_t bytes_transferred = 0;
        auto start_time = std::chrono::steady_clock::now();

        // Optional chunk pacing for realistic disk simulation or test determinism
        const char* delay_env = std::getenv("TINEXUS_COPY_CHUNK_DELAY_MS");
        const int delay_ms = (delay_env && *delay_env) ? std::atoi(delay_env) : 0;

        while (in) {
            if (stop_token.stop_requested()) {
                in.close();
                out.close();
                std::filesystem::remove(target, ec); // Remove partial corrupted file
                return {false, "Operation cancelled by user.", {}};
            }

            if (delay_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }

            in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            std::streamsize read_bytes = in.gcount();
            if (read_bytes <= 0) break;

            out.write(buffer.data(), read_bytes);
            if (!out) {
                in.close();
                out.close();
                std::filesystem::remove(target, ec);
                return {false, "Write failure during copy.", {}};
            }

            bytes_transferred += static_cast<uint64_t>(read_bytes);
            if (on_progress) {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - start_time).count();
                double speed = (elapsed > 0.001) ? (double(bytes_transferred) / elapsed) : 0.0;
                double frac = (total_bytes > 0) ? (double(bytes_transferred) / double(total_bytes)) : 1.0;
                on_progress({bytes_transferred, total_bytes, frac, speed});
            }
        }

        in.close();
        out.close();

        // Preserve permissions
        auto perms = std::filesystem::status(src, ec).permissions();
        std::filesystem::permissions(target, perms, ec);

        return {true, "", target};
    } else {
        // Directory copy: compute tree size first for accurate progress
        uint64_t total_bytes = compute_tree_size(src, stop_token);
        if (stop_token.stop_requested()) {
            return {false, "Operation cancelled by user.", {}};
        }

        std::filesystem::create_directories(target, ec);
        if (ec) {
            return {false, "Failed to create target directory: " + ec.message(), {}};
        }

        uint64_t bytes_transferred = 0;
        auto start_time = std::chrono::steady_clock::now();
        const size_t CHUNK_SIZE = 256 * 1024;
        std::vector<char> buffer(CHUNK_SIZE);

        for (std::filesystem::recursive_directory_iterator it(src, std::filesystem::directory_options::skip_permission_denied, ec), end;
             it != end && !ec; it.increment(ec)) {
            if (stop_token.stop_requested()) {
                std::filesystem::remove_all(target, ec);
                return {false, "Operation cancelled by user.", {}};
            }

            auto rel = std::filesystem::relative(it->path(), src, ec);
            auto dest_item = target / rel;

            if (it->is_directory(ec)) {
                std::filesystem::create_directories(dest_item, ec);
            } else if (it->is_regular_file(ec)) {
                if (std::filesystem::exists(dest_item, ec)) {
                    if (on_conflict) {
                        ConflictInfo info;
                        info.source_path = it->path();
                        info.dest_path = dest_item;
                        info.is_directory = false;
                        info.source_size = it->file_size(ec);
                        info.dest_size = std::filesystem::file_size(dest_item, ec);
                        info.is_type_mismatch = std::filesystem::is_directory(dest_item, ec);

                        auto s_time = it->last_write_time(ec);
                        info.source_mtime = std::chrono::duration_cast<std::chrono::seconds>(s_time.time_since_epoch()).count();
                        auto d_time = std::filesystem::last_write_time(dest_item, ec);
                        info.dest_mtime = std::chrono::duration_cast<std::chrono::seconds>(d_time.time_since_epoch()).count();

                        auto resolution = on_conflict(info, stop_token);
                        if (stop_token.stop_requested() || resolution == ConflictResolution::Abort) {
                            std::filesystem::remove_all(target, ec);
                            return {false, "Operation cancelled by user.", {}};
                        }
                        if (resolution == ConflictResolution::Skip) {
                            continue;
                        }
                        if (resolution == ConflictResolution::KeepBoth) {
                            dest_item = get_unique_destination_path(dest_item.parent_path(), dest_item.filename().string());
                        }
                    }
                }

                std::ifstream in(it->path(), std::ios::binary);
                std::ofstream out(dest_item, std::ios::binary | std::ios::trunc);
                if (!in.is_open() || !out.is_open()) continue;

                while (in) {
                    if (stop_token.stop_requested()) {
                        in.close();
                        out.close();
                        std::filesystem::remove_all(target, ec);
                        return {false, "Operation cancelled by user.", {}};
                    }
                    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                    std::streamsize read_bytes = in.gcount();
                    if (read_bytes <= 0) break;
                    out.write(buffer.data(), read_bytes);
                    bytes_transferred += static_cast<uint64_t>(read_bytes);

                    if (on_progress) {
                        auto now = std::chrono::steady_clock::now();
                        double elapsed = std::chrono::duration<double>(now - start_time).count();
                        double speed = (elapsed > 0.001) ? (double(bytes_transferred) / elapsed) : 0.0;
                        double frac = (total_bytes > 0) ? (double(bytes_transferred) / double(total_bytes)) : 1.0;
                        on_progress({bytes_transferred, total_bytes, frac, speed});
                    }
                }
            }
        }

        return {true, "", target};
    }
}

OperationResult FileOperations::copy_path(const std::filesystem::path& src, const std::filesystem::path& dest_dir) {
    return copy_path_streaming(src, dest_dir);
}

OperationResult FileOperations::move_path(
    const std::filesystem::path& src,
    const std::filesystem::path& dest_dir,
    std::stop_token stop_token,
    ProgressCallback on_progress,
    ConflictCallback on_conflict
) {
    std::error_code ec;
    if (!std::filesystem::exists(src, ec)) {
        return {false, "Source item does not exist.", {}};
    }
    if (!std::filesystem::is_directory(dest_dir, ec)) {
        return {false, "Destination is not a directory.", {}};
    }

    std::filesystem::path target = dest_dir / src.filename();
    const bool is_dir = std::filesystem::is_directory(src, ec);

    if (std::filesystem::exists(target, ec)) {
        const bool dest_is_dir = std::filesystem::is_directory(target, ec);
        if (!(is_dir && dest_is_dir)) {
            if (on_conflict) {
                ConflictInfo info;
                info.source_path = src;
                info.dest_path = target;
                info.is_directory = is_dir;
                info.source_size = is_dir ? compute_tree_size(src, stop_token) : std::filesystem::file_size(src, ec);
                info.dest_size = dest_is_dir ? 0 : std::filesystem::file_size(target, ec);
                info.is_type_mismatch = (is_dir != dest_is_dir);

                auto s_time = std::filesystem::last_write_time(src, ec);
                info.source_mtime = std::chrono::duration_cast<std::chrono::seconds>(s_time.time_since_epoch()).count();
                auto d_time = std::filesystem::last_write_time(target, ec);
                info.dest_mtime = std::chrono::duration_cast<std::chrono::seconds>(d_time.time_since_epoch()).count();

                if (info.is_type_mismatch && dest_is_dir) {
                    compute_dir_stats(target, info.dest_dir_item_count, info.dest_dir_total_size, stop_token);
                }

                auto resolution = on_conflict(info, stop_token);
                if (stop_token.stop_requested() || resolution == ConflictResolution::Abort) {
                    return {false, "Operation cancelled by user.", {}};
                }
                if (resolution == ConflictResolution::Skip) {
                    return {true, "Skipped by user.", target};
                }
                if (resolution == ConflictResolution::KeepBoth) {
                    target = get_unique_destination_path(dest_dir, src.filename().string());
                } else if (resolution == ConflictResolution::Replace) {
                    std::filesystem::remove_all(target, ec);
                }
            } else {
                target = get_unique_destination_path(dest_dir, src.filename().string());
            }
        }
    }

    // Try rename first
    std::filesystem::rename(src, target, ec);
    if (ec) {
        // Across filesystem boundaries: copy then delete original
        ec.clear();
        auto copy_res = copy_path_streaming(src, dest_dir, stop_token, on_progress, on_conflict);
        if (!copy_res.success) {
            return copy_res;
        }
        std::filesystem::remove_all(src, ec);
        return copy_res;
    }

    return {true, "", target};
}

OperationResult FileOperations::trash_path(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return {false, "Target item does not exist.", {}};
    }

    // Strictly move to recoverable trash directory using Freedesktop Trash spec (.trashinfo)
    bool ok = TrashManager::instance().move_to_trash(path);
    if (!ok) {
        return {false, "Failed to move item to trash using Freedesktop specification.", {}};
    }

    std::filesystem::path target = TrashManager::instance().get_trash_dir() / "files" / path.filename();
    return {true, "", target};
}

} // namespace tinexus::files
