#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <txui/graphics/Color.hpp>

namespace tinexus::files {

struct TagDefinition {
    std::string name;
    txui::Color color;
};

// Known limitation: tags are keyed by canonical path in ~/.config/tinexus/tags.json
// and do not survive external rename/move in Phase 1.
class TagManager {
public:
    static TagManager& instance();

    static const std::vector<TagDefinition>& available_tags();
    static txui::Color tag_color(const std::string& tag_name);

    [[nodiscard]] std::optional<std::string> get_tag(const std::filesystem::path& path) const;
    void set_tag(const std::filesystem::path& path, const std::string& tag_name);
    void remove_tag(const std::filesystem::path& path);

    void load();
    void save() const;

private:
    TagManager();
    std::filesystem::path config_file_path() const;

    std::unordered_map<std::string, std::string> m_path_to_tag; // canonical path -> tag name
};

} // namespace tinexus::files
