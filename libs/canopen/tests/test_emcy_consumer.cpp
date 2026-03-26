#include <catch2/catch_test_macros.hpp>
#include "interface/canopen/emcy_consumer.hpp"

using namespace interface;
using namespace interface::canopen;

namespace {

auto make_emcy_frame(node_id_t node_id, std::uint16_t error_code,
                     std::uint8_t error_register, timestamp_us_t ts) -> can::c_can_frame {
    can::c_can_frame f{};
    f.id = k_emcy_cob_base + node_id;
    f.dlc = 8;
    f.timestamp = ts;
    // Error code (little-endian)
    f.data[0] = static_cast<byte_t>(error_code & 0xFF);
    f.data[1] = static_cast<byte_t>((error_code >> 8) & 0xFF);
    // Error register
    f.data[2] = error_register;
    // Manufacturer data
    f.data[3] = 0x11;
    f.data[4] = 0x22;
    f.data[5] = 0x33;
    f.data[6] = 0x44;
    f.data[7] = 0x55;
    return f;
}

} // anonymous namespace

TEST_CASE("EMCY consumer initial state", "[canopen][emcy]") {
    c_emcy_consumer consumer;
    auto h = consumer.history(1);
    REQUIRE(h.empty());
}

TEST_CASE("EMCY consumer parses emergency frames", "[canopen][emcy]") {
    c_emcy_consumer consumer;

    consumer.process_frame(make_emcy_frame(1, 0x1000, 0x01, 100));

    auto h = consumer.history(1);
    REQUIRE(h.size() == 1);
    REQUIRE(h[0].node == 1);
    REQUIRE(h[0].error_code == 0x1000);
    REQUIRE(h[0].error_register == 0x01);
    REQUIRE(h[0].mfr_data[0] == 0x11);
    REQUIRE(h[0].mfr_data[4] == 0x55);
    REQUIRE(h[0].timestamp == 100);
}

TEST_CASE("EMCY consumer multiple events per node", "[canopen][emcy]") {
    c_emcy_consumer consumer;

    consumer.process_frame(make_emcy_frame(1, 0x1000, 0x01, 100));
    consumer.process_frame(make_emcy_frame(1, 0x2000, 0x02, 200));
    consumer.process_frame(make_emcy_frame(1, 0x3000, 0x04, 300));

    auto h = consumer.history(1);
    REQUIRE(h.size() == 3);
    REQUIRE(h[0].error_code == 0x1000);
    REQUIRE(h[1].error_code == 0x2000);
    REQUIRE(h[2].error_code == 0x3000);
}

TEST_CASE("EMCY consumer multiple nodes", "[canopen][emcy]") {
    c_emcy_consumer consumer;

    consumer.process_frame(make_emcy_frame(1, 0x1000, 0x01, 100));
    consumer.process_frame(make_emcy_frame(2, 0x2000, 0x02, 200));

    REQUIRE(consumer.history(1).size() == 1);
    REQUIRE(consumer.history(2).size() == 1);
    REQUIRE(consumer.history(3).empty());
}

TEST_CASE("EMCY consumer callback fires on event", "[canopen][emcy]") {
    c_emcy_consumer consumer;
    int callback_count = 0;
    std::uint16_t last_error_code = 0;

    consumer.set_callback([&](const c_emcy_event& event) {
        ++callback_count;
        last_error_code = event.error_code;
    });

    consumer.process_frame(make_emcy_frame(1, 0x5000, 0x10, 100));
    REQUIRE(callback_count == 1);
    REQUIRE(last_error_code == 0x5000);

    consumer.process_frame(make_emcy_frame(2, 0x6000, 0x20, 200));
    REQUIRE(callback_count == 2);
    REQUIRE(last_error_code == 0x6000);
}

TEST_CASE("EMCY consumer ignores non-EMCY frames", "[canopen][emcy]") {
    c_emcy_consumer consumer;

    // Regular data frame
    can::c_can_frame f{};
    f.id = 0x100;
    f.dlc = 8;
    f.timestamp = 100;
    consumer.process_frame(f);

    // Heartbeat frame (0x700 range)
    f.id = 0x701;
    consumer.process_frame(f);

    REQUIRE(consumer.history(1).empty());
}

TEST_CASE("EMCY consumer ignores short EMCY frames", "[canopen][emcy]") {
    c_emcy_consumer consumer;

    can::c_can_frame f{};
    f.id = k_emcy_cob_base + 1; // Valid EMCY COB-ID
    f.dlc = 4; // Too short (need 8)
    f.timestamp = 100;
    consumer.process_frame(f);

    REQUIRE(consumer.history(1).empty());
}

TEST_CASE("EMCY consumer clear_history clears all", "[canopen][emcy]") {
    c_emcy_consumer consumer;

    consumer.process_frame(make_emcy_frame(1, 0x1000, 0x01, 100));
    consumer.process_frame(make_emcy_frame(2, 0x2000, 0x02, 200));

    consumer.clear_history();
    REQUIRE(consumer.history(1).empty());
    REQUIRE(consumer.history(2).empty());
}

TEST_CASE("EMCY consumer clear_history for specific node", "[canopen][emcy]") {
    c_emcy_consumer consumer;

    consumer.process_frame(make_emcy_frame(1, 0x1000, 0x01, 100));
    consumer.process_frame(make_emcy_frame(2, 0x2000, 0x02, 200));

    consumer.clear_history(1);
    REQUIRE(consumer.history(1).empty());
    REQUIRE(consumer.history(2).size() == 1);
}
