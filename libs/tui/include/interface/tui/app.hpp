#pragma once

/// @file app.hpp
/// @brief Main TUI application shell — tab-based layout using FTXUI.

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
    ///  the actual implementation uses ftxui::Element.)
    // In the real implementation this will return ftxui::Element directly.

protected:
    i_tab() = default;
};

/// Configuration for the TUI application.
struct c_app_config {
    std::string title{"interface"};
    bool show_status_bar{true};
};

/// Run the TUI application. Blocks until the user exits.
auto run(c_app_config config = {}) -> int;

} // namespace interface::tui
