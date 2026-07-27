#ifndef TINEXUS_CLIPBOARD_CLIPBOARD_ITEM_HPP
#define TINEXUS_CLIPBOARD_CLIPBOARD_ITEM_HPP

#include <string>
#include <chrono>
#include <cstdint>

namespace tinexus::clipboard {

using ClipboardId = uint64_t;

enum class MimeCategory : uint8_t {
    Text = 0,
    Image = 1,
    File = 2,
    Code = 3,
    Url = 4
};

struct ClipboardItem {
    ClipboardId id{0};
    std::string mime_type{"text/plain"};
    MimeCategory category{MimeCategory::Text};
    std::string content;
    uint32_t copy_count{1};
    bool is_pinned{false};
    bool is_favorite{false};
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
};

} // namespace tinexus::clipboard

#endif // TINEXUS_CLIPBOARD_CLIPBOARD_ITEM_HPP
