#include <catch2/catch_test_macros.hpp>
#include "interface/uds/client.hpp"

using namespace interface::uds;

TEST_CASE("nrc_to_string returns known names", "[uds][nrc]") {
    REQUIRE(nrc_to_string(e_nrc::general_reject) == "generalReject");
    REQUIRE(nrc_to_string(e_nrc::security_access_denied) == "securityAccessDenied");
    REQUIRE(nrc_to_string(e_nrc::invalid_key) == "invalidKey");
    REQUIRE(nrc_to_string(e_nrc::service_not_supported) == "serviceNotSupported");
}

TEST_CASE("c_uds_response data_hex formatting", "[uds][response]") {
    c_uds_response resp{
        .data = {0x01, 0xAB, 0xFF},
    };
    REQUIRE(resp.data_hex() == "01 AB FF");
}

TEST_CASE("c_uds_response empty data_hex", "[uds][response]") {
    c_uds_response resp{};
    REQUIRE(resp.data_hex().empty());
}
