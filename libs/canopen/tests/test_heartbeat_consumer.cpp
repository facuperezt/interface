#include <catch2/catch_test_macros.hpp>
#include "interface/canopen/heartbeat_consumer.hpp"

using namespace interface;
using namespace interface::canopen;

namespace {

auto make_heartbeat(node_id_t node_id, e_nmt_state state, timestamp_us_t ts) -> can::c_can_frame {
    can::c_can_frame f{};
    f.id = k_heartbeat_cob_base + node_id;
    f.dlc = 1;
    f.data[0] = static_cast<byte_t>(state);
    f.timestamp = ts;
    return f;
}

} // anonymous namespace

TEST_CASE("Heartbeat consumer initial state", "[canopen][heartbeat]") {
    c_heartbeat_consumer consumer(100'000); // 100ms timeout
    REQUIRE(consumer.node_state(1) == e_nmt_state::initialising);
    REQUIRE_FALSE(consumer.is_alive(1, 0));
}

TEST_CASE("Heartbeat consumer tracks node state", "[canopen][heartbeat]") {
    c_heartbeat_consumer consumer(100'000);
    consumer.monitor_node(1);

    consumer.process_frame(make_heartbeat(1, e_nmt_state::pre_operational, 1000));
    REQUIRE(consumer.node_state(1) == e_nmt_state::pre_operational);

    consumer.process_frame(make_heartbeat(1, e_nmt_state::operational, 2000));
    REQUIRE(consumer.node_state(1) == e_nmt_state::operational);
}

TEST_CASE("Heartbeat consumer is_alive checks timeout", "[canopen][heartbeat]") {
    c_heartbeat_consumer consumer(100'000); // 100ms timeout
    consumer.monitor_node(1);

    consumer.process_frame(make_heartbeat(1, e_nmt_state::operational, 1'000'000));

    // Within timeout
    REQUIRE(consumer.is_alive(1, 1'050'000));
    REQUIRE(consumer.is_alive(1, 1'100'000)); // Exactly at boundary

    // Beyond timeout
    REQUIRE_FALSE(consumer.is_alive(1, 1'100'001));
}

TEST_CASE("Heartbeat consumer ignores non-monitored nodes", "[canopen][heartbeat]") {
    c_heartbeat_consumer consumer(100'000);
    // Node 5 is not monitored
    consumer.process_frame(make_heartbeat(5, e_nmt_state::operational, 1000));
    REQUIRE(consumer.node_state(5) == e_nmt_state::initialising);
}

TEST_CASE("Heartbeat consumer ignores non-heartbeat frames", "[canopen][heartbeat]") {
    c_heartbeat_consumer consumer(100'000);
    consumer.monitor_node(1);

    // A regular data frame, not a heartbeat
    can::c_can_frame f{};
    f.id = 0x100;
    f.dlc = 8;
    f.timestamp = 1000;
    consumer.process_frame(f);

    REQUIRE(consumer.node_state(1) == e_nmt_state::initialising);
}

TEST_CASE("Heartbeat consumer state change callback", "[canopen][heartbeat]") {
    c_heartbeat_consumer consumer(100'000);
    consumer.monitor_node(1);

    int callback_count = 0;
    e_nmt_state last_old{};
    e_nmt_state last_new{};

    consumer.set_state_change_callback(
        [&](node_id_t, e_nmt_state old_s, e_nmt_state new_s) {
            ++callback_count;
            last_old = old_s;
            last_new = new_s;
        });

    consumer.process_frame(make_heartbeat(1, e_nmt_state::pre_operational, 1000));
    REQUIRE(callback_count == 1);
    REQUIRE(last_old == e_nmt_state::initialising);
    REQUIRE(last_new == e_nmt_state::pre_operational);

    // Same state — no callback
    consumer.process_frame(make_heartbeat(1, e_nmt_state::pre_operational, 2000));
    REQUIRE(callback_count == 1);

    // State change
    consumer.process_frame(make_heartbeat(1, e_nmt_state::operational, 3000));
    REQUIRE(callback_count == 2);
    REQUIRE(last_new == e_nmt_state::operational);
}

TEST_CASE("Heartbeat consumer timeout callback", "[canopen][heartbeat]") {
    c_heartbeat_consumer consumer(100'000); // 100ms timeout
    consumer.monitor_node(1);
    consumer.monitor_node(2);

    std::vector<node_id_t> timed_out_nodes;
    consumer.set_timeout_callback([&](node_id_t node) {
        timed_out_nodes.push_back(node);
    });

    consumer.process_frame(make_heartbeat(1, e_nmt_state::operational, 1'000'000));
    consumer.process_frame(make_heartbeat(2, e_nmt_state::operational, 1'000'000));

    // Check before timeout — nothing should fire
    consumer.check_timeouts(1'050'000);
    REQUIRE(timed_out_nodes.empty());

    // Check after timeout — both should fire
    consumer.check_timeouts(1'200'000);
    REQUIRE(timed_out_nodes.size() == 2);
}

TEST_CASE("Heartbeat consumer multiple nodes", "[canopen][heartbeat]") {
    c_heartbeat_consumer consumer(100'000);
    consumer.monitor_node(1);
    consumer.monitor_node(2);

    consumer.process_frame(make_heartbeat(1, e_nmt_state::operational, 1000));
    consumer.process_frame(make_heartbeat(2, e_nmt_state::stopped, 2000));

    REQUIRE(consumer.node_state(1) == e_nmt_state::operational);
    REQUIRE(consumer.node_state(2) == e_nmt_state::stopped);
}
