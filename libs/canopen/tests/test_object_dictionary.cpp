#include <catch2/catch_test_macros.hpp>
#include "interface/canopen/object_dictionary.hpp"

using namespace interface::canopen;

TEST_CASE("Object Dictionary add and find entry", "[canopen][od]") {
    c_object_dictionary od;
    REQUIRE(od.size() == 0);

    od.add_entry(c_od_entry{
        .index = 0x1000,
        .name = "Device Type",
        .object_type = e_od_object_type::variable,
        .sub_entries = {
            c_od_sub_entry{
                .sub_index = 0x00,
                .name = "Device Type",
                .data_type = e_od_data_type::unsigned32,
                .access = e_od_access::read_only,
                .default_value = std::uint32_t{0x000F0191},
            },
        },
    });

    REQUIRE(od.size() == 1);

    auto found = od.find(0x1000);
    REQUIRE(found.has_value());
    REQUIRE(found->get().name == "Device Type");
    REQUIRE(found->get().sub_entries.size() == 1);
}

TEST_CASE("Object Dictionary find returns nullopt for missing index", "[canopen][od]") {
    c_object_dictionary od;
    REQUIRE_FALSE(od.find(0x2000).has_value());
}

TEST_CASE("OD entry find sub-entry", "[canopen][od]") {
    c_od_entry entry{
        .index = 0x1018,
        .name = "Identity Object",
        .object_type = e_od_object_type::record,
        .sub_entries = {
            c_od_sub_entry{.sub_index = 0, .name = "Number of Entries"},
            c_od_sub_entry{.sub_index = 1, .name = "Vendor ID"},
            c_od_sub_entry{.sub_index = 2, .name = "Product Code"},
        },
    };

    auto sub = entry.find_sub(1);
    REQUIRE(sub.has_value());
    REQUIRE(sub->get().name == "Vendor ID");

    REQUIRE_FALSE(entry.find_sub(99).has_value());
}

TEST_CASE("Object Dictionary clear", "[canopen][od]") {
    c_object_dictionary od;
    od.add_entry(c_od_entry{.index = 0x1000, .name = "A"});
    od.add_entry(c_od_entry{.index = 0x1001, .name = "B"});
    REQUIRE(od.size() == 2);

    od.clear();
    REQUIRE(od.size() == 0);
}

TEST_CASE("Object Dictionary mutable find", "[canopen][od]") {
    c_object_dictionary od;
    od.add_entry(c_od_entry{
        .index = 0x2000,
        .name = "Test",
        .sub_entries = {
            c_od_sub_entry{
                .sub_index = 0,
                .name = "Value",
                .access = e_od_access::read_write,
            },
        },
    });

    auto entry = od.find_mut(0x2000);
    REQUIRE(entry.has_value());

    auto sub = entry->get().find_sub_mut(0);
    REQUIRE(sub.has_value());
    sub->get().current_value = std::uint32_t{42};

    // Verify mutation persisted
    auto check = od.find(0x2000)->get().find_sub(0)->get().current_value;
    REQUIRE(check.has_value());
    REQUIRE(std::get<std::uint32_t>(*check) == 42);
}
