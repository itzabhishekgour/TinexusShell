#include "comp/window/decoration_manager.hpp"
#include "common/logger.hpp"
#include "common/DBusNames.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>

extern "C" {
#include <wlr/util/edges.h>
#include <wlr/render/pass.h>
#define class c_class
#include <wlr/xwayland/xwayland.h>
#undef class
}

namespace tinexus::comp {

namespace {

static bool parse_hex_color(std::string_view hex_str, float out_rgba[4]) {
    while (!hex_str.empty() && (hex_str.front() == ' ' || hex_str.front() == '"' || hex_str.front() == '\'')) {
        hex_str.remove_prefix(1);
    }
    while (!hex_str.empty() && (hex_str.back() == ' ' || hex_str.back() == '"' || hex_str.back() == '\'')) {
        hex_str.remove_suffix(1);
    }
    if (hex_str.empty()) return false;
    if (hex_str.front() == '#') hex_str.remove_prefix(1);
    else if (hex_str.starts_with("0x") || hex_str.starts_with("0X")) hex_str.remove_prefix(2);

    if (hex_str.length() != 6 && hex_str.length() != 8) return false;

    try {
        uint32_t val = static_cast<uint32_t>(std::stoul(std::string(hex_str), nullptr, 16));
        if (hex_str.length() == 6) {
            out_rgba[0] = ((val >> 16) & 0xFF) / 255.0f;
            out_rgba[1] = ((val >> 8) & 0xFF) / 255.0f;
            out_rgba[2] = (val & 0xFF) / 255.0f;
            out_rgba[3] = 1.0f;
        } else {
            out_rgba[0] = ((val >> 24) & 0xFF) / 255.0f;
            out_rgba[1] = ((val >> 16) & 0xFF) / 255.0f;
            out_rgba[2] = ((val >> 8) & 0xFF) / 255.0f;
            out_rgba[3] = (val & 0xFF) / 255.0f;
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // anonymous namespace

DecorationTheme DecorationTheme::load_from_config(const std::string& path) {
    DecorationTheme th;
    std::string config_file = path;
    if (config_file.empty()) {
        const char* home = getenv("HOME");
        if (home && *home) {
            std::string user_cfg = std::string(home) + "/.config/tinexus/theme.toml";
            if (std::filesystem::exists(user_cfg)) {
                config_file = user_cfg;
            }
        }
        if (config_file.empty()) {
            std::string sys_cfg = "/etc/tinexus/defaults/theme.toml";
            if (std::filesystem::exists(sys_cfg)) {
                config_file = sys_cfg;
            }
        }
    }

    if (config_file.empty() || !std::filesystem::exists(config_file)) {
        return th;
    }

    std::ifstream f(config_file);
    if (!f.is_open()) return th;

    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());

        if (key == "title_bg_active") parse_hex_color(val, th.title_bg_active);
        else if (key == "title_bg_inactive") parse_hex_color(val, th.title_bg_inactive);
        else if (key == "divider_active") parse_hex_color(val, th.divider_active);
        else if (key == "divider_inactive") parse_hex_color(val, th.divider_inactive);
        else if (key == "border_active") parse_hex_color(val, th.border_active);
        else if (key == "border_inactive") parse_hex_color(val, th.border_inactive);
        else if (key == "close_fill") parse_hex_color(val, th.close_fill);
        else if (key == "close_border") parse_hex_color(val, th.close_border);
        else if (key == "min_fill") parse_hex_color(val, th.min_fill);
        else if (key == "min_border") parse_hex_color(val, th.min_border);
        else if (key == "max_fill") parse_hex_color(val, th.max_fill);
        else if (key == "max_border") parse_hex_color(val, th.max_border);
        else if (key == "inactive_fill") parse_hex_color(val, th.inactive_fill);
        else if (key == "inactive_border") parse_hex_color(val, th.inactive_border);
    }
    return th;
}

namespace {

bool reject_shadow_input(struct wlr_scene_buffer* /*buffer*/, double* /*sx*/, double* /*sy*/) {
    return false;
}

struct wlr_scene_buffer* make_shadow_slice(struct wlr_scene_tree* parent, struct wlr_buffer* initial_buf) {
    if (!parent || !initial_buf) {
        return nullptr;
    }
    struct wlr_scene_buffer* sb = wlr_scene_buffer_create(parent, initial_buf);
    if (sb) {
        sb->point_accepts_input = reject_shadow_input;
        wlr_scene_buffer_set_filter_mode(sb, WLR_SCALE_FILTER_BILINEAR);
    }
    return sb;
}

struct wlr_scene_buffer* make_decor_sprite(struct wlr_scene_tree* parent, struct wlr_buffer* initial_buf) {
    if (!parent || !initial_buf) {
        return nullptr;
    }
    struct wlr_scene_buffer* sb = wlr_scene_buffer_create(parent, initial_buf);
    if (sb) {
        wlr_scene_buffer_set_filter_mode(sb, WLR_SCALE_FILTER_BILINEAR);
    }
    return sb;
}

} // namespace

// ---------------------------------------------------------------------------
// TinexusWindowFrame Member Functions
// ---------------------------------------------------------------------------

void TinexusWindowFrame::update_geometry(int32_t w, int32_t h) {
    client_width = std::max(100, w);
    client_height = std::max(100, h);

    const int32_t total_w = client_width + 2;
    const int32_t total_h = client_height + DecorationAssetCache::kTitlebarHeight + 2;
    constexpr int32_t R = DecorationAssetCache::kCornerRadius;
    constexpr int32_t M_X = DecorationAssetCache::kShadowSide;
    constexpr int32_t M_T = DecorationAssetCache::kShadowTop;
    constexpr int32_t M_B = DecorationAssetCache::kShadowBottom;
    constexpr int32_t IN  = DecorationAssetCache::kShadowInset;
    constexpr int32_t E_D = DecorationAssetCache::kShadowEdgeDepth;

    const int32_t top_inner_w = std::max(0, total_w - 2 * R);
    if (corner_tl) {
        wlr_scene_node_set_position(&corner_tl->node, 0, 0);
        wlr_scene_buffer_set_dest_size(corner_tl, R, R);
    }
    if (corner_tr) {
        wlr_scene_node_set_position(&corner_tr->node, total_w - R, 0);
        wlr_scene_buffer_set_dest_size(corner_tr, R, R);
    }
    if (border_top) {
        wlr_scene_rect_set_size(border_top, top_inner_w, 1);
        wlr_scene_node_set_position(&border_top->node, R, 0);
    }
    if (titlebar_top_bg) {
        wlr_scene_rect_set_size(titlebar_top_bg, top_inner_w, R - 1);
        wlr_scene_node_set_position(&titlebar_top_bg->node, R, 1);
    }
    if (titlebar_bg) {
        wlr_scene_rect_set_size(titlebar_bg, total_w - 2, DecorationAssetCache::kTitlebarHeight - R);
        wlr_scene_node_set_position(&titlebar_bg->node, 1, R);
    }
    if (titlebar_divider) {
        wlr_scene_rect_set_size(titlebar_divider, total_w - 2, 1);
        wlr_scene_node_set_position(&titlebar_divider->node, 1, 28);
    }
    if (border_bottom) {
        wlr_scene_rect_set_size(border_bottom, total_w, 1);
        wlr_scene_node_set_position(&border_bottom->node, 0, total_h - 1);
    }
    if (border_left) {
        wlr_scene_rect_set_size(border_left, 1, total_h - R);
        wlr_scene_node_set_position(&border_left->node, 0, R);
    }
    if (border_right) {
        wlr_scene_rect_set_size(border_right, 1, total_h - R);
        wlr_scene_node_set_position(&border_right->node, total_w - 1, R);
    }

    const int32_t span_x = std::max(1, total_w - 2 * IN);
    const int32_t span_y = std::max(1, total_h - 2 * IN);

    if (shadow_tl) {
        wlr_scene_node_set_position(&shadow_tl->node, -M_X, -M_T);
        wlr_scene_buffer_set_dest_size(shadow_tl, M_X + IN, M_T + IN);
    }
    if (shadow_t) {
        wlr_scene_node_set_position(&shadow_t->node, IN, -M_T);
        wlr_scene_buffer_set_dest_size(shadow_t, span_x, M_T + E_D);
    }
    if (shadow_tr) {
        wlr_scene_node_set_position(&shadow_tr->node, total_w - IN, -M_T);
        wlr_scene_buffer_set_dest_size(shadow_tr, M_X + IN, M_T + IN);
    }
    if (shadow_l) {
        wlr_scene_node_set_position(&shadow_l->node, -M_X, IN);
        wlr_scene_buffer_set_dest_size(shadow_l, M_X + E_D, span_y);
    }
    if (shadow_r) {
        wlr_scene_node_set_position(&shadow_r->node, total_w - E_D, IN);
        wlr_scene_buffer_set_dest_size(shadow_r, M_X + E_D, span_y);
    }
    if (shadow_bl) {
        wlr_scene_node_set_position(&shadow_bl->node, -M_X, total_h - IN);
        wlr_scene_buffer_set_dest_size(shadow_bl, M_X + IN, M_B + IN);
    }
    if (shadow_b) {
        wlr_scene_node_set_position(&shadow_b->node, IN, total_h - E_D);
        wlr_scene_buffer_set_dest_size(shadow_b, span_x, M_B + E_D);
    }
    if (shadow_br) {
        wlr_scene_node_set_position(&shadow_br->node, total_w - IN, total_h - IN);
        wlr_scene_buffer_set_dest_size(shadow_br, M_X + IN, M_B + IN);
    }
}

void TinexusWindowFrame::refresh_visual_buffers() {
    const auto& assets = TinexusDecorationManager::instance().assets();
    if (!assets.is_ready()) return;

    const auto& sh = assets.shadow(is_active);
    if (shadow_tl) wlr_scene_buffer_set_buffer(shadow_tl, sh.tl);
    if (shadow_t)  wlr_scene_buffer_set_buffer(shadow_t, sh.t);
    if (shadow_tr) wlr_scene_buffer_set_buffer(shadow_tr, sh.tr);
    if (shadow_l)  wlr_scene_buffer_set_buffer(shadow_l, sh.l);
    if (shadow_r)  wlr_scene_buffer_set_buffer(shadow_r, sh.r);
    if (shadow_bl) wlr_scene_buffer_set_buffer(shadow_bl, sh.bl);
    if (shadow_b)  wlr_scene_buffer_set_buffer(shadow_b, sh.b);
    if (shadow_br) wlr_scene_buffer_set_buffer(shadow_br, sh.br);

    if (corner_tl) wlr_scene_buffer_set_buffer(corner_tl, assets.corner_tl(is_active));
    if (corner_tr) wlr_scene_buffer_set_buffer(corner_tr, assets.corner_tr(is_active));

    using BK = DecorationAssetCache::ButtonKind;
    if (btn_close) wlr_scene_buffer_set_buffer(btn_close, assets.traffic_button(BK::Close, is_active, traffic_hovered));
    if (btn_min)   wlr_scene_buffer_set_buffer(btn_min, assets.traffic_button(BK::Minimize, is_active, traffic_hovered));
    if (btn_max)   wlr_scene_buffer_set_buffer(btn_max, assets.traffic_button(BK::Maximize, is_active, traffic_hovered));

    update_geometry(client_width, client_height);
}

void TinexusWindowFrame::set_active(bool active) {
    is_active = active;
    const auto& th = TinexusDecorationManager::instance().theme();

    const float* title_col = active ? th.title_bg_active : th.title_bg_inactive;
    const float* border_col = active ? th.border_active : th.border_inactive;

    if (titlebar_top_bg) {
        wlr_scene_rect_set_color(titlebar_top_bg, title_col);
    }
    if (titlebar_bg) {
        wlr_scene_rect_set_color(titlebar_bg, title_col);
    }
    if (titlebar_divider) {
        wlr_scene_rect_set_color(titlebar_divider, active ? th.divider_active : th.divider_inactive);
    }
    if (border_top) {
        wlr_scene_rect_set_color(border_top, border_col);
    }
    if (border_bottom) {
        wlr_scene_rect_set_color(border_bottom, border_col);
    }
    if (border_left) {
        wlr_scene_rect_set_color(border_left, border_col);
    }
    if (border_right) {
        wlr_scene_rect_set_color(border_right, border_col);
    }

    refresh_visual_buffers();
}

void TinexusWindowFrame::set_traffic_hovered(bool hovered) {
    if (traffic_hovered == hovered) return;
    traffic_hovered = hovered;
    const auto& assets = TinexusDecorationManager::instance().assets();
    if (!assets.is_ready()) return;

    using BK = DecorationAssetCache::ButtonKind;
    if (btn_close) wlr_scene_buffer_set_buffer(btn_close, assets.traffic_button(BK::Close, is_active, traffic_hovered));
    if (btn_min)   wlr_scene_buffer_set_buffer(btn_min, assets.traffic_button(BK::Minimize, is_active, traffic_hovered));
    if (btn_max)   wlr_scene_buffer_set_buffer(btn_max, assets.traffic_button(BK::Maximize, is_active, traffic_hovered));
}

void TinexusWindowFrame::set_fullscreen(bool fullscreen) {
    is_fullscreen = fullscreen;

    const bool show_decor = !fullscreen;
    const bool show_shadow = (!fullscreen && !is_maximized);

    if (shadow_tree) wlr_scene_node_set_enabled(&shadow_tree->node, show_shadow);
    if (corner_tl) wlr_scene_node_set_enabled(&corner_tl->node, show_decor);
    if (corner_tr) wlr_scene_node_set_enabled(&corner_tr->node, show_decor);
    if (titlebar_top_bg) wlr_scene_node_set_enabled(&titlebar_top_bg->node, show_decor);
    if (titlebar_bg) wlr_scene_node_set_enabled(&titlebar_bg->node, show_decor);
    if (titlebar_divider) wlr_scene_node_set_enabled(&titlebar_divider->node, show_decor);
    if (traffic_tree) wlr_scene_node_set_enabled(&traffic_tree->node, show_decor);
    if (border_top) wlr_scene_node_set_enabled(&border_top->node, show_decor);
    if (border_bottom) wlr_scene_node_set_enabled(&border_bottom->node, show_decor);
    if (border_left) wlr_scene_node_set_enabled(&border_left->node, show_decor);
    if (border_right) wlr_scene_node_set_enabled(&border_right->node, show_decor);

    if (client_tree) {
        wlr_scene_node_set_position(&client_tree->node, fullscreen ? 0 : 1, fullscreen ? 0 : 29);
    }
}

void TinexusWindowFrame::set_maximized(bool maximized) {
    is_maximized = maximized;
    if (shadow_tree) {
        wlr_scene_node_set_enabled(&shadow_tree->node, !is_fullscreen && !is_maximized);
    }
}

void TinexusWindowFrame::set_visual_transform(double scale, float opacity) {
    const float op = std::clamp(opacity, 0.0f, 1.0f);
    if (scale >= 0.9999 && op >= 0.999f) {
        if (corner_tl) wlr_scene_buffer_set_opacity(corner_tl, 1.0f);
        if (corner_tr) wlr_scene_buffer_set_opacity(corner_tr, 1.0f);
        if (btn_close) wlr_scene_buffer_set_opacity(btn_close, 1.0f);
        if (btn_min)   wlr_scene_buffer_set_opacity(btn_min, 1.0f);
        if (btn_max)   wlr_scene_buffer_set_opacity(btn_max, 1.0f);
        if (shadow_tl) wlr_scene_buffer_set_opacity(shadow_tl, 1.0f);
        if (shadow_t)  wlr_scene_buffer_set_opacity(shadow_t, 1.0f);
        if (shadow_tr) wlr_scene_buffer_set_opacity(shadow_tr, 1.0f);
        if (shadow_l)  wlr_scene_buffer_set_opacity(shadow_l, 1.0f);
        if (shadow_r)  wlr_scene_buffer_set_opacity(shadow_r, 1.0f);
        if (shadow_bl) wlr_scene_buffer_set_opacity(shadow_bl, 1.0f);
        if (shadow_b)  wlr_scene_buffer_set_opacity(shadow_b, 1.0f);
        if (shadow_br) wlr_scene_buffer_set_opacity(shadow_br, 1.0f);
        set_active(is_active);
        set_maximized(is_maximized);
        set_fullscreen(is_fullscreen);
        return;
    }

    const double s = std::clamp(scale, 0.05, 1.05);
    const auto& th = TinexusDecorationManager::instance().theme();
    const float* raw_title = is_active ? th.title_bg_active : th.title_bg_inactive;
    const float* raw_div   = is_active ? th.divider_active : th.divider_inactive;
    const float* raw_bdr   = is_active ? th.border_active : th.border_inactive;
    const float title_col[4]{raw_title[0] * op, raw_title[1] * op, raw_title[2] * op, raw_title[3] * op};
    const float div_col[4]{raw_div[0] * op, raw_div[1] * op, raw_div[2] * op, raw_div[3] * op};
    const float bdr_col[4]{raw_bdr[0] * op, raw_bdr[1] * op, raw_bdr[2] * op, raw_bdr[3] * op};

    if (shadow_tree) {
        wlr_scene_node_set_enabled(&shadow_tree->node, !is_fullscreen && !is_maximized && s >= 0.88);
    }
    if (traffic_tree) {
        wlr_scene_node_set_enabled(&traffic_tree->node, !is_fullscreen && s >= 0.70);
    }

    const int32_t sw = std::max(1, static_cast<int32_t>(std::lround(client_width * s)));
    const int32_t sh = std::max(1, static_cast<int32_t>(std::lround(client_height * s)));
    const int32_t tb = is_fullscreen ? 0 : std::max(1, static_cast<int32_t>(std::lround(DecorationAssetCache::kTitlebarHeight * s)));
    const int32_t total_w = sw + (is_fullscreen ? 0 : 2);
    const int32_t total_h = sh + tb + (is_fullscreen ? 0 : 2);
    const int32_t R = std::max(1, std::min(tb, static_cast<int32_t>(std::lround(DecorationAssetCache::kCornerRadius * s))));
    const int32_t top_inner_w = std::max(1, total_w - 2 * R);

    if (corner_tl) {
        wlr_scene_node_set_position(&corner_tl->node, 0, 0);
        wlr_scene_buffer_set_dest_size(corner_tl, R, R);
        wlr_scene_buffer_set_opacity(corner_tl, op);
    }
    if (corner_tr) {
        wlr_scene_node_set_position(&corner_tr->node, std::max(0, total_w - R), 0);
        wlr_scene_buffer_set_dest_size(corner_tr, R, R);
        wlr_scene_buffer_set_opacity(corner_tr, op);
    }
    if (border_top) {
        wlr_scene_rect_set_size(border_top, top_inner_w, 1);
        wlr_scene_node_set_position(&border_top->node, R, 0);
        wlr_scene_rect_set_color(border_top, bdr_col);
    }
    if (titlebar_top_bg) {
        wlr_scene_rect_set_size(titlebar_top_bg, top_inner_w, std::max(1, R - 1));
        wlr_scene_node_set_position(&titlebar_top_bg->node, R, 1);
        wlr_scene_rect_set_color(titlebar_top_bg, title_col);
    }
    if (titlebar_bg) {
        wlr_scene_rect_set_size(titlebar_bg, std::max(1, total_w - 2), std::max(1, tb - R));
        wlr_scene_node_set_position(&titlebar_bg->node, 1, R);
        wlr_scene_rect_set_color(titlebar_bg, title_col);
    }
    if (titlebar_divider) {
        wlr_scene_rect_set_size(titlebar_divider, std::max(1, total_w - 2), 1);
        wlr_scene_node_set_position(&titlebar_divider->node, 1, tb);
        wlr_scene_rect_set_color(titlebar_divider, div_col);
    }
    if (border_bottom) {
        wlr_scene_rect_set_size(border_bottom, total_w, 1);
        wlr_scene_node_set_position(&border_bottom->node, 0, std::max(0, total_h - 1));
        wlr_scene_rect_set_color(border_bottom, bdr_col);
    }
    if (border_left) {
        wlr_scene_rect_set_size(border_left, 1, std::max(1, total_h - R));
        wlr_scene_node_set_position(&border_left->node, 0, R);
        wlr_scene_rect_set_color(border_left, bdr_col);
    }
    if (border_right) {
        wlr_scene_rect_set_size(border_right, 1, std::max(1, total_h - R));
        wlr_scene_node_set_position(&border_right->node, std::max(0, total_w - 1), R);
        wlr_scene_rect_set_color(border_right, bdr_col);
    }

    if (client_tree) {
        wlr_scene_node_set_position(&client_tree->node, is_fullscreen ? 0 : 1, is_fullscreen ? 0 : (1 + tb));
    }
}

HitTarget TinexusWindowFrame::hit_test(double local_x, double local_y) const {
    if (is_fullscreen) {
        return HitTarget::None;
    }

    int32_t total_w = client_width + 2;
    int32_t total_h = client_height + 28 + 2;

    if (local_x < 0.0 || local_x >= total_w || local_y < 0.0 || local_y >= total_h) {
        return HitTarget::None;
    }

    // Outer border resize zones (3px tolerance)
    if (local_y <= 2.0) return HitTarget::BorderTop;
    if (local_y >= total_h - 3.0) return HitTarget::BorderBottom;
    if (local_x <= 2.0) return HitTarget::BorderLeft;
    if (local_x >= total_w - 3.0) return HitTarget::BorderRight;

    // Titlebar zone (y in [1..28])
    if (local_y >= 1.0 && local_y <= 28.0) {
        // Traffic lights click targets (matching 14px left margin and 20px circle centers)
        if (local_y >= 4.0 && local_y <= 24.0) {
            if (local_x >= 10.0 && local_x <= 28.0) return HitTarget::CloseButton;
            if (local_x >= 28.0 && local_x <= 48.0) return HitTarget::MinimizeButton;
            if (local_x >= 48.0 && local_x <= 68.0) return HitTarget::MaximizeButton;
        }
        return HitTarget::TitlebarDrag;
    }

    return HitTarget::None;
}

// ---------------------------------------------------------------------------
// TinexusDecorationManager Implementation
// ---------------------------------------------------------------------------

TinexusDecorationManager& TinexusDecorationManager::instance() noexcept {
    static TinexusDecorationManager s_instance;
    return s_instance;
}

bool TinexusDecorationManager::init(struct wl_display* display, IWindowActionHandler* action_handler,
                                   struct wlr_scene_tree* scene_tree_normal) {
    if (!display || !action_handler || !scene_tree_normal) {
        log::error("[Decoration] Invalid arguments provided to TinexusDecorationManager::init");
        return false;
    }

    m_display = display;
    m_action_handler = action_handler;
    m_scene_tree_normal = scene_tree_normal;

    m_manager_v1 = wlr_xdg_decoration_manager_v1_create(m_display);
    if (!m_manager_v1) {
        log::error("[Decoration] Failed to create wlr_xdg_decoration_manager_v1 global");
        return false;
    }

    m_new_decoration_listener.notify = handle_new_toplevel_decoration;
    wl_signal_add(&m_manager_v1->events.new_toplevel_decoration, &m_new_decoration_listener);

    load_theme();
    if (!m_assets.is_ready()) {
        m_assets.rebuild(m_theme);
    }

    log::info("[Decoration] TinexusDecorationManager initialized successfully with HiDPI assets & zxdg_decoration_manager_v1");
    return true;
}

void TinexusDecorationManager::set_theme(const DecorationTheme& theme) {
    m_theme = theme;
    m_assets.rebuild(m_theme);
    for (auto& [key, frame] : m_frames) {
        if (frame) {
            frame->set_active(frame->is_active);
        }
    }
}

void TinexusDecorationManager::load_theme(const std::string& path) {
    set_theme(DecorationTheme::load_from_config(path));
}

void TinexusDecorationManager::shutdown() {
    if (m_new_decoration_listener.link.next) {
        wl_list_remove(&m_new_decoration_listener.link);
        m_new_decoration_listener.link.next = nullptr;
    }
    for (auto& ctx : m_contexts) {
        if (!ctx) continue;
        if (ctx->request_mode.link.next) {
            wl_list_remove(&ctx->request_mode.link);
            ctx->request_mode.link.next = nullptr;
        }
        if (ctx->surface_commit.link.next) {
            wl_list_remove(&ctx->surface_commit.link);
            ctx->surface_commit.link.next = nullptr;
        }
        if (ctx->destroy.link.next) {
            wl_list_remove(&ctx->destroy.link);
            ctx->destroy.link.next = nullptr;
        }
    }
    m_hovered_frame = nullptr;
    m_frames.clear();
    m_contexts.clear();
    m_assets.release();
    m_manager_v1 = nullptr;
    m_display = nullptr;
    m_action_handler = nullptr;
    m_scene_tree_normal = nullptr;
    log::info("[Decoration] TinexusDecorationManager shut down cleanly");
}

bool TinexusDecorationManager::is_native_csd_app(const char* app_id) noexcept {
    if (!app_id || *app_id == '\0') {
        return false;
    }
    std::string_view id(app_id);

    // 1. Native Tinexus shell and UI components (Qt6 / txui client-side decoration)
    if (id.starts_with("tinexus-") ||
        id.starts_with(tinexus::common::dbus::app_id::Prefix) ||
        id.starts_with("txui-") ||
        id == "lock" ||
        id == "launcher") {
        return true;
    }

    // 2. Known external apps that reliably self-decorate (CSD) under Wayland
    if (id == "firefox" ||
        id == "org.mozilla.firefox" ||
        id == "chromium" ||
        id == "chromium-browser" ||
        id == "google-chrome") {
        return true;
    }

    return false;
}

bool TinexusDecorationManager::client_wants_csd(struct wlr_xdg_toplevel* toplevel) const {
    if (!toplevel) return false;
    for (const auto& ctx : m_contexts) {
        if (ctx && ctx->decoration && ctx->decoration->toplevel == toplevel) {
            return ctx->decoration->current.mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE ||
                   ctx->decoration->scheduled_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE ||
                   ctx->decoration->requested_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE ||
                   (ctx->has_pending_mode && ctx->pending_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
        }
    }
    return false;
}

bool TinexusDecorationManager::client_wants_ssd(struct wlr_xdg_toplevel* toplevel) const {
    if (!toplevel) return false;
    for (const auto& ctx : m_contexts) {
        if (ctx && ctx->decoration && ctx->decoration->toplevel == toplevel) {
            return ctx->decoration->current.mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE ||
                   ctx->decoration->scheduled_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE ||
                   ctx->decoration->requested_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE ||
                   (ctx->has_pending_mode && ctx->pending_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        }
    }
    return false;
}

void TinexusDecorationManager::apply_pending_decoration_mode(struct wlr_xdg_toplevel* toplevel) {
    if (!toplevel || !toplevel->base || !toplevel->base->initialized) return;
    for (const auto& ctx : m_contexts) {
        if (ctx && ctx->decoration && ctx->decoration->toplevel == toplevel && ctx->has_pending_mode) {
            apply_decoration_context(ctx.get());
        }
    }
}

TinexusWindowFrame* TinexusDecorationManager::find_frame_from_node(struct wlr_scene_node* node) const {
    if (!node) return nullptr;
    for (struct wlr_scene_node* cur = node; cur != nullptr; cur = (cur->parent ? &cur->parent->node : nullptr)) {
        for (const auto& [key, f] : m_frames) {
            if (f && f->frame_tree && &f->frame_tree->node == cur) {
                return f.get();
            }
        }
    }
    return nullptr;
}

TinexusWindowFrame* TinexusDecorationManager::create_frame_impl(void* window_key, struct wlr_scene_tree* parent, const char* label) {
    struct wlr_scene_tree* root_tree = parent ? parent : m_scene_tree_normal;
    if (!window_key || !root_tree) return nullptr;

    if (!m_assets.is_ready()) {
        m_assets.rebuild(m_theme);
    }
    const auto& init_sh = m_assets.shadow(true);

    auto frame = std::make_unique<TinexusWindowFrame>();

    // 1. Root frame tree under parent workspace scene tree (or normal scene tree)
    frame->frame_tree = wlr_scene_tree_create(root_tree);
    if (!frame->frame_tree) return nullptr;

    // 2. Dual-layer soft shadow 9-slice tree rendered behind titlebar and client
    frame->shadow_tree = wlr_scene_tree_create(frame->frame_tree);
    if (frame->shadow_tree) {
        frame->shadow_tl = make_shadow_slice(frame->shadow_tree, init_sh.tl);
        frame->shadow_t  = make_shadow_slice(frame->shadow_tree, init_sh.t);
        frame->shadow_tr = make_shadow_slice(frame->shadow_tree, init_sh.tr);
        frame->shadow_l  = make_shadow_slice(frame->shadow_tree, init_sh.l);
        frame->shadow_r  = make_shadow_slice(frame->shadow_tree, init_sh.r);
        frame->shadow_bl = make_shadow_slice(frame->shadow_tree, init_sh.bl);
        frame->shadow_b  = make_shadow_slice(frame->shadow_tree, init_sh.b);
        frame->shadow_br = make_shadow_slice(frame->shadow_tree, init_sh.br);
    }

    // 3. Antialiased rounded top corners + crisp 28px titlebar geometry
    constexpr int32_t R = DecorationAssetCache::kCornerRadius;
    frame->titlebar_top_bg = wlr_scene_rect_create(frame->frame_tree, 802 - 2 * R, R - 1, m_theme.title_bg_active);
    wlr_scene_node_set_position(&frame->titlebar_top_bg->node, R, 1);

    frame->titlebar_bg = wlr_scene_rect_create(frame->frame_tree, 800, DecorationAssetCache::kTitlebarHeight - R, m_theme.title_bg_active);
    wlr_scene_node_set_position(&frame->titlebar_bg->node, 1, R);

    frame->titlebar_divider = wlr_scene_rect_create(frame->frame_tree, 800, 1, m_theme.divider_active);
    wlr_scene_node_set_position(&frame->titlebar_divider->node, 1, 28);

    // 4. Crisp 1px outer borders joining seamlessly into rounded top corner arcs
    frame->border_top = wlr_scene_rect_create(frame->frame_tree, 802 - 2 * R, 1, m_theme.border_active);
    wlr_scene_node_set_position(&frame->border_top->node, R, 0);

    frame->border_bottom = wlr_scene_rect_create(frame->frame_tree, 802, 1, m_theme.border_active);
    wlr_scene_node_set_position(&frame->border_bottom->node, 0, 629);

    frame->border_left = wlr_scene_rect_create(frame->frame_tree, 1, 630 - R, m_theme.border_active);
    wlr_scene_node_set_position(&frame->border_left->node, 0, R);

    frame->border_right = wlr_scene_rect_create(frame->frame_tree, 1, 630 - R, m_theme.border_active);
    wlr_scene_node_set_position(&frame->border_right->node, 801, R);

    frame->corner_tl = make_decor_sprite(frame->frame_tree, m_assets.corner_tl(true));
    frame->corner_tr = make_decor_sprite(frame->frame_tree, m_assets.corner_tr(true));

    // 5. HiDPI antialiased traffic light buttons (14px left margin, 8px top)
    frame->traffic_tree = wlr_scene_tree_create(frame->frame_tree);
    wlr_scene_node_set_position(&frame->traffic_tree->node, 14, 8);

    using BK = DecorationAssetCache::ButtonKind;
    frame->btn_close = make_decor_sprite(frame->traffic_tree, m_assets.traffic_button(BK::Close, true, false));
    if (frame->btn_close) {
        wlr_scene_node_set_position(&frame->btn_close->node, 0, 0);
        wlr_scene_buffer_set_dest_size(frame->btn_close, DecorationAssetCache::kTrafficSize, DecorationAssetCache::kTrafficSize);
    }
    frame->btn_min = make_decor_sprite(frame->traffic_tree, m_assets.traffic_button(BK::Minimize, true, false));
    if (frame->btn_min) {
        wlr_scene_node_set_position(&frame->btn_min->node, 20, 0);
        wlr_scene_buffer_set_dest_size(frame->btn_min, DecorationAssetCache::kTrafficSize, DecorationAssetCache::kTrafficSize);
    }
    frame->btn_max = make_decor_sprite(frame->traffic_tree, m_assets.traffic_button(BK::Maximize, true, false));
    if (frame->btn_max) {
        wlr_scene_node_set_position(&frame->btn_max->node, 40, 0);
        wlr_scene_buffer_set_dest_size(frame->btn_max, DecorationAssetCache::kTrafficSize, DecorationAssetCache::kTrafficSize);
    }

    // 6. Client tree container (offset by 1px left, 29px top)
    frame->client_tree = wlr_scene_tree_create(frame->frame_tree);
    wlr_scene_node_set_position(&frame->client_tree->node, 1, 29);

    frame->set_active(true);

    TinexusWindowFrame* ptr = frame.get();
    m_frames[window_key] = std::move(frame);
    log::info("[Decoration] Created HiDPI Tinexus SSD frame for window {} ('{}')",
              window_key, label ? label : "unknown");
    return ptr;
}

void TinexusDecorationManager::destroy_frame_impl(void* window_key) {
    auto it = m_frames.find(window_key);
    if (it != m_frames.end()) {
        if (m_hovered_frame == it->second.get()) {
            m_hovered_frame = nullptr;
        }
        if (it->second && it->second->frame_tree) {
            wlr_scene_node_destroy(&it->second->frame_tree->node);
        }
        m_frames.erase(it);
        log::info("[Decoration] Destroyed SSD frame for window {}", window_key);
    }
}

TinexusWindowFrame* TinexusDecorationManager::get_frame_impl(void* window_key) const {
    auto it = m_frames.find(window_key);
    return (it != m_frames.end()) ? it->second.get() : nullptr;
}

TinexusWindowFrame* TinexusDecorationManager::create_frame(struct wlr_xdg_toplevel* toplevel, struct wlr_scene_tree* parent) {
    const char* label = (toplevel && toplevel->app_id) ? toplevel->app_id : "wayland-toplevel";
    return create_frame_impl(static_cast<void*>(toplevel), parent, label);
}

void TinexusDecorationManager::destroy_frame(struct wlr_xdg_toplevel* toplevel) {
    destroy_frame_impl(static_cast<void*>(toplevel));
}

TinexusWindowFrame* TinexusDecorationManager::get_frame(struct wlr_xdg_toplevel* toplevel) const {
    return get_frame_impl(static_cast<void*>(toplevel));
}

TinexusWindowFrame* TinexusDecorationManager::create_xwayland_frame(struct wlr_xwayland_surface* xsurface, struct wlr_scene_tree* parent) {
    const char* label = (xsurface && xsurface->c_class) ? xsurface->c_class : "x11-surface";
    return create_frame_impl(static_cast<void*>(xsurface), parent, label);
}

void TinexusDecorationManager::destroy_xwayland_frame(struct wlr_xwayland_surface* xsurface) {
    destroy_frame_impl(static_cast<void*>(xsurface));
}

TinexusWindowFrame* TinexusDecorationManager::get_xwayland_frame(struct wlr_xwayland_surface* xsurface) const {
    return get_frame_impl(static_cast<void*>(xsurface));
}

void TinexusDecorationManager::set_xwayland_active(struct wlr_xwayland_surface* xsurface, bool active) {
    auto* f = get_xwayland_frame(xsurface);
    if (f) f->set_active(active);
}

void TinexusDecorationManager::update_xwayland_geometry(struct wlr_xwayland_surface* xsurface, int32_t w, int32_t h) {
    auto* f = get_xwayland_frame(xsurface);
    if (f) f->update_geometry(w, h);
}

bool TinexusDecorationManager::handle_cursor_button(struct wlr_scene_node* node, double cursor_x, double cursor_y,
                                                   uint32_t /*button*/, uint32_t state, void* wrapper) {
    if (state != WL_POINTER_BUTTON_STATE_PRESSED || !node || !wrapper || !m_action_handler) {
        return false;
    }

    // Resolve ONLY the topmost frame owning `node` — never fall through to background frames
    TinexusWindowFrame* frame = find_frame_from_node(node);
    if (!frame || !frame->frame_tree) {
        return false;
    }

    int world_x = 0, world_y = 0;
    if (!wlr_scene_node_coords(&frame->frame_tree->node, &world_x, &world_y)) {
        return false;
    }
    double local_x = cursor_x - static_cast<double>(world_x);
    double local_y = cursor_y - static_cast<double>(world_y);
    HitTarget target = frame->hit_test(local_x, local_y);

    switch (target) {
        case HitTarget::CloseButton:
            log::info("[Decoration] Hit Close button on window {}", wrapper);
            m_action_handler->request_close(wrapper);
            return true;

        case HitTarget::MinimizeButton:
            log::info("[Decoration] Hit Minimize button on window {}", wrapper);
            m_action_handler->request_minimize(wrapper, true);
            return true;

        case HitTarget::MaximizeButton: {
            bool is_max = m_action_handler->is_window_maximized(wrapper);
            log::info("[Decoration] Hit Maximize button on window {} (currently max={})",
                      wrapper, is_max);
            m_action_handler->request_maximize(wrapper, !is_max);
            return true;
        }

        case HitTarget::TitlebarDrag: {
            // Double click within 350ms triggers maximize toggle
            auto now_ms = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());

            if (m_last_click_wrapper == wrapper && (now_ms - m_last_click_time) <= 350) {
                m_last_click_time = 0;
                m_last_click_wrapper = nullptr;
                bool is_max = m_action_handler->is_window_maximized(wrapper);
                log::info("[Decoration] Double-click on titlebar -> toggle maximize (state={})", !is_max);
                m_action_handler->request_maximize(wrapper, !is_max);
                return true;
            }

            m_last_click_time = now_ms;
            m_last_click_wrapper = wrapper;

            log::info("[Decoration] Initiating titlebar drag for window {}", wrapper);
            m_action_handler->request_move(wrapper);
            return true;
        }

        case HitTarget::BorderTop:
            m_action_handler->request_resize(wrapper, WLR_EDGE_TOP);
            return true;
        case HitTarget::BorderBottom:
            m_action_handler->request_resize(wrapper, WLR_EDGE_BOTTOM);
            return true;
        case HitTarget::BorderLeft:
            m_action_handler->request_resize(wrapper, WLR_EDGE_LEFT);
            return true;
        case HitTarget::BorderRight:
            m_action_handler->request_resize(wrapper, WLR_EDGE_RIGHT);
            return true;

        case HitTarget::None:
        default:
            return false;
    }
}

HitTarget TinexusDecorationManager::handle_cursor_motion(struct wlr_scene_node* node, double cursor_x, double cursor_y) {
    TinexusWindowFrame* frame = find_frame_from_node(node);
    if (m_hovered_frame && m_hovered_frame != frame) {
        m_hovered_frame->set_traffic_hovered(false);
        m_hovered_frame = nullptr;
    }
    if (!frame || !frame->frame_tree) {
        return HitTarget::None;
    }

    int world_x = 0, world_y = 0;
    if (!wlr_scene_node_coords(&frame->frame_tree->node, &world_x, &world_y)) {
        frame->set_traffic_hovered(false);
        if (m_hovered_frame == frame) m_hovered_frame = nullptr;
        return HitTarget::None;
    }

    HitTarget target = frame->hit_test(cursor_x - static_cast<double>(world_x),
                                       cursor_y - static_cast<double>(world_y));
    const bool over_traffic = (target == HitTarget::CloseButton ||
                               target == HitTarget::MinimizeButton ||
                               target == HitTarget::MaximizeButton);
    frame->set_traffic_hovered(over_traffic);
    m_hovered_frame = over_traffic ? frame : nullptr;
    return target;
}

void TinexusDecorationManager::set_toplevel_active(struct wlr_xdg_toplevel* toplevel, bool active) {
    auto* f = get_frame(toplevel);
    if (f) {
        f->set_active(active);
    }
}

void TinexusDecorationManager::set_toplevel_fullscreen(struct wlr_xdg_toplevel* toplevel, bool fullscreen) {
    auto* f = get_frame(toplevel);
    if (f) {
        f->set_fullscreen(fullscreen);
    }
}

void TinexusDecorationManager::set_toplevel_maximized(struct wlr_xdg_toplevel* toplevel, bool maximized) {
    auto* f = get_frame(toplevel);
    if (f) {
        f->set_maximized(maximized);
    }
}

void TinexusDecorationManager::update_toplevel_geometry(struct wlr_xdg_toplevel* toplevel, int32_t w, int32_t h) {
    auto* f = get_frame(toplevel);
    if (f) {
        f->update_geometry(w, h);
    }
}

// ---------------------------------------------------------------------------
// Protocol Signal Handlers
// ---------------------------------------------------------------------------

enum wlr_xdg_toplevel_decoration_v1_mode TinexusDecorationManager::resolve_decoration_mode(
    struct wlr_xdg_toplevel_decoration_v1* decoration) noexcept {
    if (!decoration || !decoration->toplevel) {
        return WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    }
    const char* app_id = decoration->toplevel->app_id;
    if (is_native_csd_app(app_id)) {
        return WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    }
    if (decoration->requested_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE) {
        return WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    }
    // Never pass WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE (asserted in wlroots 0.19)
    return WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
}

void TinexusDecorationManager::apply_decoration_context(DecorationContext* ctx) {
    if (!ctx || !ctx->decoration || !ctx->decoration->toplevel) return;
    struct wlr_xdg_toplevel_decoration_v1* decoration = ctx->decoration;
    struct wlr_xdg_toplevel* toplevel = decoration->toplevel;
    const char* app_id = toplevel->app_id;

    const auto mode = resolve_decoration_mode(decoration);
    ctx->pending_mode = mode;

    // wlroots 0.19 wlr_xdg_toplevel_decoration_v1_set_mode() calls
    // wlr_xdg_surface_schedule_configure(), which asserts base->initialized.
    if (!toplevel->base || !toplevel->base->initialized) {
        ctx->has_pending_mode = true;
        log::info("[Decoration] Deferring mode {} for uninitialized surface (app_id='{}')",
                  static_cast<int>(mode), app_id ? app_id : "unknown");
        return;
    }

    ctx->has_pending_mode = false;
    log::info("[Decoration] Applying mode {} for app_id='{}'",
              static_cast<int>(mode), app_id ? app_id : "unknown");
    wlr_xdg_toplevel_decoration_v1_set_mode(decoration, mode);

    if (ctx->manager && ctx->manager->m_action_handler) {
        ctx->manager->m_action_handler->on_decoration_mode_changed(toplevel);
    }
}

void TinexusDecorationManager::handle_new_toplevel_decoration(struct wl_listener* listener, void* data) {
    TinexusDecorationManager* self = wl_container_of(listener, self, m_new_decoration_listener);
    auto* decoration = static_cast<struct wlr_xdg_toplevel_decoration_v1*>(data);

    if (!decoration || !decoration->toplevel) return;

    const char* app_id = decoration->toplevel->app_id;
    log::info("[Decoration] New toplevel decoration interface created: toplevel={} app_id='{}'",
              static_cast<void*>(decoration->toplevel), app_id ? app_id : "unknown");

    auto ctx = std::make_unique<DecorationContext>();
    ctx->decoration = decoration;
    ctx->manager = self;
    ctx->pending_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    ctx->has_pending_mode = false;

    ctx->request_mode.notify = handle_decoration_request_mode;
    wl_signal_add(&decoration->events.request_mode, &ctx->request_mode);

    if (decoration->toplevel->base && decoration->toplevel->base->surface) {
        ctx->surface_commit.notify = handle_decoration_surface_commit;
        wl_signal_add(&decoration->toplevel->base->surface->events.commit, &ctx->surface_commit);
    }

    ctx->destroy.notify = handle_decoration_destroy;
    wl_signal_add(&decoration->events.destroy, &ctx->destroy);

    DecorationContext* raw_ctx = ctx.get();
    self->m_contexts.push_back(std::move(ctx));
    apply_decoration_context(raw_ctx);
}

void TinexusDecorationManager::handle_decoration_request_mode(struct wl_listener* listener, void* /*data*/) {
    DecorationContext* ctx = wl_container_of(listener, ctx, request_mode);
    if (!ctx || !ctx->decoration || !ctx->decoration->toplevel) return;

    const char* app_id = ctx->decoration->toplevel->app_id;
    log::info("[Decoration] Client requested mode {} for app_id='{}'",
              static_cast<int>(ctx->decoration->requested_mode), app_id ? app_id : "unknown");

    apply_decoration_context(ctx);
}

void TinexusDecorationManager::handle_decoration_surface_commit(struct wl_listener* listener, void* /*data*/) {
    DecorationContext* ctx = wl_container_of(listener, ctx, surface_commit);
    if (!ctx || !ctx->has_pending_mode || !ctx->decoration || !ctx->decoration->toplevel) {
        return;
    }
    if (ctx->decoration->toplevel->base && ctx->decoration->toplevel->base->initialized) {
        apply_decoration_context(ctx);
    }
}

void TinexusDecorationManager::handle_decoration_destroy(struct wl_listener* listener, void* /*data*/) {
    DecorationContext* ctx = wl_container_of(listener, ctx, destroy);
    log::info("[Decoration] Decoration destroyed for decoration={}", static_cast<void*>(ctx->decoration));

    if (ctx->request_mode.link.next) {
        wl_list_remove(&ctx->request_mode.link);
        ctx->request_mode.link.next = nullptr;
    }
    if (ctx->surface_commit.link.next) {
        wl_list_remove(&ctx->surface_commit.link);
        ctx->surface_commit.link.next = nullptr;
    }
    if (ctx->destroy.link.next) {
        wl_list_remove(&ctx->destroy.link);
        ctx->destroy.link.next = nullptr;
    }

    if (ctx->manager) {
        auto& list = ctx->manager->m_contexts;
        auto it = std::find_if(list.begin(), list.end(), [ctx](const auto& p) {
            return p.get() == ctx;
        });
        if (it != list.end()) {
            list.erase(it);
        }
    }
}

} // namespace tinexus::comp
