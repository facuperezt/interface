#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "interface/can_trace/c_asc_writer.hpp"
#include "interface/can_trace/c_asc_reader.hpp"

#include <filesystem>

using namespace interface;
using namespace interface::can_trace;
using Catch::Matchers::WithinAbs;

namespace {

auto make_frame(can_id_t id, std::uint8_t dlc, timestamp_us_t ts,
                std::initializer_list<byte_t> bytes) -> can::c_can_frame {
    can::c_can_frame f{};
    f.id = id;
    f.dlc = dlc;
    f.timestamp = ts;
    std::size_t i = 0;
    for (auto b : bytes) {
        if (i < f.data.size()) {
            f.data[i++] = b;
        }
    }
    return f;
}

} // anonymous namespace

TEST_CASE("ASC writer fails when not opened", "[can_trace][asc_writer]") {
    c_asc_writer writer;
    auto f = make_frame(0x100, 2, 1000, {0xAB, 0xCD});
    auto result = writer.write(f);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ASC writer round-trip with reader", "[can_trace][asc_writer]") {
    auto path = std::filesystem::temp_directory_path() / "test_asc_roundtrip.asc";

    auto f1 = make_frame(0x100, 8, 100000, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    auto f2 = make_frame(0x200, 4, 200000, {0xAA, 0xBB, 0xCC, 0xDD});
    auto f3 = make_frame(0x1FF, 2, 500000, {0xFE, 0xED});

    // Write frames — scope writer to close before reader opens
    {
        c_asc_writer writer;
        REQUIRE(writer.open(path).has_value());
        REQUIRE(writer.write(f1).has_value());
        REQUIRE(writer.write(f2).has_value());
        REQUIRE(writer.write(f3).has_value());
        writer.close();
    }

    // Read back and verify — scope reader before deleting temp file
    {
        c_asc_reader reader;
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 3);

        auto& r1 = result->at(0);
        REQUIRE(r1.id == 0x100);
        REQUIRE(r1.dlc == 8);
        REQUIRE(r1.data[0] == 0x01);
        REQUIRE(r1.data[7] == 0x08);
        REQUIRE_THAT(static_cast<double>(r1.timestamp) / 1'000'000.0,
                     WithinAbs(0.1, 0.001));

        auto& r2 = result->at(1);
        REQUIRE(r2.id == 0x200);
        REQUIRE(r2.dlc == 4);
        REQUIRE(r2.data[0] == 0xAA);

        auto& r3 = result->at(2);
        REQUIRE(r3.id == 0x1FF);
        REQUIRE(r3.dlc == 2);
    }

    std::filesystem::remove(path);
}

TEST_CASE("ASC writer extended ID round-trip", "[can_trace][asc_writer]") {
    auto path = std::filesystem::temp_directory_path() / "test_asc_ext_roundtrip.asc";

    can::c_can_frame f{};
    f.id = 0x1ABCDEF0;
    f.dlc = 2;
    f.timestamp = 1000;
    f.data[0] = 0x01;
    f.data[1] = 0x02;
    f.flags.extended = true;

    {
        c_asc_writer writer;
        REQUIRE(writer.open(path).has_value());
        REQUIRE(writer.write(f).has_value());
        writer.close();
    }

    {
        c_asc_reader reader;
        REQUIRE(reader.open(path).has_value());
        auto r = reader.read_next();
        REQUIRE(r.has_value());
        REQUIRE(r->has_value());
        REQUIRE(r->value().id == 0x1ABCDEF0);
        REQUIRE(r->value().flags.extended);
    }

    std::filesystem::remove(path);
}
