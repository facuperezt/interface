#include <catch2/catch_test_macros.hpp>
#include "interface/canopen/nmt.hpp"

using namespace interface::canopen;

TEST_CASE("NMT state to string", "[canopen][nmt]") {
    REQUIRE(nmt_state_to_string(e_nmt_state::operational) == "Operational");
    REQUIRE(nmt_state_to_string(e_nmt_state::pre_operational) == "Pre-operational");
    REQUIRE(nmt_state_to_string(e_nmt_state::stopped) == "Stopped");
    REQUIRE(nmt_state_to_string(e_nmt_state::initialising) == "Initialising");
}

TEST_CASE("NMT command to string", "[canopen][nmt]") {
    REQUIRE(nmt_command_to_string(e_nmt_command::start_remote_node) == "Start Remote Node");
    REQUIRE(nmt_command_to_string(e_nmt_command::reset_node) == "Reset Node");
}
