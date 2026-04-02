/// @file app.cpp
/// @brief TUI application -- tab-based layout with FTXUI.

#include "interface/tui/app.hpp"
#include "interface/core/version.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#if defined(INTERFACE_HAS_CAN_DB) && defined(INTERFACE_HAS_CAN_TRACE)
#include "interface/can_db/c_dbc_parser.hpp"
#include "interface/can_db/c_trace_decoder.hpp"
#include "interface/can_trace/c_asc_reader.hpp"
#endif

namespace interface::tui {

#if defined(INTERFACE_HAS_CAN_DB) && defined(INTERFACE_HAS_CAN_TRACE)

/// State for the Trace Viewer tab.
struct c_trace_viewer_state {
    std::string dbc_path;
    std::string trace_path;
    std::string status_message{"No files loaded"};
    std::vector<can_db::c_decoded_frame> decoded_frames;
    int scroll_position{0};
    bool has_error{false};
    bool trace_focused{false};
    int cursor_row{0};
    int visible_rows{20};
};

namespace {

auto complete_path(std::string& current_path) -> void {
    namespace fs = std::filesystem;

    if (current_path.empty()) {
        current_path = "./";
        return;
    }

    fs::path partial(current_path);
    fs::path parent = partial.parent_path();
    std::string prefix = partial.filename().string();

    // If the path ends with '/', list that directory's contents
    if (current_path.back() == '/' || current_path.back() == '\\') {
        parent = partial;
        prefix = "";
    }

    if (parent.empty()) parent = ".";

    std::error_code ec;
    if (!fs::exists(parent, ec) || !fs::is_directory(parent, ec)) return;

    std::vector<std::string> matches;
    for (const auto& entry : fs::directory_iterator(parent, ec)) {
        auto name = entry.path().filename().string();
        if (name.starts_with(prefix)) {
            matches.push_back(name);
        }
    }

    if (matches.empty()) return;

    if (matches.size() == 1) {
        auto completed = parent / matches[0];
        current_path = completed.string();
        if (fs::is_directory(completed, ec)) {
            current_path += '/';
        }
        return;
    }

    // Multiple matches: complete the longest common prefix
    std::string common = matches[0];
    for (std::size_t i = 1; i < matches.size(); ++i) {
        std::size_t j = 0;
        while (j < common.size() && j < matches[i].size() && common[j] == matches[i][j]) {
            ++j;
        }
        common = common.substr(0, j);
    }
    if (common.size() > prefix.size()) {
        current_path = (parent / common).string();
    }
}

} // anonymous namespace

#endif // INTERFACE_HAS_CAN_DB && INTERFACE_HAS_CAN_TRACE

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

    auto tab_toggle = Toggle(&tab_names, &selected_tab);

    // -----------------------------------------------------------------
    // Trace Viewer tab
    // -----------------------------------------------------------------
#if defined(INTERFACE_HAS_CAN_DB) && defined(INTERFACE_HAS_CAN_TRACE)
    auto tv_state = std::make_shared<c_trace_viewer_state>();

    auto dbc_input = Input(&tv_state->dbc_path, "path/to/file.dbc");
    auto trace_input = Input(&tv_state->trace_path, "path/to/file.asc");

    // Shared decode logic used by both button and Enter key
    auto do_decode = [tv_state] {
        tv_state->has_error = false;
        tv_state->decoded_frames.clear();
        tv_state->scroll_position = 0;
        tv_state->cursor_row = 0;

        // Parse DBC
        can_db::c_dbc_parser parser;
        auto db_result = parser.parse(tv_state->dbc_path);
        if (!db_result.has_value()) {
            tv_state->status_message = "DBC error: " + db_result.error().message;
            tv_state->has_error = true;
            return;
        }

        // Open trace
        can_trace::c_asc_reader reader;
        auto open_result = reader.open(tv_state->trace_path);
        if (!open_result.has_value()) {
            tv_state->status_message = "Trace error: " + open_result.error().message;
            tv_state->has_error = true;
            return;
        }

        // Decode
        can_db::c_trace_decoder decoder(std::move(*db_result));
        auto decode_result = decoder.decode_trace(reader);
        if (!decode_result.has_value()) {
            tv_state->status_message = "Decode error: " + decode_result.error().message;
            tv_state->has_error = true;
            return;
        }

        tv_state->decoded_frames = std::move(*decode_result);

        // Build status message
        std::size_t known_count = 0;
        for (const auto& f : tv_state->decoded_frames) {
            if (f.known) ++known_count;
        }
        auto unknown_count = tv_state->decoded_frames.size() - known_count;

        auto dbc_name = std::filesystem::path(tv_state->dbc_path).filename().string();
        tv_state->status_message = std::format(
            "Loaded: {} | {} frames decoded ({} known, {} unknown)",
            dbc_name, tv_state->decoded_frames.size(), known_count, unknown_count
        );

        // Auto-focus trace table after successful decode
        if (!tv_state->decoded_frames.empty()) {
            tv_state->trace_focused = true;
        }
    };

