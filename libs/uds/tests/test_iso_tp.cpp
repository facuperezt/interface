#include <catch2/catch_test_macros.hpp>
#include "interface/uds/iso_tp.hpp"
#include "interface/can_hal/c_mock_adapter.hpp"

using namespace interface;
using namespace interface::uds;
using namespace interface::can;
using namespace interface::can_hal;

namespace {

auto make_adapter() -> std::shared_ptr<c_mock_adapter> {
    auto adapter = std::make_shared<c_mock_adapter>();
    adapter->open(c_bitrate_config{});
    return adapter;
}

} // anonymous namespace

TEST_CASE("ISO-TP send single frame (<=7 bytes)", "[uds][isotp]") {
    auto adapter = make_adapter();
    c_isotp_transport tp(adapter, c_isotp_config{.tx_id = 0x7DF, .rx_id = 0x7E8});

    std::array<byte_t, 3> data{0x22, 0xF1, 0x90};
    auto result = tp.send(byte_span_t{data});
    REQUIRE(result.has_value());

    auto history = adapter->get_tx_history();
    REQUIRE(history.size() == 1);
    REQUIRE(history[0].id == 0x7DF);
    REQUIRE(history[0].data[0] == 0x03); // SF PCI: length=3
    REQUIRE(history[0].data[1] == 0x22);
    REQUIRE(history[0].data[2] == 0xF1);
    REQUIRE(history[0].data[3] == 0x90);
}

TEST_CASE("ISO-TP receive single frame", "[uds][isotp]") {
    auto adapter = make_adapter();
    c_isotp_transport tp(adapter, c_isotp_config{.tx_id = 0x7DF, .rx_id = 0x7E8});

    // Inject a single frame response
    c_can_frame rx_frame{.id = 0x7E8, .dlc = 8};
    rx_frame.data = {0x03, 0x62, 0xF1, 0x90, 0x00, 0x00, 0x00, 0x00};
    adapter->inject_rx(rx_frame);

    auto result = tp.receive();
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
    REQUIRE(result->at(0) == 0x62);
    REQUIRE(result->at(1) == 0xF1);
    REQUIRE(result->at(2) == 0x90);
}

TEST_CASE("ISO-TP send multi-frame with flow control", "[uds][isotp]") {
    auto adapter = make_adapter();
    c_isotp_transport tp(adapter, c_isotp_config{.tx_id = 0x7DF, .rx_id = 0x7E8});

    // Inject Flow Control response: CTS, BS=0, STmin=0
    c_can_frame fc_frame{.id = 0x7E8, .dlc = 8};
    fc_frame.data = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    adapter->inject_rx(fc_frame);

    // Send 10 bytes (requires FF + 1 CF)
    std::array<byte_t, 10> data{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    auto result = tp.send(byte_span_t{data});
    REQUIRE(result.has_value());

    auto history = adapter->get_tx_history();
    REQUIRE(history.size() == 2); // FF + 1 CF

    // First Frame: PCI = 0x10 | (len >> 8), len & 0xFF
    REQUIRE(history[0].data[0] == 0x10); // FF, upper nibble=1, length high=0
    REQUIRE(history[0].data[1] == 0x0A); // Length low = 10
    REQUIRE(history[0].data[2] == 0x01); // First data byte
    REQUIRE(history[0].data[7] == 0x06); // 6th data byte

    // Consecutive Frame 1: SN=1
    REQUIRE(history[1].data[0] == 0x21); // CF, SN=1
    REQUIRE(history[1].data[1] == 0x07); // 7th data byte
    REQUIRE(history[1].data[4] == 0x0A); // 10th data byte
}

TEST_CASE("ISO-TP receive multi-frame", "[uds][isotp]") {
    auto adapter = make_adapter();
    c_isotp_transport tp(adapter, c_isotp_config{.tx_id = 0x7DF, .rx_id = 0x7E8});

    // Inject First Frame: 10 bytes total
    c_can_frame ff{.id = 0x7E8, .dlc = 8};
    ff.data = {0x10, 0x0A, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    adapter->inject_rx(ff);

    // Inject Consecutive Frame: SN=1, 4 remaining bytes
    c_can_frame cf{.id = 0x7E8, .dlc = 8};
    cf.data = {0x21, 0x07, 0x08, 0x09, 0x0A, 0xCC, 0xCC, 0xCC};
    adapter->inject_rx(cf);

    auto result = tp.receive();
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 10);
    REQUIRE(result->at(0) == 0x01);
    REQUIRE(result->at(5) == 0x06);
    REQUIRE(result->at(6) == 0x07);
    REQUIRE(result->at(9) == 0x0A);

    // Verify FC was sent
    auto history = adapter->get_tx_history();
    REQUIRE(history.size() == 1); // FC frame
    REQUIRE((history[0].data[0] & 0xF0) == 0x30); // Flow Control
}

TEST_CASE("ISO-TP send fails when adapter not open", "[uds][isotp]") {
    auto adapter = std::make_shared<c_mock_adapter>(); // NOT opened
    c_isotp_transport tp(adapter);

    std::array<byte_t, 1> data{0x3E};
    auto result = tp.send(byte_span_t{data});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::hardware);
}

TEST_CASE("ISO-TP send fails on empty data", "[uds][isotp]") {
    auto adapter = make_adapter();
    c_isotp_transport tp(adapter);

    auto result = tp.send(byte_span_t{});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::protocol);
}

TEST_CASE("ISO-TP receive timeout", "[uds][isotp]") {
    auto adapter = make_adapter();
    c_isotp_transport tp(adapter, c_isotp_config{.timeout = std::chrono::milliseconds{20}});

    auto result = tp.receive();
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::timeout);
}

TEST_CASE("ISO-TP set_addressing updates IDs", "[uds][isotp]") {
    auto adapter = make_adapter();
    c_isotp_transport tp(adapter);
    tp.set_addressing(0x641, 0x642);

    std::array<byte_t, 1> data{0x3E};
    auto result = tp.send(byte_span_t{data});
    REQUIRE(result.has_value());

    auto history = adapter->get_tx_history();
    REQUIRE(history[0].id == 0x641);
}

TEST_CASE("ISO-TP config accessor", "[uds][isotp]") {
    c_isotp_config cfg{.tx_id = 0x123, .rx_id = 0x456, .block_size = 4, .st_min = 20};
    auto adapter = make_adapter();
    c_isotp_transport tp(adapter, cfg);

    auto& c = tp.config();
    REQUIRE(c.tx_id == 0x123);
    REQUIRE(c.rx_id == 0x456);
    REQUIRE(c.block_size == 4);
    REQUIRE(c.st_min == 20);
}

TEST_CASE("ISO-TP flow control overflow abort", "[uds][isotp]") {
    auto adapter = make_adapter();
    c_isotp_transport tp(adapter, c_isotp_config{.tx_id = 0x7DF, .rx_id = 0x7E8});

    // Inject FC with overflow
    c_can_frame fc{.id = 0x7E8, .dlc = 8};
    fc.data = {0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // 0x32 = FC + overflow
    adapter->inject_rx(fc);

    std::array<byte_t, 10> data{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    auto result = tp.send(byte_span_t{data});
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::protocol);
}
