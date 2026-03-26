#include <catch2/catch_test_macros.hpp>
#include "interface/can_trace/i_trace_reader.hpp"

using namespace interface::can_trace;

TEST_CASE("c_trace_info default construction", "[can_trace]") {
    c_trace_info info{};
    REQUIRE(info.format.empty());
    REQUIRE(info.frame_count == 0);
    REQUIRE(info.start_time == 0);
}

// TODO: Add tests for ASC, BLF, CSV readers once implemented.