    auto decode_button = Button("Decode", do_decode);

    // Wrap inputs with Tab-to-autocomplete
    auto dbc_input_with_complete = CatchEvent(dbc_input, [tv_state](Event event) -> bool {
        if (event == Event::Tab) {
            complete_path(tv_state->dbc_path);
            return true;
        }
        return false;
    });

    auto trace_input_with_complete = CatchEvent(trace_input, [tv_state](Event event) -> bool {
        if (event == Event::Tab) {
            complete_path(tv_state->trace_path);
            return true;
        }
        return false;
    });

    auto trace_controls = Container::Horizontal({
        dbc_input_with_complete,
        trace_input_with_complete,
        decode_button,
    });

    // Wrap controls so Enter triggers decode from any input field
    auto trace_controls_with_enter = CatchEvent(trace_controls, [do_decode](Event event) -> bool {
        if (event == Event::Return) {
            do_decode();
            return true;
        }
        return false;
    });

    auto trace_view_renderer = Renderer(trace_controls_with_enter, [tv_state, trace_controls_with_enter] {
        Elements header_row;
        header_row.push_back(text("Timestamp") | size(WIDTH, EQUAL, 16) | bold);
        header_row.push_back(text("ID") | size(WIDTH, EQUAL, 8) | bold);
        header_row.push_back(text("Message") | size(WIDTH, EQUAL, 18) | bold);
        header_row.push_back(text("DLC") | size(WIDTH, EQUAL, 5) | bold);
        header_row.push_back(text("Data") | size(WIDTH, EQUAL, 26) | bold);
        header_row.push_back(text("Signals") | flex | bold);

        auto total = static_cast<int>(tv_state->decoded_frames.size());

        // Virtual scrolling: only render visible rows around cursor
        auto half = tv_state->visible_rows / 2;
        auto scroll_start = std::max(0, tv_state->cursor_row - half);
        auto scroll_end = std::min(total, scroll_start + tv_state->visible_rows);
        if (scroll_end == total) {
            scroll_start = std::max(0, total - tv_state->visible_rows);
        }

        Elements rows;
        for (int idx = scroll_start; idx < scroll_end; ++idx) {
            const auto& frame = tv_state->decoded_frames[static_cast<std::size_t>(idx)];
            auto ts = std::format("{:.6f}",
                static_cast<double>(frame.raw.timestamp) / 1'000'000.0);
            auto id = std::format("0x{:03X}", frame.raw.id);
            auto msg_name = frame.known ? frame.message_name : "???";
            auto dlc = std::format("{}", frame.raw.dlc);

            std::string data_hex;
            for (std::size_t i = 0; i < frame.raw.data_length(); ++i) {
                if (!data_hex.empty()) data_hex += ' ';
                data_hex += std::format("{:02X}", frame.raw.data[i]);
            }

            std::string sig_str;
            for (const auto& sig : frame.signals) {
                if (!sig_str.empty()) sig_str += ", ";
                sig_str += std::format("{}={:.2f} {}", sig.name, sig.value, sig.unit);
            }

            auto row_decorator = (idx == tv_state->cursor_row && tv_state->trace_focused)
                ? inverted
                : nothing;

            rows.push_back(hbox({
                text(ts) | size(WIDTH, EQUAL, 16),
                text(id) | size(WIDTH, EQUAL, 8),
                text(msg_name) | size(WIDTH, EQUAL, 18),
                text(dlc) | size(WIDTH, EQUAL, 5),
                text(data_hex) | size(WIDTH, EQUAL, 26),
                text(sig_str) | flex,
            }) | row_decorator);
        }

        auto status_color = tv_state->has_error ? color(Color::Red) : color(Color::Green);

        // Append row indicator at render time
        auto display_status = tv_state->status_message;
        if (!tv_state->decoded_frames.empty() && tv_state->trace_focused) {
            display_status += std::format(" | Row {}/{}",
                tv_state->cursor_row + 1, tv_state->decoded_frames.size());
        }

        return vbox({
            text("CAN Trace Viewer") | bold | center,
            separator(),
            hbox({
                text("DBC File: ") | bold,
                trace_controls_with_enter->ChildAt(0)->Render() | flex,
                text("  Trace File: ") | bold,
                trace_controls_with_enter->ChildAt(1)->Render() | flex,
                text(" "),
                trace_controls_with_enter->ChildAt(2)->Render(),
            }),
            separator(),
            hbox(header_row),
            separator(),
            vbox(std::move(rows)) | flex,
            separator(),
            text(display_status) | status_color,
        });
    });

