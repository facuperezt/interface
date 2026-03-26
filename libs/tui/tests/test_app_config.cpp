#include <catch2/catch_test_macros.hpp>
#include "interface/tui/app.hpp"

using namespace interface::tui;

TEST_CASE("c_app_config defaults", "[tui][config]") {
    c_app_config config{};
    REQUIRE(config.title == "interface");
    REQUIRE(config.show_status_bar == true);
}

TEST_CASE("c_app_config custom values", "[tui][config]") {
    c_app_config config{
        .title = "My Tool",
        .show_status_bar = false,
    };
    REQUIRE(config.title == "My Tool");
    REQUIRE(config.show_status_bar == false);
}
