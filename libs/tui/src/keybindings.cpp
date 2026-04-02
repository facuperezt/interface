/// @file keybindings.cpp
/// @brief Customizable keyboard shortcuts implementation.

#include "interface/tui/keybindings.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <ranges>
#include <sstream>

namespace interface::tui {

// ===========================================================================
// Action metadata
// ===========================================================================

namespace {

struct action_info {
    e_action action;
    std::string_view name;
    std::string_view category;
};

// clang-format off
constexpr std::array k_action_table = {
    action_info{e_action::next_tab,       "next_tab",       "Navigation"},
    action_info{e_action::prev_tab,       "prev_tab",       "Navigation"},
    action_info{e_action::tab_1,          "tab_1",          "Navigation"},
    action_info{e_action::tab_2,          "tab_2",          "Navigation"},
    action_info{e_action::tab_3,          "tab_3",          "Navigation"},
    action_info{e_action::tab_4,          "tab_4",          "Navigation"},
    action_info{e_action::tab_5,          "tab_5",          "Navigation"},

    action_info{e_action::quit,           "quit",           "General"},
    action_info{e_action::help,           "help",           "General"},
    action_info{e_action::toggle_focus,   "toggle_focus",   "General"},

    action_info{e_action::scroll_up,      "scroll_up",      "Trace Viewer"},
    action_info{e_action::scroll_down,    "scroll_down",    "Trace Viewer"},
    action_info{e_action::page_up,        "page_up",        "Trace Viewer"},
    action_info{e_action::page_down,      "page_down",      "Trace Viewer"},
    action_info{e_action::half_page_up,  "half_page_up",   "Trace Viewer"},
    action_info{e_action::half_page_down,"half_page_down",  "Trace Viewer"},
    action_info{e_action::go_to_top,      "go_to_top",      "Trace Viewer"},
    action_info{e_action::go_to_bottom,   "go_to_bottom",   "Trace Viewer"},
    action_info{e_action::search,         "search",         "Trace Viewer"},
    action_info{e_action::filter,         "filter",         "Trace Viewer"},
    action_info{e_action::clear_filter,   "clear_filter",   "Trace Viewer"},

    action_info{e_action::send_request,   "send_request",   "UDS Console"},
    action_info{e_action::clear_console,  "clear_console",  "UDS Console"},
    action_info{e_action::history_prev,   "history_prev",   "UDS Console"},
    action_info{e_action::history_next,   "history_next",   "UDS Console"},

    action_info{e_action::refresh_od,     "refresh_od",     "CANopen"},
    action_info{e_action::start_node,     "start_node",     "CANopen"},
    action_info{e_action::stop_node,      "stop_node",      "CANopen"},

    action_info{e_action::copy,           "copy",           "Common"},
    action_info{e_action::export_data,    "export_data",    "Common"},
    action_info{e_action::toggle_pause,   "toggle_pause",   "Common"},
    action_info{e_action::toggle_hex_dec, "toggle_hex_dec", "Common"},

    action_info{e_action::add_rule,       "add_rule",       "Sequence Detector"},
    action_info{e_action::remove_rule,    "remove_rule",    "Sequence Detector"},
    action_info{e_action::reset_detector, "reset_detector", "Sequence Detector"},
};
// clang-format on

auto find_action_info(e_action action) -> const action_info* {
    for (const auto& info : k_action_table) {
        if (info.action == action) {
            return &info;
        }
    }
    return nullptr;
}

auto find_action_by_name(std::string_view name) -> std::optional<e_action> {
    for (const auto& info : k_action_table) {
        if (info.name == name) {
            return info.action;
        }
    }
    return std::nullopt;
}

/// Case-insensitive string comparison.
auto ci_equal(std::string_view a, std::string_view b) -> bool {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

// ===========================================================================
// c_key_combo
// ===========================================================================

auto c_key_combo::to_string() const -> std::string {
    std::string result;
    if (ctrl)  result += "Ctrl+";
    if (alt)   result += "Alt+";
    if (shift) result += "Shift+";
    result += key;
    return result;
}

auto c_key_combo::from_string(std::string_view s)
    -> std::expected<c_key_combo, c_error> {
    if (s.empty()) {
        return make_error("Empty key string", e_error_category::parse);
    }

    c_key_combo combo;

    // Split on '+' — modifiers come first, key is the last token.
    std::string input{s};
    std::vector<std::string> tokens;
    {
        std::istringstream stream(input);
        std::string token;
        while (std::getline(stream, token, '+')) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
    }

    if (tokens.empty()) {
        return make_error("Invalid key string: no tokens", e_error_category::parse);
    }

    // Last token is the key itself.
    combo.key = tokens.back();
    tokens.pop_back();

    // Remaining tokens are modifiers.
    for (const auto& mod : tokens) {
        if (ci_equal(mod, "ctrl")) {
            combo.ctrl = true;
        } else if (ci_equal(mod, "alt")) {
            combo.alt = true;
        } else if (ci_equal(mod, "shift")) {
            combo.shift = true;
        } else {
            return make_error(
                std::string("Unknown modifier: ") + mod,
                e_error_category::parse);
        }
    }

    return combo;
}

auto c_key_combo::from_ftxui_event(const ftxui::Event& e)
    -> std::optional<c_key_combo> {
    // Tab / Shift+Tab
    if (e == ftxui::Event::Tab)        return c_key_combo{"Tab"};
    if (e == ftxui::Event::TabReverse) return c_key_combo{"Tab", false, false, true};

    // Arrow keys
    if (e == ftxui::Event::ArrowUp)    return c_key_combo{"Up"};
    if (e == ftxui::Event::ArrowDown)  return c_key_combo{"Down"};
    if (e == ftxui::Event::ArrowLeft)  return c_key_combo{"Left"};
    if (e == ftxui::Event::ArrowRight) return c_key_combo{"Right"};

    // Enter, Escape, Backspace
    if (e == ftxui::Event::Return)     return c_key_combo{"Enter"};
    if (e == ftxui::Event::Escape)     return c_key_combo{"Escape"};
    if (e == ftxui::Event::Backspace)  return c_key_combo{"Backspace"};

    // Page Up / Page Down / Home / End (CSI sequences)
    if (e == ftxui::Event::PageUp)     return c_key_combo{"PgUp"};
    if (e == ftxui::Event::PageDown)   return c_key_combo{"PgDn"};
    if (e == ftxui::Event::Home)       return c_key_combo{"Home"};
    if (e == ftxui::Event::End)        return c_key_combo{"End"};

    // Delete
    if (e == ftxui::Event::Delete)     return c_key_combo{"Delete"};

    // Space
    if (e == ftxui::Event::Character(' ')) return c_key_combo{"Space"};

    // F-keys (F1-F12) — FTXUI represents them as escape sequences.
    if (e == ftxui::Event::F1)  return c_key_combo{"F1"};
    if (e == ftxui::Event::F2)  return c_key_combo{"F2"};
    if (e == ftxui::Event::F3)  return c_key_combo{"F3"};
    if (e == ftxui::Event::F4)  return c_key_combo{"F4"};
    if (e == ftxui::Event::F5)  return c_key_combo{"F5"};
    if (e == ftxui::Event::F6)  return c_key_combo{"F6"};
    if (e == ftxui::Event::F7)  return c_key_combo{"F7"};
    if (e == ftxui::Event::F8)  return c_key_combo{"F8"};
    if (e == ftxui::Event::F9)  return c_key_combo{"F9"};
    if (e == ftxui::Event::F10) return c_key_combo{"F10"};
    if (e == ftxui::Event::F11) return c_key_combo{"F11"};
    if (e == ftxui::Event::F12) return c_key_combo{"F12"};

    // Ctrl+letter and printable characters.
    // FTXUI sends Ctrl+A..Z as Event::Special({1..26}), NOT Character.
    // Regular printable keys come as Event::Character. We check the raw
    // input bytes for both cases.
    auto input = e.input();

    // Alt+key: terminals send ESC (0x1B) followed by the key character.
    // FTXUI delivers this as a 2-byte SPECIAL event.
    if (input.size() == 2 && input[0] == '\x1B') {
        char ch = input[1];
        if (ch >= '!' && ch <= '~') {
            if (ch >= 'A' && ch <= 'Z') {
                std::string key_name(1, ch);
                return c_key_combo{key_name, false, true, true}; // Alt+Shift
            }
            std::string key_name(1, ch);
            return c_key_combo{key_name, false, true}; // Alt+key
        }
    }

    if (input.size() == 1) {
        char ch = input[0];
        // Ctrl+A to Ctrl+Z (0x01 to 0x1A)
        if (ch >= '\x01' && ch <= '\x1A') {
            std::string key_name(1, static_cast<char>('A' + (ch - '\x01')));
            return c_key_combo{key_name, true};
        }
        // Regular printable character
        if (ch >= '!' && ch <= '~') {
            // Uppercase letter indicates Shift was held.
            if (ch >= 'A' && ch <= 'Z') {
                std::string key_name(1, ch);
                return c_key_combo{key_name, false, false, true};
            }
            std::string key_name(1, ch);
            return c_key_combo{key_name};
        }
    }

    return std::nullopt;
}

// ===========================================================================
// c_keybindings
// ===========================================================================

c_keybindings::c_keybindings() { load_defaults(); }

// -- Bind / unbind ----------------------------------------------------------

auto c_keybindings::bind(e_action action, c_key_combo combo) -> void {
    // Remove any existing binding for this combo (prevent duplicates).
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        if (it->second == combo && it->first != action) {
            m_bindings.erase(it);
            break;
        }
    }
    m_bindings.insert_or_assign(action, std::move(combo));
}

auto c_keybindings::unbind(e_action action) -> void {
    m_bindings.erase(action);
}

auto c_keybindings::reset_defaults() -> void {
    m_bindings.clear();
    load_defaults();
}

// -- Lookup -----------------------------------------------------------------

auto c_keybindings::action_for(const c_key_combo& combo) const
    -> std::optional<e_action> {
    for (const auto& [action, bound] : m_bindings) {
        if (bound == combo) return action;
    }
    return std::nullopt;
}

auto c_keybindings::combo_for(e_action action) const
    -> std::optional<c_key_combo> {
    auto it = m_bindings.find(action);
    if (it != m_bindings.end()) return it->second;
    return std::nullopt;
}

auto c_keybindings::display_string(e_action action) const -> std::string {
    auto c = combo_for(action);
    if (c) return c->to_string();
    return "(unbound)";
}

// -- Serialisation ----------------------------------------------------------

auto c_keybindings::save_to_file(const std::filesystem::path& path) const
    -> result_t<void> {
    std::ofstream out(path);
    if (!out) {
        return make_error(
            "Cannot open file for writing: " + path.string(),
            e_error_category::io);
    }
    out << to_json().dump(4) << '\n';
    if (!out) {
        return make_error(
            "Write failed: " + path.string(), e_error_category::io);
    }
    return {};
}

auto c_keybindings::load_from_file(const std::filesystem::path& path)
    -> result_t<void> {
    std::ifstream in(path);
    if (!in) {
        return make_error(
            "Cannot open file for reading: " + path.string(),
            e_error_category::io);
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const nlohmann::json::parse_error& ex) {
        return make_error(
            std::string("JSON parse error: ") + ex.what(),
            e_error_category::parse);
    }
    return from_json(j);
}

auto c_keybindings::to_json() const -> nlohmann::json {
    nlohmann::json j;
    j["version"] = 1;

    nlohmann::json bindings_obj = nlohmann::json::object();
    for (const auto& [action, combo] : m_bindings) {
        auto* info = find_action_info(action);
        if (info) {
            bindings_obj[std::string(info->name)] = combo.to_string();
        }
    }
    j["bindings"] = std::move(bindings_obj);
    return j;
}

auto c_keybindings::from_json(const nlohmann::json& j) -> result_t<void> {
    if (!j.contains("bindings") || !j["bindings"].is_object()) {
        return make_error(
            "JSON missing 'bindings' object", e_error_category::parse);
    }

    const auto& bindings_obj = j["bindings"];
    for (const auto& [name, value] : bindings_obj.items()) {
        if (!value.is_string()) continue;

        auto action = find_action_by_name(name);
        if (!action) continue; // ignore unknown action names

        auto combo = c_key_combo::from_string(value.get<std::string>());
        if (!combo) continue; // ignore invalid key strings

        bind(*action, *combo);
    }

    return {};
}

// -- Enumeration ------------------------------------------------------------

auto c_keybindings::all_bindings() const
    -> std::vector<std::pair<e_action, c_key_combo>> {
    std::vector<std::pair<e_action, c_key_combo>> result;
    result.reserve(m_bindings.size());
    for (const auto& [action, combo] : m_bindings) {
        result.emplace_back(action, combo);
    }
    return result;
}

auto c_keybindings::action_name(e_action action) -> std::string_view {
    auto* info = find_action_info(action);
    return info ? info->name : "unknown";
}

auto c_keybindings::action_category(e_action action) -> std::string_view {
    auto* info = find_action_info(action);
    return info ? info->category : "Unknown";
}

// -- FTXUI integration ------------------------------------------------------

auto c_keybindings::matches(const ftxui::Event& event,
                            e_action action) const -> bool {
    auto combo = combo_for(action);
    if (!combo) return false;

    auto event_combo = c_key_combo::from_ftxui_event(event);
    if (!event_combo) return false;

    return *combo == *event_combo;
}

// -- Default bindings -------------------------------------------------------

auto c_keybindings::load_defaults() -> void {
    // Navigation
    m_bindings[e_action::next_tab]  = c_key_combo{"Tab"};
    m_bindings[e_action::prev_tab]  = c_key_combo{"Tab", false, false, true}; // Shift+Tab
    m_bindings[e_action::tab_1]     = c_key_combo{"1", false, true}; // Alt+1
    m_bindings[e_action::tab_2]     = c_key_combo{"2", false, true}; // Alt+2
    m_bindings[e_action::tab_3]     = c_key_combo{"3", false, true}; // Alt+3
    m_bindings[e_action::tab_4]     = c_key_combo{"4", false, true}; // Alt+4
    m_bindings[e_action::tab_5]     = c_key_combo{"5", false, true}; // Alt+5

    // General
    m_bindings[e_action::quit]         = c_key_combo{"Q", true};   // Ctrl+Q
    m_bindings[e_action::help]         = c_key_combo{"F1"};
    m_bindings[e_action::toggle_focus] = c_key_combo{"F2"};

    // Trace Viewer
    m_bindings[e_action::scroll_up]    = c_key_combo{"k"};
    m_bindings[e_action::scroll_down]  = c_key_combo{"j"};
    m_bindings[e_action::page_up]       = c_key_combo{"PgUp"};
    m_bindings[e_action::page_down]    = c_key_combo{"PgDn"};
    m_bindings[e_action::half_page_up]   = c_key_combo{"U", true};  // Ctrl+U
    m_bindings[e_action::half_page_down] = c_key_combo{"D", true};  // Ctrl+D
    m_bindings[e_action::go_to_top]    = c_key_combo{"g"};
    m_bindings[e_action::go_to_bottom] = c_key_combo{"G", false, false, true}; // Shift+g
    m_bindings[e_action::search]       = c_key_combo{"F", true};   // Ctrl+F
    m_bindings[e_action::filter]       = c_key_combo{"f"};
    m_bindings[e_action::clear_filter] = c_key_combo{"Escape"};

    // UDS Console
    m_bindings[e_action::send_request] = c_key_combo{"Enter"};
    m_bindings[e_action::clear_console] = c_key_combo{"L", true};  // Ctrl+L
    m_bindings[e_action::history_prev] = c_key_combo{"Up"};
    m_bindings[e_action::history_next] = c_key_combo{"Down"};

    // CANopen
    m_bindings[e_action::refresh_od] = c_key_combo{"r"};
    m_bindings[e_action::start_node] = c_key_combo{"s"};
    m_bindings[e_action::stop_node]  = c_key_combo{"S", false, false, true}; // Shift+s

    // Common
    m_bindings[e_action::copy]           = c_key_combo{"C", true}; // Ctrl+C
    m_bindings[e_action::export_data]    = c_key_combo{"E", true}; // Ctrl+E
    m_bindings[e_action::toggle_pause]   = c_key_combo{"Space"};
    m_bindings[e_action::toggle_hex_dec] = c_key_combo{"h"};

    // Sequence Detector
    m_bindings[e_action::add_rule]       = c_key_combo{"a"};
    m_bindings[e_action::remove_rule]    = c_key_combo{"d"};
    m_bindings[e_action::reset_detector] = c_key_combo{"R", true}; // Ctrl+R
}

} // namespace interface::tui
