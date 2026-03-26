#include <catch2/catch_test_macros.hpp>
#include "interface/can_db/i_database_parser.hpp"

using namespace interface::can_db;

TEST_CASE("c_database find_message by ID", "[can_db][database]") {
    c_database db{
        .name = "test_db",
        .messages = {
            c_message_def{.id = 0x100, .name = "EngineRPM", .dlc = 8},
            c_message_def{.id = 0x200, .name = "VehicleSpeed", .dlc = 8},
        },
    };

    auto found = db.find_message(0x100);
    REQUIRE(found.has_value());
    REQUIRE(found->get().name == "EngineRPM");

    auto not_found = db.find_message(0x999);
    REQUIRE_FALSE(not_found.has_value());
}

TEST_CASE("c_database empty database returns nullopt", "[can_db][database]") {
    c_database db{};
    REQUIRE_FALSE(db.find_message(0x100).has_value());
}
