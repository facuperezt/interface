#include <catch2/catch_test_macros.hpp>
#include "interface/canopen/pdo.hpp"

using namespace interface;
using namespace interface::can;
using namespace interface::canopen;

TEST_CASE("PDO config total bits and bytes", "[canopen][pdo]") {
    c_pdo_config config{
        .mapping = {
            c_pdo_map_entry{.index = 0x6000, .sub_index = 1, .bit_length = 16},
            c_pdo_map_entry{.index = 0x6000, .sub_index = 2, .bit_length = 8},
        },
    };

    REQUIRE(config.total_bits() == 24);
    REQUIRE(config.total_bytes() == 3);
}

TEST_CASE("PDO decode extracts values", "[canopen][pdo]") {
    // Set up an OD with entries
    c_object_dictionary od;
    od.add_entry(c_od_entry{
        .index = 0x6000,
        .name = "Digital Inputs",
        .sub_entries = {
            c_od_sub_entry{.sub_index = 1, .name = "Input 1"},
            c_od_sub_entry{.sub_index = 2, .name = "Input 2"},
        },
    });

    c_pdo_config config{
        .enabled = true,
        .cob_id = 0x181,
        .mapping = {
            c_pdo_map_entry{.index = 0x6000, .sub_index = 1, .bit_length = 8},
            c_pdo_map_entry{.index = 0x6000, .sub_index = 2, .bit_length = 8},
        },
    };

    c_can_frame frame{.id = 0x181, .dlc = 2, .data = {0x42, 0xFF}};

    auto result = decode_pdo(frame, config, od);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 2);
    REQUIRE(result->at(0).name == "Input 1");
    REQUIRE(result->at(0).value == 0x42);
    REQUIRE(result->at(1).name == "Input 2");
    REQUIRE(result->at(1).value == 0xFF);
}

TEST_CASE("PDO decode fails when disabled", "[canopen][pdo]") {
    c_pdo_config config{.enabled = false};
    c_can_frame frame{};
    c_object_dictionary od;

    auto result = decode_pdo(frame, config, od);
    REQUIRE_FALSE(result.has_value());
}
