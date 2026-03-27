/// @file app.cpp
/// @brief TUI application -- tab-based layout with FTXUI.

#include "interface/tui/app.hpp"
#include "interface/core/version.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

namespace interface::tui {

// ---------------------------------------------------------------------------
// c_app
// ---------------------------------------------------------------------------

c_app::c_app(c_app_config config) : m_config(std::move(config)) {}

auto c_app::load_keybindings(const std::filesystem::path& path)
    -> result_t<void> {
    return m_keybindings.load_from_file(path);
}

auto c_app::keybindings() const -> const c_keybindings& {
    return m_keybindings;
}

auto c_app::run() -> int {
    using namespace ftxui;

    // Tab state
    int selected_tab = 0;
    std::vector<std::string> tab_names = {
        "Trace Viewer",
        "Database Browser",
        "UDS Console",
        "CANopen Explorer",
    };

    constexpr int k_num_tabs = 4;

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

    auto screen = ScreenInteractive::Fullscreen();

    // Wrap with a CatchEvent to handle keybindings.
    // Note: Tab/Shift+Tab are left to FTXUI's Toggle component so both the
    // tab bar highlight AND the content pane update together. We only
    // intercept non-Toggle shortcuts (quit, number keys) here.
    auto with_keybindings = CatchEvent(main_component, [&](Event event) -> bool {
        if (m_keybindings.matches(event, e_action::quit)) {
            screen.Exit();
            return true;
        }
        // Number keys for direct tab selection -- Toggle reads selected_tab
        // on next render, so just updating the variable is sufficient.
        if (m_keybindings.matches(event, e_action::tab_1)) {
            selected_tab = 0;
            return true;
        }
        if (m_keybindings.matches(event, e_action::tab_2)) {
            selected_tab = 1;
            return true;
        }
        if (m_keybindings.matches(event, e_action::tab_3)) {
            selected_tab = 2;
            return true;
        }
        if (m_keybindings.matches(event, e_action::tab_4)) {
            selected_tab = 3;
            return true;
        }
        // Let Tab/Shift+Tab fall through to FTXUI's Toggle so both the
        // tab bar highlight and the content pane update in sync.
        return false;
    });

    auto renderer = Renderer(with_keybindings, [&] {
        auto quit_key = m_keybindings.display_string(e_action::quit);
        auto status_text = std::string{"interface v"}
                           + std::string{interface::k_version_string}
                           + " | No adapter connected"
                           + " | " + quit_key + " quit";

        return vbox({
            // Header
            hbox({
                text(m_config.title) | bold,
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
            m_config.show_status_bar
                ? (separator(), hbox({text(status_text) | dim}))
                : text(""),
        }) | border;
    });

    screen.Loop(renderer);
    return 0;
}

// ---------------------------------------------------------------------------
// Convenience free function
// ---------------------------------------------------------------------------

auto run(c_app_config config) -> int {
    c_app app(std::move(config));
    return app.run();
}

} // namespace interface::tui
