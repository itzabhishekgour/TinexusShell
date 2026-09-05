#include "files/TagManager.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

namespace tinexus::files {

static const std::vector<TagDefinition> S_TAGS = {
    {"Red",    txui::Color(255,  69,  58, 255)},
    {"Orange", txui::Color(255, 159,  10, 255)},
    {"Yellow", txui::Color(255, 214,  10, 255)},
    {"Green",  txui::Color( 48, 209,  88, 255)},
    {"Blue",   txui::Color( 10, 132, 255, 255)},
    {"Purple", txui::Color(191,  90, 242, 255)},
    {"Gray",   txui::Color(142, 142, 147, 255)}
};

TagManager& TagManager::instance() {
    static TagManager s_instance;
    return s_instance;
}

TagManager::TagManager() {
    load();
}

const std::vector<TagDefinition>& TagManager::available_tags() {
    return S_TAGS;
}

txui::Color TagManager::tag_color(const std::string& tag_name) {
    for (const auto& tag : S_TAGS) {
        if (tag.name == tag_name) return tag.color;
    }
    return txui::Color(142, 142, 147, 255);
}

std::filesystem::path TagManager::config_file_path() const {
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base = (xdg_config && *xdg_config) ? std::filesystem::path(xdg_config)
                                                            : (std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "/home/tinexus") / ".config");
    return base / "tinexus" / "tags.json";
}

std::optional<std::string> TagManager::get_tag(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::canonical(path, ec);
    std::string key = (ec ? path.string() : canonical.string());

    auto it = m_path_to_tag.find(key);
    if (it != m_path_to_tag.end()) {
        return it->second;
    }
    return std::nullopt;
}

void TagManager::set_tag(const std::filesystem::path& path, const std::string& tag_name) {
    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::canonical(path, ec);
    std::string key = (ec ? path.string() : canonical.string());

    m_path_to_tag[key] = tag_name;
    save();
}

void TagManager::remove_tag(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::canonical(path, ec);
    std::string key = (ec ? path.string() : canonical.string());

    m_path_to_tag.erase(key);
    save();
}

void TagManager::load() {
    m_path_to_tag.clear();
    auto conf = config_file_path();
    std::error_code ec;
    if (!std::filesystem::exists(conf, ec)) {
        return;
    }

    std::ifstream in(conf);
    if (!in.is_open()) return;

    std::string line;
    // Simple line-by-line JSON parser for {"path": "tag"}
    while (std::getline(in, line)) {
        size_t first_q = line.find('\"');
        if (first_q == std::string::npos) continue;
        size_t second_q = line.find('\"', first_q + 1);
        if (second_q == std::string::npos) continue;

        size_t colon = line.find(':', second_q + 1);
        if (colon == std::string::npos) continue;

        size_t third_q = line.find('\"', colon + 1);
        if (third_q == std::string::npos) continue;
        size_t fourth_q = line.find('\"', third_q + 1);
        if (fourth_q == std::string::npos) continue;

        std::string p = line.substr(first_q + 1, second_q - first_q - 1);
        std::string t = line.substr(third_q + 1, fourth_q - third_q - 1);

        if (!p.empty() && !t.empty()) {
            m_path_to_tag[p] = t;
        }
    }
}

void TagManager::save() const {
    auto conf = config_file_path();
    std::error_code ec;
    std::filesystem::create_directories(conf.parent_path(), ec);

    std::filesystem::path tmp = conf.string() + ".tmp";
    std::ofstream out(tmp);
    if (!out.is_open()) return;

    out << "{\n";
    size_t i = 0;
    for (const auto& [path, tag] : m_path_to_tag) {
        out << "  \"" << path << "\": \"" << tag << "\"" << (i + 1 < m_path_to_tag.size() ? "," : "") << "\n";
        i++;
    }
    out << "}\n";
    out.flush();
    out.close();

    // Atomic rename
    std::filesystem::rename(tmp, conf, ec);
}

} // namespace tinexus::files
