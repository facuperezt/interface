#pragma once

/// @file keybindings.hpp
/// @brief Customizable keyboard shortcuts for the TUI application.

#include "interface/core/error.hpp"

#include <ftxui/component/event.hpp>

#include <nlohmann/json.hpp>

#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace interface::tui {

// ---------------------------------------------------------------------------
// e_action -- all bindable keyboard actions
// ---------------------------------------------------------------------------

enum class e_action {
    // Navigation
    next_tab,
    prev_tab,
    tab_1,
    tab_2,
    tab_3,
    tab_4,
    tab_5,

    // General
    quit,
    help,
    toggle_focus,

    // Trace viewer
    scroll_up,
    scroll_down,
    page_up,
    page_down,
    go_to_top,
    go_to_bottom,
    search,
    filter,
    clear_filter,

    // UDS Console
    send_request,
    clear_console,
    history_prev,
    history_next,

    // CANopen
    refresh_od,
    start_node,
    stop_node,

    // Common
    copy,
    export_data,
    toggle_pause,
    toggle_hex_dec,

    // Sequence detector
    add_rule,
    remove_rule,
    reset_detector,
};

} // namespace interface::tui

// Enable e_action as an unordered_map key (must precede unordered_map usage).
template <>
struct std::hash<interface::tui::e_action> {
    auto operator()(interface::tui::e_action a) const noexcept -> std::size_t {
        return std::hash<int>{}(static_cast<int>(a));
    }
};

namespace interface::tui {

// ---------------------------------------------------------------------------
// c_key_combo -- represents a single key combination
// ---------------------------------------------------------------------------

struct c_key_combo {
    std::string key;     ///< e.g. "Tab", "q", "F1", "Enter", "1"-"9"
    bool ctrl  = false;
    bool alt   = false;
    bool shift = false;

    auto operator==(const c_key_combo&) const -> bool = default;

    /// Human-readable representation, e.g. "Ctrl+Shift+F".
    [[nodiscard]] auto to_string() const -> std::string;

    /// Parse a string like "Ctrl+Q", "Shift+Tab", "F1", "a", "Escape".
    static auto from_string(std::string_view s) -> std::expected<c_key_combo, c_error>;

    /// Convert an FTXUI Event to a c_key_combo (nullopt if not mappable).
    static auto from_ftxui_event(const ftxui::Event& e) -> std::optional<c_key_combo>;
};

// ---------------------------------------------------------------------------
// c_keybindings -- manages the full set of key-to-action mappings
// ---------------------------------------------------------------------------

class c_keybindings {
public:
    /// Construct with default bindings.
    c_keybindings();

    // -- Bind / unbind ------------------------------------------------------

    auto bind(e_action action, c_key_combo combo) -> void;
    auto unbind(e_action action) -> void;
    auto reset_defaults() -> void;

    // -- Lookup -------------------------------------------------------------

    [[nodiscard]] auto action_for(const c_key_combo& combo) const
        -> std::optional<e_action>;
    [[nodiscard]] auto combo_for(e_action action) const
        -> std::optional<c_key_combo>;
    [[nodiscard]] auto display_string(e_action action) const -> std::string;

    // -- Serialisation (JSON via nlohmann/json) -----------------------------

    [[nodiscard]] auto save_to_file(const std::filesystem::path& path) const
        -> result_t<void>;
    auto load_from_file(const std::filesystem::path& path) -> result_t<void>;
    [[nodiscard]] auto to_json() const -> nlohmann::json;
    auto from_json(const nlohmann::json& j) -> result_t<void>;

    // -- Enumeration --------------------------------------------------------

    [[nodiscard]] auto all_bindings() const
        -> std::vector<std::pair<e_action, c_key_combo>>;
    [[nodiscard]] static auto action_name(e_action action) -> std::string_view;
    [[nodiscard]] static auto action_category(e_action action)
        -> std::string_view;

    // -- FTXUI integration --------------------------------------------------

    [[nodiscard]] auto matches(const ftxui::Event& event,
                               e_action action) const -> bool;

private:
    std::unordered_map<e_action, c_key_combo> m_bindings;

    auto load_defaults() -> void;
};

} // namespace interface::tui
