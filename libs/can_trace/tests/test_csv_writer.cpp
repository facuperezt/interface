#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "interface/can_trace/c_csv_writer.hpp"
#include "interface/can_trace/c_csv_reader.hpp"

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

TEST_CASE("CSV writer fails when not opened", "[can_trace][csv_writer]") {
    c_csv_writer writer;
    auto f = make_frame(0x100, 2, 1000, {0xAB, 0xCD});
    auto result = writer.write(f);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("CSV writer round-trip with reader (default config)", "[can_trace][csv_writer]") {
    auto path = std::filesystem::temp_directory_path() / "test_csv_roundtrip.csv";

    auto f1 = make_frame(0x100, 8, 100000, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08});
    auto f2 = make_frame(0x200, 4, 200000, {0xAA, 0xBB, 0xCC, 0xDD});

    {
        c_csv_writer writer;
        REQUIRE(writer.open(path).has_value());
        REQUIRE(writer.write(f1).has_value());
        REQUIRE(writer.write(f2).has_value());
        writer.close();
    }

    {
        c_csv_reader reader;
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 2);

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
    }

    std::filesystem::remove(path);
}

TEST_CASE("CSV writer with semicolon delimiter", "[can_trace][csv_writer]") {
    auto path = std::filesystem::temp_directory_path() / "test_csv_semicolon.csv";

    c_csv_column_config config;
    config.delimiter = ';';

    auto f = make_frame(0x100, 2, 50000, {0xDE, 0xAD});

    {
        c_csv_writer writer(config);
        REQUIRE(writer.open(path).has_value());
        REQUIRE(writer.write(f).has_value());
        writer.close();
    }

    {
        c_csv_reader reader(config);
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 1);
        REQUIRE(result->at(0).id == 0x100);
        REQUIRE(result->at(0).data[0] == 0xDE);
        REQUIRE(result->at(0).data[1] == 0xAD);
    }

    std::filesystem::remove(path);
}
