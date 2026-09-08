#include "monitor/memory_parser.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>

namespace tinexus::monitor {

MemoryMetrics MemoryParser::parse_memory() {
    MemoryMetrics mem;
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        log::error("MemoryParser: Failed to open /proc/meminfo");
        return mem;
    }

    std::string key;
    uint64_t val;
    std::string unit;
    while (file >> key >> val >> unit) {
        if (key == "MemTotal:") {
            mem.total_ram_bytes = val * 1024;
        } else if (key == "MemAvailable:") {
            mem.available_ram_bytes = val * 1024;
        } else if (key == "MemFree:") {
            mem.free_ram_bytes = val * 1024;
        } else if (key == "Buffers:") {
            mem.buffers_bytes = val * 1024;
        } else if (key == "Cached:") {
            mem.cached_bytes = val * 1024;
        } else if (key == "SwapTotal:") {
            mem.total_swap_bytes = val * 1024;
        } else if (key == "SwapFree:") {
            mem.free_swap_bytes = val * 1024;
        }
    }

    if (mem.total_ram_bytes > mem.available_ram_bytes) {
        mem.used_ram_bytes = mem.total_ram_bytes - mem.available_ram_bytes;
    }
    if (mem.total_swap_bytes > mem.free_swap_bytes) {
        mem.used_swap_bytes = mem.total_swap_bytes - mem.free_swap_bytes;
    }

    return mem;
}

} // namespace tinexus::monitor
