/// @file app.cpp
/// @brief TUI application — tab-based layout with FTXUI.

#include "interface/tui/app.hpp"
#include "interface/core/version.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

namespace interface::tui {

auto run(c_app_config config) -> int {
    using namespace ftxui;

    // Tab state
    int selected_tab = 0;
    std::vector<std::string> tab_names = {
        "Trace Viewer",
        "Database Browser",
        "UDS Console",
        "CANopen Explorer",
    };

    auto tab_toggle = Toggle(&tab_names, &selected_tab);

    // Placeholder content for each tab
    auto trace_view = Renderer([] {
        return vbox({
            text("CAN Trace Viewer") | bold | center,
            separator(),
            text("Load a trace file (.asc, .blf, .csv) or connect a CAN adapter.") | dim,
            filler(),
        });
    });

    auto db_view = Renderer([] {
        return vbox({
            text("Database Browser") | bold | center,
            separator(),
            text("Load a database file (.dbc, .eds, .cdd) to browse messages and signals.") | dim,
            filler(),
        });
    });

    auto uds_view = Renderer([] {
        return vbox({
            text("UDS Console") | bold | center,
            separator(),
            text("Configure TX/RX IDs and send diagnostic requests.") | dim,
            filler(),
        });
    });

    auto canopen_view = Renderer([] {
        return vbox({
            text("CANopen Explorer") | bold | center,
            separator(),
            text("Browse Object Dictionary, SDO read/write, PDO monitor, NMT control.") | dim,
            filler(),
        });
    });

    auto tab_content = Container::Tab(
        {trace_view, db_view, uds_view, canopen_view},
        &selected_tab
    );

    auto main_component = Container::Vertical({
        tab_toggle,
        tab_content,
    });

    auto renderer = Renderer(main_component, [&] {
        auto status_text = std::string{"interface v"} + std::string{interface::k_version_string}
                           + " | No adapter connected";

        return vbox({
            // Header
            hbox({
                text(config.title) | bold,
                filler(),
                text(std::string{interface::k_version_string}) | dim,
            }) | color(Color::Cyan),
            separator(),

            // Tab bar
            tab_toggle->Render() | center,
            separator(),

            // Tab content
            tab_content->Render() | flex,

            // Status bar
            config.show_status_bar
                ? (separator(), hbox({text(status_text) | dim}))
                : text(""),
        }) | border;
    });

    auto screen = ScreenInteractive::Fullscreen();
    screen.Loop(renderer);

    return 0;
}

} // namespace interface::tui
