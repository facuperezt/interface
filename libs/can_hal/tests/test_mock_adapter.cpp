#include <catch2/catch_test_macros.hpp>
#include "interface/can_hal/c_mock_adapter.hpp"

using namespace interface;
using namespace interface::can;
using namespace interface::can_hal;

TEST_CASE("Mock adapter lifecycle", "[can_hal][mock]") {
    c_mock_adapter adapter;

    REQUIRE_FALSE(adapter.is_open());

    auto result = adapter.open(c_bitrate_config{.nominal_bps = 500000});
    REQUIRE(result.has_value());
    REQUIRE(adapter.is_open());

    // Double open should fail
    auto result2 = adapter.open(c_bitrate_config{});
    REQUIRE_FALSE(result2.has_value());

    adapter.close();
    REQUIRE_FALSE(adapter.is_open());
}

TEST_CASE("Mock adapter send and TX history", "[can_hal][mock]") {
    c_mock_adapter adapter;
    adapter.open(c_bitrate_config{});

    c_can_frame frame{.id = 0x100, .dlc = 2, .data = {0x01, 0x02}};
    auto result = adapter.send(frame);
    REQUIRE(result.has_value());

    auto history = adapter.get_tx_history();
    REQUIRE(history.size() == 1);
    REQUIRE(history[0].id == 0x100);

    adapter.clear_tx_history();
    REQUIRE(adapter.get_tx_history().empty());
}

TEST_CASE("Mock adapter inject and receive", "[can_hal][mock]") {
    c_mock_adapter adapter;
    adapter.open(c_bitrate_config{});

    c_can_frame injected{.id = 0x200, .dlc = 1, .data = {0xFF}};
    adapter.inject_rx(injected);

    auto result = adapter.receive(std::chrono::milliseconds{50});
    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    REQUIRE(result->value().id == 0x200);
}

TEST_CASE("Mock adapter receive times out on empty queue", "[can_hal][mock]") {
    c_mock_adapter adapter;
    adapter.open(c_bitrate_config{});

    auto result = adapter.receive(std::chrono::milliseconds{10});
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->has_value()); // nullopt
}

TEST_CASE("Mock adapter filter applies to receive", "[can_hal][mock]") {
    c_mock_adapter adapter;
    adapter.open(c_bitrate_config{});
    adapter.set_filter(c_can_filter::exact(0x300));

    adapter.inject_rx(c_can_frame{.id = 0x200, .dlc = 0});
    adapter.inject_rx(c_can_frame{.id = 0x300, .dlc = 0});

    auto result = adapter.receive(std::chrono::milliseconds{50});
    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    REQUIRE(result->value().id == 0x300);
}

TEST_CASE("Mock adapter send fails when closed", "[can_hal][mock]") {
    c_mock_adapter adapter;
    auto result = adapter.send(c_can_frame{});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::hardware);
}

TEST_CASE("Mock adapter info", "[can_hal][mock]") {
    c_mock_adapter adapter;
    auto i = adapter.info();
    REQUIRE(i.driver == "mock");
    REQUIRE_FALSE(i.name.empty());
}
