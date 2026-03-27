#include <catch2/catch_test_macros.hpp>

#include "interface/tui/keybindings.hpp"

#include <filesystem>
#include <fstream>

using namespace interface::tui;

// ===========================================================================
// 1. Default keybindings are populated on construction
// ===========================================================================

TEST_CASE("Default keybindings are populated on construction",
          "[tui][keybindings]") {
    c_keybindings kb;
    auto bindings = kb.all_bindings();
    REQUIRE(!bindings.empty());

    // Spot-check a few well-known defaults.
    auto quit_combo = kb.combo_for(e_action::quit);
    REQUIRE(quit_combo.has_value());
    REQUIRE(quit_combo->key == "Q");
    REQUIRE(quit_combo->ctrl == true);

    auto next_tab = kb.combo_for(e_action::next_tab);
    REQUIRE(next_tab.has_value());
    REQUIRE(next_tab->key == "Tab");
}

// ===========================================================================
// 2. action_for() returns correct action for a bound combo
// ===========================================================================

TEST_CASE("action_for returns correct action for a bound combo",
          "[tui][keybindings]") {
    c_keybindings kb;
    c_key_combo tab_combo{"Tab"};
    auto action = kb.action_for(tab_combo);
    REQUIRE(action.has_value());
    REQUIRE(*action == e_action::next_tab);
}

// ===========================================================================
// 3. action_for() returns nullopt for unbound combo
// ===========================================================================

TEST_CASE("action_for returns nullopt for unbound combo",
          "[tui][keybindings]") {
    c_keybindings kb;
    c_key_combo weird{"Z", true, true, true}; // Ctrl+Alt+Shift+Z
    REQUIRE(!kb.action_for(weird).has_value());
}

// ===========================================================================
// 4. combo_for() returns correct combo for a bound action
// ===========================================================================

TEST_CASE("combo_for returns correct combo for a bound action",
          "[tui][keybindings]") {
    c_keybindings kb;
    auto combo = kb.combo_for(e_action::search);
    REQUIRE(combo.has_value());
    REQUIRE(combo->key == "F");
    REQUIRE(combo->ctrl == true);
}

// ===========================================================================
// 5. bind() overrides existing binding
// ===========================================================================

TEST_CASE("bind overrides existing binding", "[tui][keybindings]") {
    c_keybindings kb;

    // Override quit from Ctrl+Q to Ctrl+W.
    c_key_combo new_combo{"W", true};
    kb.bind(e_action::quit, new_combo);

    auto combo = kb.combo_for(e_action::quit);
    REQUIRE(combo.has_value());
    REQUIRE(combo->key == "W");
    REQUIRE(combo->ctrl == true);

    // The old Ctrl+Q should no longer resolve to quit.
    c_key_combo old_combo{"Q", true};
    auto action = kb.action_for(old_combo);
    REQUIRE(!action.has_value());
}

// ===========================================================================
// 6. unbind() removes a binding
// ===========================================================================

TEST_CASE("unbind removes a binding", "[tui][keybindings]") {
    c_keybindings kb;
    REQUIRE(kb.combo_for(e_action::quit).has_value());

    kb.unbind(e_action::quit);
    REQUIRE(!kb.combo_for(e_action::quit).has_value());
}

// ===========================================================================
// 7. reset_defaults() restores all defaults
// ===========================================================================

TEST_CASE("reset_defaults restores all defaults", "[tui][keybindings]") {
    c_keybindings kb;
    auto original = kb.all_bindings();

    // Modify several bindings.
    kb.unbind(e_action::quit);
    kb.bind(e_action::help, c_key_combo{"F12"});

    kb.reset_defaults();
    auto after_reset = kb.all_bindings();
    REQUIRE(original.size() == after_reset.size());

    // Check a known default was restored.
    auto quit = kb.combo_for(e_action::quit);
    REQUIRE(quit.has_value());
    REQUIRE(quit->key == "Q");
    REQUIRE(quit->ctrl == true);
}

// ===========================================================================
// 8. c_key_combo::from_string() parses various formats
// ===========================================================================

