#include <catch2/catch_test_macros.hpp>
#include "interface/can/frame.hpp"

using namespace interface::can;

TEST_CASE("c_can_frame default construction", "[can][frame]") {
    c_can_frame frame{};
    REQUIRE(frame.id == 0);
    REQUIRE(frame.dlc == 0);
    REQUIRE(frame.data_length() == 0);
    REQUIRE(frame.payload().empty());
}

TEST_CASE("c_can_frame data_length clamps to max DLC", "[can][frame]") {
    c_can_frame frame{.dlc = 15};
    REQUIRE(frame.data_length() == k_can_max_dlc);
}

TEST_CASE("c_can_frame payload returns correct span", "[can][frame]") {
    c_can_frame frame{
        .id = 0x123,
        .dlc = 3,
        .data = {0xAA, 0xBB, 0xCC},
    };

    auto p = frame.payload();
    REQUIRE(p.size() == 3);
    REQUIRE(p[0] == 0xAA);
    REQUIRE(p[2] == 0xCC);
}

TEST_CASE("c_can_frame format produces hex string", "[can][frame]") {
    c_can_frame frame{
        .id = 0x7DF,
        .dlc = 2,
        .data = {0x01, 0x00},
    };

    auto s = frame.format();
    REQUIRE(s.contains("7df"));  // hex id
    REQUIRE(s.contains("[2]")); // dlc
    REQUIRE(s.contains("01"));  // data byte
}

TEST_CASE("c_canfd_frame DLC-to-length mapping", "[can][frame]") {
    c_canfd_frame frame{};

    frame.dlc = 8;
    REQUIRE(frame.data_length() == 8);

    frame.dlc = 9;
    REQUIRE(frame.data_length() == 12);

    frame.dlc = 13;
    REQUIRE(frame.data_length() == 32);

    frame.dlc = 15;
    REQUIRE(frame.data_length() == 64);
}
