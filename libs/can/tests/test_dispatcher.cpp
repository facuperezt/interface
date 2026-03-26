#include <catch2/catch_test_macros.hpp>
#include "interface/can/dispatcher.hpp"

using namespace interface;
using namespace interface::can;

namespace {

auto make_frame(can_id_t id) -> c_can_frame {
    c_can_frame f{};
    f.id = id;
    f.dlc = 2;
    f.data[0] = 0xAB;
    f.data[1] = 0xCD;
    return f;
}

} // anonymous namespace

TEST_CASE("Dispatcher subscribe to specific ID", "[can][dispatcher]") {
    c_dispatcher dispatcher;
    int call_count = 0;
    can_id_t received_id = 0;

    dispatcher.subscribe(0x100, [&](const c_can_frame& frame) {
        ++call_count;
        received_id = frame.id;
    });

    dispatcher.dispatch(make_frame(0x100));
    REQUIRE(call_count == 1);
    REQUIRE(received_id == 0x100);

    // Non-matching ID should not trigger callback
    dispatcher.dispatch(make_frame(0x200));
    REQUIRE(call_count == 1);
}

TEST_CASE("Dispatcher subscribe with filter", "[can][dispatcher]") {
    c_dispatcher dispatcher;
    int call_count = 0;

    // Match any ID in range 0x100 - 0x1FF
    dispatcher.subscribe(c_can_filter{.id = 0x100, .mask = 0x700}, [&](const c_can_frame&) {
        ++call_count;
    });

    dispatcher.dispatch(make_frame(0x100));
    dispatcher.dispatch(make_frame(0x1FF));
    dispatcher.dispatch(make_frame(0x200));  // Should not match

    REQUIRE(call_count == 2);
}

TEST_CASE("Dispatcher multiple subscribers", "[can][dispatcher]") {
    c_dispatcher dispatcher;
    int count_a = 0;
    int count_b = 0;

    dispatcher.subscribe(0x100, [&](const c_can_frame&) { ++count_a; });
    dispatcher.subscribe(0x100, [&](const c_can_frame&) { ++count_b; });

    dispatcher.dispatch(make_frame(0x100));
    REQUIRE(count_a == 1);
    REQUIRE(count_b == 1);
}

TEST_CASE("Dispatcher unsubscribe removes callback", "[can][dispatcher]") {
    c_dispatcher dispatcher;
    int call_count = 0;

    auto token = dispatcher.subscribe(0x100, [&](const c_can_frame&) {
        ++call_count;
    });

    dispatcher.dispatch(make_frame(0x100));
    REQUIRE(call_count == 1);

    dispatcher.unsubscribe(token);

    dispatcher.dispatch(make_frame(0x100));
    REQUIRE(call_count == 1); // Should not increment
}

TEST_CASE("Dispatcher accept-all filter", "[can][dispatcher]") {
    c_dispatcher dispatcher;
    int call_count = 0;

    dispatcher.subscribe(c_can_filter::accept_all(), [&](const c_can_frame&) {
        ++call_count;
    });

    dispatcher.dispatch(make_frame(0x000));
    dispatcher.dispatch(make_frame(0x100));
    dispatcher.dispatch(make_frame(0x7FF));

    REQUIRE(call_count == 3);
}

TEST_CASE("Dispatcher dispatch with no subscribers does nothing", "[can][dispatcher]") {
    c_dispatcher dispatcher;
    // Should not crash
    dispatcher.dispatch(make_frame(0x100));
}