TEST_CASE("from_string parses various key combo formats",
          "[tui][keybindings]") {
    SECTION("Simple key") {
        auto r = c_key_combo::from_string("a");
        REQUIRE(r.has_value());
        REQUIRE(r->key == "a");
        REQUIRE(!r->ctrl);
        REQUIRE(!r->alt);
        REQUIRE(!r->shift);
    }

    SECTION("Ctrl+Q") {
        auto r = c_key_combo::from_string("Ctrl+Q");
        REQUIRE(r.has_value());
        REQUIRE(r->key == "Q");
        REQUIRE(r->ctrl);
    }

    SECTION("Shift+Tab") {
        auto r = c_key_combo::from_string("Shift+Tab");
        REQUIRE(r.has_value());
        REQUIRE(r->key == "Tab");
        REQUIRE(r->shift);
    }

    SECTION("F1") {
        auto r = c_key_combo::from_string("F1");
        REQUIRE(r.has_value());
        REQUIRE(r->key == "F1");
    }

    SECTION("Escape") {
        auto r = c_key_combo::from_string("Escape");
        REQUIRE(r.has_value());
        REQUIRE(r->key == "Escape");
    }

    SECTION("Ctrl+Shift+F") {
        auto r = c_key_combo::from_string("Ctrl+Shift+F");
        REQUIRE(r.has_value());
        REQUIRE(r->key == "F");
        REQUIRE(r->ctrl);
        REQUIRE(r->shift);
    }

    SECTION("Alt+X") {
        auto r = c_key_combo::from_string("Alt+X");
        REQUIRE(r.has_value());
        REQUIRE(r->key == "X");
        REQUIRE(r->alt);
    }
}

// ===========================================================================
// 9. c_key_combo::from_string() rejects invalid strings
// ===========================================================================

TEST_CASE("from_string rejects invalid key strings", "[tui][keybindings]") {
    SECTION("Empty string") {
        auto r = c_key_combo::from_string("");
        REQUIRE(!r.has_value());
    }

    SECTION("Unknown modifier") {
        auto r = c_key_combo::from_string("Super+A");
        REQUIRE(!r.has_value());
    }
}

// ===========================================================================
// 10. c_key_combo::to_string() round-trips with from_string
// ===========================================================================

TEST_CASE("to_string round-trips with from_string", "[tui][keybindings]") {
    auto originals = {
        c_key_combo{"Q", true, false, false},
        c_key_combo{"Tab", false, false, true},
        c_key_combo{"F1"},
        c_key_combo{"a"},
        c_key_combo{"Escape"},
        c_key_combo{"F", true, false, true},
    };

    for (const auto& original : originals) {
        auto str = original.to_string();
        auto parsed = c_key_combo::from_string(str);
        REQUIRE(parsed.has_value());
        REQUIRE(*parsed == original);
    }
}

// ===========================================================================
// 11. to_json() / from_json() round-trip
// ===========================================================================

TEST_CASE("to_json and from_json round-trip", "[tui][keybindings]") {
    c_keybindings original;
    auto j = original.to_json();

    // Verify JSON structure.
    REQUIRE(j.contains("version"));
    REQUIRE(j["version"] == 1);
    REQUIRE(j.contains("bindings"));
    REQUIRE(j["bindings"].is_object());

    // Load into a fresh instance.
    c_keybindings loaded;
    loaded.unbind(e_action::quit); // modify to ensure from_json actually applies
    auto result = loaded.from_json(j);
    REQUIRE(result.has_value());

    // Verify the quit binding was restored.
    auto quit = loaded.combo_for(e_action::quit);
    REQUIRE(quit.has_value());
    REQUIRE(quit->key == "Q");
    REQUIRE(quit->ctrl);
}

// ===========================================================================
// 12. load_from_file() / save_to_file() round-trip via temp file
// ===========================================================================

TEST_CASE("save_to_file and load_from_file round-trip via temp file",
          "[tui][keybindings]") {
    auto tmp = std::filesystem::temp_directory_path() / "test_keybindings.json";

    c_keybindings original;
    auto save_result = original.save_to_file(tmp);
    REQUIRE(save_result.has_value());

    c_keybindings loaded;
    loaded.unbind(e_action::quit);
    auto load_result = loaded.load_from_file(tmp);
    REQUIRE(load_result.has_value());

    auto quit = loaded.combo_for(e_action::quit);
    REQUIRE(quit.has_value());
    REQUIRE(quit->key == "Q");
    REQUIRE(quit->ctrl);

    std::filesystem::remove(tmp);
}

// ===========================================================================
// 13. Partial JSON config merges with defaults (unmentioned keys stay)
// ===========================================================================

