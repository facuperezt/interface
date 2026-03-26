#pragma once

/// @file app.hpp
/// @brief Main TUI application shell -- tab-based layout using FTXUI.

#include "interface/tui/keybindings.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace interface::tui {

/// A tab in the TUI application.
class i_tab {
public:
    virtual ~i_tab() = default;

    /// Tab display name (shown in the tab bar).
    [[nodiscard]] virtual auto name() const -> std::string = 0;

    /// Render the tab content. Returns an FTXUI Element.
    /// (Typed as void* here to avoid pulling FTXUI into every translation unit;
    ///  the actual implementation uses ftxui::Element directly.)
    // In the real implementation this will return ftxui::Element directly.

protected:
    i_tab() = default;
};

/// Configuration for the TUI application.
struct c_app_config {
    std::string title{"interface"};
    bool show_status_bar{true};
};

/// The TUI application instance.
class c_app {
public:
    explicit c_app(c_app_config config = {});

    /// Load custom keybindings from a JSON file (merges with defaults).
    auto load_keybindings(const std::filesystem::path& path) -> result_t<void>;

    /// Access the current keybindings.
    [[nodiscard]] auto keybindings() const -> const c_keybindings&;

    /// Run the application. Blocks until the user exits.
    auto run() -> int;

private:
    c_app_config m_config;
    c_keybindings m_keybindings;
};

/// Convenience free function (preserves backward compatibility).
auto run(c_app_config config = {}) -> int;

} // namespace interface::tui
