#include <catch2/catch_test_macros.hpp>
#include "interface/uds/client.hpp"
#include "interface/can_hal/c_mock_adapter.hpp"

using namespace interface;
using namespace interface::uds;
using namespace interface::can;
using namespace interface::can_hal;

namespace {

/// Helper: create a mock adapter with a pre-injected positive response.
auto make_mock_with_response(byte_buffer_t response_payload)
    -> std::shared_ptr<c_mock_adapter>
{
    auto adapter = std::make_shared<c_mock_adapter>();
    adapter->open(c_bitrate_config{});

    // Build a single-frame ISO-TP response
    c_can_frame rx_frame{.id = 0x7E8, .dlc = 8};
    rx_frame.data[0] = static_cast<byte_t>(response_payload.size()); // SF PCI
    for (std::size_t i = 0; i < response_payload.size() && i < 7; ++i) {
        rx_frame.data[i + 1] = response_payload[i];
    }
    adapter->inject_rx(rx_frame);

    return adapter;
}

} // anonymous namespace

TEST_CASE("UDS client initial session is default", "[uds][client]") {
    auto adapter = std::make_shared<c_mock_adapter>();
    adapter->open(c_bitrate_config{});
    c_uds_client client{adapter};

    REQUIRE(client.current_session() == e_session::default_session);
}

TEST_CASE("UDS DiagnosticSessionControl positive response", "[uds][client]") {
    // Positive response: 0x50 (0x10 + 0x40), 0x03 (extended)
    auto adapter = make_mock_with_response({0x50, 0x03});
    c_uds_client client{adapter};

    auto result = client.diagnostic_session_control(e_session::extended_diagnostic);
    REQUIRE(result.has_value());
    REQUIRE(result->positive);
    REQUIRE(client.current_session() == e_session::extended_diagnostic);
}

TEST_CASE("UDS TesterPresent positive response", "[uds][client]") {
    auto adapter = make_mock_with_response({0x7E, 0x00});
    c_uds_client client{adapter};

    auto result = client.tester_present(false);
    REQUIRE(result.has_value());
    REQUIRE(result->positive);
}

TEST_CASE("UDS negative response parsed correctly", "[uds][client]") {
    // Negative response: 0x7F, SID=0x22, NRC=0x31 (requestOutOfRange)
    auto adapter = make_mock_with_response({0x7F, 0x22, 0x31});
    c_uds_client client{adapter};

    auto result = client.read_data_by_identifier(0xF190);
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->positive);
    REQUIRE(result->nrc.has_value());
    REQUIRE(*result->nrc == e_nrc::request_out_of_range);
}

TEST_CASE("UDS ReadDataByIdentifier positive response", "[uds][client]") {
    // Positive response: 0x62, DID_HI, DID_LO, data...
    auto adapter = make_mock_with_response({0x62, 0xF1, 0x90, 0x41, 0x42});
    c_uds_client client{adapter};

    auto result = client.read_data_by_identifier(0xF190);
    REQUIRE(result.has_value());
    REQUIRE(result->positive);
    REQUIRE(result->service_id == sid::k_read_data_by_identifier);
}

TEST_CASE("UDS client fails when adapter not open", "[uds][client]") {
    auto adapter = std::make_shared<c_mock_adapter>(); // not opened
    c_uds_client client{adapter};

    auto result = client.tester_present();
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::hardware);
}

TEST_CASE("UDS client timeout when no response", "[uds][client]") {
    auto adapter = std::make_shared<c_mock_adapter>();
    adapter->open(c_bitrate_config{});
    // No response injected

    c_uds_client_config cfg{.p2_timeout = std::chrono::milliseconds{20}};
    c_uds_client client{adapter, cfg};

    auto result = client.tester_present();
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::timeout);
}