TEST_CASE("Partial JSON config merges with defaults",
          "[tui][keybindings]") {
    c_keybindings kb;

    // Only override quit binding in JSON.
    nlohmann::json j = {
        {"version", 1},
        {"bindings", {{"quit", "Ctrl+W"}}}
    };

    auto result = kb.from_json(j);
    REQUIRE(result.has_value());

    // Quit was overridden.
    auto quit = kb.combo_for(e_action::quit);
    REQUIRE(quit.has_value());
    REQUIRE(quit->key == "W");
    REQUIRE(quit->ctrl);

    // Other defaults remain.
    auto next_tab = kb.combo_for(e_action::next_tab);
    REQUIRE(next_tab.has_value());
    REQUIRE(next_tab->key == "Tab");
}

// ===========================================================================
// 14. all_bindings() returns all current bindings
// ===========================================================================

TEST_CASE("all_bindings returns all current bindings",
          "[tui][keybindings]") {
    c_keybindings kb;
    auto bindings = kb.all_bindings();

    // We should have at least the number of defaults (33 actions).
    REQUIRE(bindings.size() >= 30);

    // Verify it contains quit.
    bool found_quit = false;
    for (const auto& [action, combo] : bindings) {
        if (action == e_action::quit) {
            found_quit = true;
            break;
        }
    }
    REQUIRE(found_quit);
}

// ===========================================================================
// 15. action_name() returns human-readable name for each action
// ===========================================================================

TEST_CASE("action_name returns correct name", "[tui][keybindings]") {
    REQUIRE(c_keybindings::action_name(e_action::quit) == "quit");
    REQUIRE(c_keybindings::action_name(e_action::next_tab) == "next_tab");
    REQUIRE(c_keybindings::action_name(e_action::scroll_up) == "scroll_up");
    REQUIRE(c_keybindings::action_name(e_action::export_data) == "export_data");
    REQUIRE(c_keybindings::action_name(e_action::reset_detector) == "reset_detector");
}

// ===========================================================================
// 16. action_category() returns correct category
// ===========================================================================

TEST_CASE("action_category returns correct category", "[tui][keybindings]") {
    REQUIRE(c_keybindings::action_category(e_action::next_tab) == "Navigation");
    REQUIRE(c_keybindings::action_category(e_action::quit) == "General");
    REQUIRE(c_keybindings::action_category(e_action::scroll_up) == "Trace Viewer");
    REQUIRE(c_keybindings::action_category(e_action::send_request) == "UDS Console");
    REQUIRE(c_keybindings::action_category(e_action::refresh_od) == "CANopen");
    REQUIRE(c_keybindings::action_category(e_action::copy) == "Common");
    REQUIRE(c_keybindings::action_category(e_action::add_rule) == "Sequence Detector");
}

// ===========================================================================
// 17. display_string() shows the current binding as a human-readable string
// ===========================================================================

TEST_CASE("display_string shows current binding", "[tui][keybindings]") {
    c_keybindings kb;

    REQUIRE(kb.display_string(e_action::quit) == "Ctrl+Q");
    REQUIRE(kb.display_string(e_action::next_tab) == "Tab");
    REQUIRE(kb.display_string(e_action::prev_tab) == "Shift+Tab");

    // Unbound action should show "(unbound)".
    kb.unbind(e_action::quit);
    REQUIRE(kb.display_string(e_action::quit) == "(unbound)");
}

// ===========================================================================
// 18. Duplicate combo detection - binding same combo to two actions replaces first
// ===========================================================================

TEST_CASE("Binding same combo to two actions replaces the first",
          "[tui][keybindings]") {
    c_keybindings kb;

    // Tab is currently bound to next_tab.
    c_key_combo tab_combo{"Tab"};
    REQUIRE(kb.action_for(tab_combo) == e_action::next_tab);

    // Bind Tab to help instead.
    kb.bind(e_action::help, tab_combo);

    // Tab should now resolve to help only.
    REQUIRE(kb.action_for(tab_combo) == e_action::help);

    // next_tab should no longer be bound (it was evicted).
    REQUIRE(!kb.combo_for(e_action::next_tab).has_value());
}

// ===========================================================================
// 19. matches() with constructed FTXUI events
// ===========================================================================

