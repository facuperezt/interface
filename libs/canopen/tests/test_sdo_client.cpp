#include <catch2/catch_test_macros.hpp>
#include "interface/canopen/sdo_client.hpp"
#include "interface/can_hal/c_mock_adapter.hpp"

using namespace interface;
using namespace interface::can;
using namespace interface::can_hal;
using namespace interface::canopen;

namespace {

auto make_adapter() -> std::shared_ptr<c_mock_adapter> {
    auto adapter = std::make_shared<c_mock_adapter>();
    adapter->open(c_bitrate_config{});
    return adapter;
}

/// Build an SDO response frame.
auto make_sdo_response(node_id_t node_id, std::array<byte_t, 8> data) -> c_can_frame {
    c_can_frame frame{};
    frame.id = static_cast<can_id_t>(0x580 + node_id);
    frame.dlc = 8;
    frame.data = data;
    return frame;
}

} // anonymous namespace

TEST_CASE("SDO expedited upload (<=4 bytes)", "[canopen][sdo]") {
    auto adapter = make_adapter();

    // Expedited upload response: cs=0x4F (expedited, size indicated, 1 byte data)
    // cs = 0x40 | 0x02 (e=1) | 0x01 (s=1) | (3<<2 = unused bytes)
    // 4 - n = data bytes, n = 3, so 1 data byte
    auto response = make_sdo_response(1, {0x4F, 0x00, 0x10, 0x00, 0x42, 0x00, 0x00, 0x00});
    adapter->inject_rx(response);

    c_sdo_client client(adapter, c_sdo_config{.node_id = 1});
    auto result = client.upload(0x1000, 0x00);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 1);
    REQUIRE(result->at(0) == 0x42);

    // Verify the request was sent correctly
    auto history = adapter->get_tx_history();
    REQUIRE(history.size() == 1);
    REQUIRE(history[0].id == 0x601); // 0x600 + node_id
    REQUIRE(history[0].data[0] == 0x40); // Initiate upload request
    REQUIRE(history[0].data[1] == 0x00); // Index low
    REQUIRE(history[0].data[2] == 0x10); // Index high
    REQUIRE(history[0].data[3] == 0x00); // Sub-index
}

TEST_CASE("SDO expedited upload (4 bytes)", "[canopen][sdo]") {
    auto adapter = make_adapter();

    // cs=0x43: expedited, size indicated, 0 unused bytes (4 data bytes)
    auto response = make_sdo_response(1, {0x43, 0x00, 0x10, 0x00, 0x91, 0x01, 0x0F, 0x00});
    adapter->inject_rx(response);

    c_sdo_client client(adapter, c_sdo_config{.node_id = 1});
    auto result = client.upload(0x1000, 0x00);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 4);
    REQUIRE(result->at(0) == 0x91);
    REQUIRE(result->at(3) == 0x00);
}

TEST_CASE("SDO expedited download (<=4 bytes)", "[canopen][sdo]") {
    auto adapter = make_adapter();

    // Download confirmation response
    auto response = make_sdo_response(1, {0x60, 0x00, 0x20, 0x01, 0x00, 0x00, 0x00, 0x00});
    adapter->inject_rx(response);

    c_sdo_client client(adapter, c_sdo_config{.node_id = 1});
    std::array<byte_t, 2> data{0xAB, 0xCD};
    auto result = client.download(0x2000, 0x01, byte_span_t{data});
    REQUIRE(result.has_value());

    auto history = adapter->get_tx_history();
    REQUIRE(history.size() == 1);
    REQUIRE(history[0].data[0] == 0x2B); // Expedited, size indicated, n=2 (4-2=2 unused)
    REQUIRE(history[0].data[4] == 0xAB);
    REQUIRE(history[0].data[5] == 0xCD);
}

TEST_CASE("SDO segmented upload (>4 bytes)", "[canopen][sdo]") {
    auto adapter = make_adapter();

    // Response 1: Initiate upload response, not expedited, size=8 bytes
    adapter->inject_rx(make_sdo_response(1, {0x41, 0x00, 0x20, 0x00, 0x08, 0x00, 0x00, 0x00}));
    // Response 2: Upload segment, toggle=0, not last, 7 bytes
    adapter->inject_rx(make_sdo_response(1, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}));
    // Response 3: Upload segment, toggle=1, last, 1 byte (n=6, last=1)
    adapter->inject_rx(make_sdo_response(1, {0x1D, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));

    c_sdo_client client(adapter, c_sdo_config{.node_id = 1});
    auto result = client.upload(0x2000, 0x00);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 8);
    REQUIRE(result->at(0) == 0x01);
    REQUIRE(result->at(6) == 0x07);
    REQUIRE(result->at(7) == 0x08);
}

TEST_CASE("SDO segmented download (>4 bytes)", "[canopen][sdo]") {
    auto adapter = make_adapter();

    // Response 1: Initiate download confirmation
    adapter->inject_rx(make_sdo_response(1, {0x60, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00}));
    // Response 2: Segment download confirmation, toggle=0
    adapter->inject_rx(make_sdo_response(1, {0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
    // Response 3: Segment download confirmation, toggle=1
    adapter->inject_rx(make_sdo_response(1, {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));

    c_sdo_client client(adapter, c_sdo_config{.node_id = 1});
    std::array<byte_t, 10> data{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    auto result = client.download(0x2000, 0x00, byte_span_t{data});
    REQUIRE(result.has_value());
}

TEST_CASE("SDO abort response", "[canopen][sdo]") {
    auto adapter = make_adapter();

    // Abort transfer response: object not found (0x06020000)
    adapter->inject_rx(make_sdo_response(1, {0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x02, 0x06}));

    c_sdo_client client(adapter, c_sdo_config{.node_id = 1});
    auto result = client.upload(0x1000, 0x00);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::protocol);
}

TEST_CASE("SDO client fails when adapter not open", "[canopen][sdo]") {
    auto adapter = std::make_shared<c_mock_adapter>(); // NOT opened
    c_sdo_client client(adapter);
    auto result = client.upload(0x1000, 0x00);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::hardware);
}

TEST_CASE("SDO client timeout", "[canopen][sdo]") {
    auto adapter = make_adapter();
    // No response injected
    c_sdo_client client(adapter, c_sdo_config{.node_id = 1, .timeout = std::chrono::milliseconds{20}});
    auto result = client.upload(0x1000, 0x00);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::timeout);
}

TEST_CASE("SDO set_node_id changes COB-IDs", "[canopen][sdo]") {
    auto adapter = make_adapter();

    // Response for node 5
    adapter->inject_rx(make_sdo_response(5, {0x4F, 0x00, 0x10, 0x00, 0x42, 0x00, 0x00, 0x00}));

    c_sdo_client client(adapter, c_sdo_config{.node_id = 1});
    client.set_node_id(5);

    auto result = client.upload(0x1000, 0x00);
    REQUIRE(result.has_value());

    auto history = adapter->get_tx_history();
    REQUIRE(history[0].id == 0x605); // 0x600 + 5
}
