#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <array>
#include "interface/can_db/i_database_parser.hpp"

using namespace interface;
using namespace interface::can_db;
using Catch::Matchers::WithinRel;

TEST_CASE("Decode unsigned little-endian signal", "[can_db][decoder]") {
    c_signal_def sig{
        .name = "RPM",
        .start_bit = 0,
        .length = 16,
        .byte_order = e_byte_order::little_endian,
        .value_type = e_value_type::unsigned_int,
        .factor = 0.25,
        .offset = 0.0,
    };

    // Raw value: 0x0400 = 1024 → physical = 1024 * 0.25 = 256.0
    std::array<byte_t, 8> data{0x00, 0x04, 0, 0, 0, 0, 0, 0};
    auto result = c_signal_decoder::decode(sig, byte_span_t{data});

    REQUIRE_THAT(result, WithinRel(256.0, 0.001));
}

TEST_CASE("Decode signed little-endian signal", "[can_db][decoder]") {
    c_signal_def sig{
        .name = "Temperature",
        .start_bit = 0,
        .length = 8,
        .byte_order = e_byte_order::little_endian,
        .value_type = e_value_type::signed_int,
        .factor = 1.0,
        .offset = -40.0,
    };

    // Raw value: 0xD0 = -48 (signed) → physical = -48 * 1 + (-40) = -88
    std::array<byte_t, 8> data{0xD0, 0, 0, 0, 0, 0, 0, 0};
    auto result = c_signal_decoder::decode(sig, byte_span_t{data});

    REQUIRE_THAT(result, WithinRel(-88.0, 0.001));
}

TEST_CASE("Encode unsigned little-endian signal", "[can_db][decoder]") {
    c_signal_def sig{
        .name = "RPM",
        .start_bit = 0,
        .length = 16,
        .byte_order = e_byte_order::little_endian,
        .value_type = e_value_type::unsigned_int,
        .factor = 0.25,
        .offset = 0.0,
    };

    std::array<byte_t, 8> data{};
    auto result = c_signal_decoder::encode(sig, 256.0, mutable_byte_span_t{data});

    REQUIRE(result.has_value());
    REQUIRE(data[0] == 0x00);
    REQUIRE(data[1] == 0x04);
}

TEST_CASE("Decode zero-length signal returns 0", "[can_db][decoder]") {
    c_signal_def sig{.length = 0};
    std::array<byte_t, 8> data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    REQUIRE(c_signal_decoder::decode(sig, byte_span_t{data}) == 0.0);
}
