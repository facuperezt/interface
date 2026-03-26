#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "interface/can/statistics.hpp"

using namespace interface;
using namespace interface::can;
using Catch::Matchers::WithinAbs;

namespace {

auto make_frame(can_id_t id, std::uint8_t dlc, timestamp_us_t ts) -> c_can_frame {
    c_can_frame f{};
    f.id = id;
    f.dlc = dlc;
    f.timestamp = ts;
    return f;
}

} // anonymous namespace

TEST_CASE("Statistics starts empty", "[can][statistics]") {
    c_bus_statistics stats;
    REQUIRE(stats.frame_count() == 0);
    REQUIRE(stats.frame_count(0x100) == 0);
    REQUIRE(stats.bus_load_percent() == 0.0);
}

TEST_CASE("Statistics counts total and per-ID frames", "[can][statistics]") {
    c_bus_statistics stats;

    stats.record(make_frame(0x100, 8, 1000));
    stats.record(make_frame(0x200, 4, 2000));
    stats.record(make_frame(0x100, 8, 3000));
    stats.record(make_frame(0x100, 8, 4000));

    REQUIRE(stats.frame_count() == 4);
    REQUIRE(stats.frame_count(0x100) == 3);
    REQUIRE(stats.frame_count(0x200) == 1);
    REQUIRE(stats.frame_count(0x300) == 0);
}

TEST_CASE("Statistics delta-time calculation", "[can][statistics]") {
    c_bus_statistics stats;

    // ID 0x100: timestamps 1000, 3000, 7000
    // Deltas: 2000, 4000
    // Min=2000, Max=4000, Avg=3000
    stats.record(make_frame(0x100, 8, 1000));
    stats.record(make_frame(0x100, 8, 3000));
    stats.record(make_frame(0x100, 8, 7000));

    auto ds = stats.delta_stats(0x100);
    REQUIRE(ds.min_us == 2000);
    REQUIRE(ds.max_us == 4000);
    REQUIRE(ds.avg_us == 3000);
}

TEST_CASE("Statistics delta-time with single frame returns zeros", "[can][statistics]") {
    c_bus_statistics stats;
    stats.record(make_frame(0x100, 8, 1000));

    auto ds = stats.delta_stats(0x100);
    REQUIRE(ds.min_us == 0);
    REQUIRE(ds.max_us == 0);
    REQUIRE(ds.avg_us == 0);
}

TEST_CASE("Statistics delta-time for unknown ID returns zeros", "[can][statistics]") {
    c_bus_statistics stats;
    auto ds = stats.delta_stats(0x999);
    REQUIRE(ds.min_us == 0);
    REQUIRE(ds.max_us == 0);
    REQUIRE(ds.avg_us == 0);
}

TEST_CASE("Statistics bus load estimation", "[can][statistics]") {
    // 500 kbit/s, send a bunch of 8-byte frames over 1 second
    c_bus_statistics stats(500'000);

    // Each 8-byte standard frame ~ (47 + 64) * 1.2 = ~133 bits
    // 100 frames over 1s -> ~13300 bits / 500000 = ~2.66%
    for (int i = 0; i < 100; ++i) {
        stats.record(make_frame(0x100, 8, static_cast<timestamp_us_t>(i) * 10000));
    }

    auto load = stats.bus_load_percent();
    REQUIRE(load > 0.0);
    REQUIRE(load < 100.0);
}

TEST_CASE("Statistics reset clears everything", "[can][statistics]") {
    c_bus_statistics stats;

    stats.record(make_frame(0x100, 8, 1000));
    stats.record(make_frame(0x100, 8, 2000));
    REQUIRE(stats.frame_count() == 2);

    stats.reset();
    REQUIRE(stats.frame_count() == 0);
    REQUIRE(stats.frame_count(0x100) == 0);

    auto ds = stats.delta_stats(0x100);
    REQUIRE(ds.min_us == 0);
}

TEST_CASE("Statistics sliding window prunes old frames", "[can][statistics]") {
    // Window of 5000 microseconds
    c_bus_statistics stats(500'000, 5000);

    stats.record(make_frame(0x100, 8, 1000));  // Will be pruned
    stats.record(make_frame(0x100, 8, 2000));  // Will be pruned
    stats.record(make_frame(0x100, 8, 7000));  // Within window (7000 - 5000 = 2000 cutoff)
    stats.record(make_frame(0x100, 8, 8000));  // Within window

    // Window is last 5000us from latest (8000). Cutoff = 3000.
    // Only frames at 7000 and 8000 survive.
    REQUIRE(stats.frame_count() == 2);
    REQUIRE(stats.frame_count(0x100) == 2);
}
