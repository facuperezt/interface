#include <catch2/catch_test_macros.hpp>
#include "interface/can/filter.hpp"

using namespace interface::can;

TEST_CASE("accept_all filter matches any ID", "[can][filter]") {
    auto filter = c_can_filter::accept_all();
    REQUIRE(filter.matches(0x000));
    REQUIRE(filter.matches(0x7FF));
    REQUIRE(filter.matches(0x1FFFFFFF));
}

TEST_CASE("exact filter matches only the target ID", "[can][filter]") {
    auto filter = c_can_filter::exact(0x123);
    REQUIRE(filter.matches(0x123));
    REQUIRE_FALSE(filter.matches(0x124));
    REQUIRE_FALSE(filter.matches(0x000));
}

TEST_CASE("custom mask filter", "[can][filter]") {
    // Match any ID where bits 10:8 are 0b001 (i.e., 0x1xx)
    c_can_filter filter{.id = 0x100, .mask = 0x700};

    REQUIRE(filter.matches(0x100));
    REQUIRE(filter.matches(0x1FF));
    REQUIRE(filter.matches(0x123));
    REQUIRE_FALSE(filter.matches(0x200));
    REQUIRE_FALSE(filter.matches(0x000));
}
