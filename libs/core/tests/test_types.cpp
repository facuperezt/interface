#include <catch2/catch_test_macros.hpp>
#include "interface/core/types.hpp"

using namespace interface;

TEST_CASE("byte_t is 8-bit unsigned", "[core][types]") {
    static_assert(sizeof(byte_t) == 1);
    static_assert(std::is_unsigned_v<byte_t>);
}

TEST_CASE("can_id_t is at least 29 bits", "[core][types]") {
    static_assert(sizeof(can_id_t) >= 4);
    constexpr can_id_t extended_max = 0x1FFF'FFFF;
    REQUIRE(extended_max == 536870911);
}

TEST_CASE("byte_buffer_t is resizable", "[core][types]") {
    byte_buffer_t buf{0x01, 0x02, 0x03};
    REQUIRE(buf.size() == 3);
    buf.push_back(0x04);
    REQUIRE(buf.size() == 4);
}

TEST_CASE("byte_span_t provides non-owning view", "[core][types]") {
    byte_buffer_t buf{0xDE, 0xAD, 0xBE, 0xEF};
    byte_span_t span{buf};

    REQUIRE(span.size() == 4);
    REQUIRE(span[0] == 0xDE);
    REQUIRE(span[3] == 0xEF);
}