TEST_CASE("matches with FTXUI events", "[tui][keybindings]") {
    c_keybindings kb;

    SECTION("Tab event matches next_tab") {
        REQUIRE(kb.matches(ftxui::Event::Tab, e_action::next_tab));
    }

    SECTION("Shift+Tab event matches prev_tab") {
        REQUIRE(kb.matches(ftxui::Event::TabReverse, e_action::prev_tab));
    }

    SECTION("F1 event matches help") {
        REQUIRE(kb.matches(ftxui::Event::F1, e_action::help));
    }

    SECTION("Arrow Up matches history_prev") {
        REQUIRE(kb.matches(ftxui::Event::ArrowUp, e_action::history_prev));
    }

    SECTION("Enter matches send_request") {
        REQUIRE(kb.matches(ftxui::Event::Return, e_action::send_request));
    }

    SECTION("Escape matches clear_filter") {
        REQUIRE(kb.matches(ftxui::Event::Escape, e_action::clear_filter));
    }

    SECTION("Character event matches scroll_down") {
        REQUIRE(kb.matches(ftxui::Event::Character('j'), e_action::scroll_down));
    }

    SECTION("Unbound event does not match") {
        REQUIRE(!kb.matches(ftxui::Event::Character('z'), e_action::quit));
    }
}

// ===========================================================================
// 20. from_ftxui_event handles special keys
// ===========================================================================

TEST_CASE("from_ftxui_event handles special keys", "[tui][keybindings]") {
    SECTION("Space") {
        auto combo = c_key_combo::from_ftxui_event(ftxui::Event::Character(' '));
        REQUIRE(combo.has_value());
        REQUIRE(combo->key == "Space");
    }

    SECTION("PageUp") {
        auto combo = c_key_combo::from_ftxui_event(ftxui::Event::PageUp);
        REQUIRE(combo.has_value());
        REQUIRE(combo->key == "PgUp");
    }

    SECTION("PageDown") {
        auto combo = c_key_combo::from_ftxui_event(ftxui::Event::PageDown);
        REQUIRE(combo.has_value());
        REQUIRE(combo->key == "PgDn");
    }

    SECTION("Backspace") {
        auto combo = c_key_combo::from_ftxui_event(ftxui::Event::Backspace);
        REQUIRE(combo.has_value());
        REQUIRE(combo->key == "Backspace");
    }

    SECTION("Home and End") {
        auto home = c_key_combo::from_ftxui_event(ftxui::Event::Home);
        REQUIRE(home.has_value());
        REQUIRE(home->key == "Home");

        auto end = c_key_combo::from_ftxui_event(ftxui::Event::End);
        REQUIRE(end.has_value());
        REQUIRE(end->key == "End");
    }
}

// ===========================================================================
// 21. Ctrl+letter via Event::Special (FTXUI sends Ctrl+A..Z as Special, not Character)
// ===========================================================================

TEST_CASE("Ctrl+letter events via Event::Special are recognized",
          "[tui][keybindings]") {
    c_keybindings kb;

    SECTION("Ctrl+Q via Special matches quit") {
        // FTXUI sends Ctrl+Q as Event::Special({17}) -- 17 = 'Q' - 'A' + 1
        auto ctrl_q = ftxui::Event::Special({17});
        REQUIRE(kb.matches(ctrl_q, e_action::quit));
    }

    SECTION("Ctrl+F via Special matches search") {
        // Ctrl+F = Special({6})
        auto ctrl_f = ftxui::Event::Special({6});
        REQUIRE(kb.matches(ctrl_f, e_action::search));
    }

    SECTION("Ctrl+E via Special matches export_data") {
        // Ctrl+E = Special({5})
        auto ctrl_e = ftxui::Event::Special({5});
        REQUIRE(kb.matches(ctrl_e, e_action::export_data));
    }

    SECTION("Ctrl+R via Special matches reset_detector") {
        // Ctrl+R = Special({18})
        auto ctrl_r = ftxui::Event::Special({18});
        REQUIRE(kb.matches(ctrl_r, e_action::reset_detector));
    }
}

// ===========================================================================
// 22. load_from_file reports error on missing file
// ===========================================================================

TEST_CASE("load_from_file reports error on missing file",
          "[tui][keybindings]") {
    c_keybindings kb;
    auto result = kb.load_from_file("/nonexistent/path/bindings.json");
    REQUIRE(!result.has_value());
    REQUIRE(result.error().category == interface::e_error_category::io);
}

// ===========================================================================
// 22. from_json reports error when bindings key is missing
// ===========================================================================

TEST_CASE("from_json reports error when bindings key is missing",
          "[tui][keybindings]") {
    c_keybindings kb;
    nlohmann::json j = {{"version", 1}};
    auto result = kb.from_json(j);
    REQUIRE(!result.has_value());
    REQUIRE(result.error().category == interface::e_error_category::parse);
}