    // Vim scrolling and focus management
    auto trace_view = CatchEvent(trace_view_renderer, [this, tv_state](Event event) -> bool {
        if (!tv_state->trace_focused) {
            // When not focused on trace, Escape focuses the trace (if we have frames)
            if (event == Event::Escape && !tv_state->decoded_frames.empty()) {
                tv_state->trace_focused = true;
                return true;
            }
            return false;
        }

        // Trace is focused -- handle vim motions
        auto total = static_cast<int>(tv_state->decoded_frames.size());
        if (total == 0) return false;

        // j / Down
        if (m_keybindings.matches(event, e_action::scroll_down) || event == Event::ArrowDown) {
            tv_state->cursor_row = std::min(tv_state->cursor_row + 1, total - 1);
            return true;
        }
        // k / Up
        if (m_keybindings.matches(event, e_action::scroll_up) || event == Event::ArrowUp) {
            tv_state->cursor_row = std::max(tv_state->cursor_row - 1, 0);
            return true;
        }
        // Ctrl+D (half page down)
        if (m_keybindings.matches(event, e_action::half_page_down)) {
            tv_state->cursor_row = std::min(tv_state->cursor_row + tv_state->visible_rows / 2, total - 1);
            return true;
        }
        // Ctrl+U (half page up)
        if (m_keybindings.matches(event, e_action::half_page_up)) {
            tv_state->cursor_row = std::max(tv_state->cursor_row - tv_state->visible_rows / 2, 0);
            return true;
        }
        // g (top)
        if (m_keybindings.matches(event, e_action::go_to_top)) {
            tv_state->cursor_row = 0;
            return true;
        }
        // G (bottom)
        if (m_keybindings.matches(event, e_action::go_to_bottom)) {
            tv_state->cursor_row = total - 1;
            return true;
        }
        // PgDn
        if (m_keybindings.matches(event, e_action::page_down)) {
            tv_state->cursor_row = std::min(tv_state->cursor_row + tv_state->visible_rows, total - 1);
            return true;
        }
        // PgUp
        if (m_keybindings.matches(event, e_action::page_up)) {
            tv_state->cursor_row = std::max(tv_state->cursor_row - tv_state->visible_rows, 0);
            return true;
        }
        // Escape or 'i': back to inputs
        if (event == Event::Escape || event == Event::Character('i')) {
            tv_state->trace_focused = false;
            return true;
        }

        return false;
    });

#else
    auto trace_view = Renderer([] {
        return vbox({
            text("CAN Trace Viewer") | bold | center,
            separator(),
            text("Load a trace file (.asc, .blf, .csv) or connect a CAN adapter.") | dim,
            filler(),
        });
    });
#endif // INTERFACE_HAS_CAN_DB && INTERFACE_HAS_CAN_TRACE

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
    // FTXUI's Toggle navigates with ArrowLeft/ArrowRight, not Tab.
    // We intercept Tab/Shift+Tab and forward as arrow events so the
    // Toggle highlight AND selected_tab stay in sync.
    auto with_keybindings = CatchEvent(main_component, [&
#if defined(INTERFACE_HAS_CAN_DB) && defined(INTERFACE_HAS_CAN_TRACE)
        , tv_state
#endif
    ](Event event) -> bool {
        if (m_keybindings.matches(event, e_action::quit)) {
            screen.Exit();
            return true;
        }
        // Tab/Shift+Tab: translate to ArrowRight/ArrowLeft for the Toggle.
        // On the Trace Viewer tab, let Tab pass through for autocomplete
        // when the trace table is not focused (inputs need Tab).
#if defined(INTERFACE_HAS_CAN_DB) && defined(INTERFACE_HAS_CAN_TRACE)
        bool trace_tab_input_active = (selected_tab == 0 && !tv_state->trace_focused);
#else
        bool trace_tab_input_active = false;
#endif
        if (m_keybindings.matches(event, e_action::next_tab) && !trace_tab_input_active) {
            tab_toggle->OnEvent(Event::ArrowRight);
            return true;
        }
        if (m_keybindings.matches(event, e_action::prev_tab)) {
            tab_toggle->OnEvent(Event::ArrowLeft);
            return true;
        }
        // Number keys for direct tab selection
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
