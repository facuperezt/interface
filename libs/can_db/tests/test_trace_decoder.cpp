#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "interface/can_db/c_trace_decoder.hpp"
#include "interface/can_trace/c_asc_reader.hpp"

#include <filesystem>
#include <fstream>

using namespace interface;
using namespace interface::can_db;
using namespace interface::can_trace;
using Catch::Matchers::WithinRel;

namespace {

auto write_temp_file(const std::string& name, const std::string& content) -> std::filesystem::path {
    auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream f(path);
    f << content;
    return path;
}

auto make_test_database() -> c_database {
    return c_database{
        .name = "test_db",
        .messages = {
            c_message_def{
                .id = 0x100,
                .name = "EngineData",
                .dlc = 8,
                .signals = {
                    c_signal_def{
                        .name = "RPM",
                        .start_bit = 0,
                        .length = 16,
                        .byte_order = e_byte_order::little_endian,
                        .value_type = e_value_type::unsigned_int,
                        .factor = 0.25,
                        .offset = 0.0,
                        .unit = "rpm",
                    },
                    c_signal_def{
                        .name = "Temp",
                        .start_bit = 16,
                        .length = 8,
                        .byte_order = e_byte_order::little_endian,
                        .value_type = e_value_type::signed_int,
                        .factor = 1.0,
                        .offset = -40.0,
                        .unit = "C",
                    },
                },
            },
        },
    };
}

} // anonymous namespace

TEST_CASE("Decode frame with known message", "[can_db][decoder]") {
    auto db = make_test_database();
    c_trace_decoder decoder(std::move(db));

    // RPM raw = 0x0400 = 1024, physical = 1024 * 0.25 = 256.0
    // Temp raw byte = 0x50 = 80 (unsigned), signed 80, physical = 80 * 1 + (-40) = 40.0
    can::c_can_frame frame{
        .id = 0x100,
        .dlc = 8,
        .data = {0x00, 0x04, 0x50, 0, 0, 0, 0, 0},
    };

    auto result = decoder.decode_frame(frame);

    REQUIRE(result.known == true);
    REQUIRE(result.message_name == "EngineData");
    REQUIRE(result.signals.size() == 2);

    REQUIRE(result.signals[0].name == "RPM");
    REQUIRE_THAT(result.signals[0].value, WithinRel(256.0, 0.001));
    REQUIRE_THAT(result.signals[0].raw_value, WithinRel(1024.0, 0.001));
    REQUIRE(result.signals[0].unit == "rpm");

    REQUIRE(result.signals[1].name == "Temp");
    REQUIRE_THAT(result.signals[1].value, WithinRel(40.0, 0.001));
    REQUIRE(result.signals[1].unit == "C");
}

TEST_CASE("Decode frame with unknown message", "[can_db][decoder]") {
    auto db = make_test_database();
    c_trace_decoder decoder(std::move(db));

    can::c_can_frame frame{
        .id = 0x999,
        .dlc = 4,
        .data = {0xAA, 0xBB, 0xCC, 0xDD, 0, 0, 0, 0},
    };

    auto result = decoder.decode_frame(frame);

    REQUIRE(result.known == false);
    REQUIRE(result.message_name.empty());
    REQUIRE(result.signals.empty());
    REQUIRE(result.raw.id == 0x999);
}

TEST_CASE("Decode trace from ASC reader", "[can_db][decoder]") {
    auto db = make_test_database();
    c_trace_decoder decoder(std::move(db));

    auto content = R"(date Mon Jan 01 00:00:00 AM 2024
base hex timestamps absolute
   0.001000 1  100             Rx   d 8 00 04 50 00 00 00 00 00
   0.002000 1  200             Rx   d 4 AA BB CC DD
   0.003000 1  100             Rx   d 8 00 08 3C 00 00 00 00 00
)";

    auto path = write_temp_file("test_decoder_trace.asc", content);
    {
        c_asc_reader reader;
        REQUIRE(reader.open(path).has_value());

        auto result = decoder.decode_trace(reader);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 3);

        // Frame 1: known (EngineData)
        REQUIRE(result->at(0).known == true);
        REQUIRE(result->at(0).message_name == "EngineData");
        REQUIRE(result->at(0).signals.size() == 2);
        REQUIRE_THAT(result->at(0).signals[0].value, WithinRel(256.0, 0.001));

        // Frame 2: unknown
        REQUIRE(result->at(1).known == false);
        REQUIRE(result->at(1).signals.empty());

        // Frame 3: known (EngineData again)
        REQUIRE(result->at(2).known == true);
        REQUIRE_THAT(result->at(2).signals[0].value, WithinRel(512.0, 0.001));
    }
    std::filesystem::remove(path);
}

TEST_CASE("Decode empty trace", "[can_db][decoder]") {
    auto db = make_test_database();
    c_trace_decoder decoder(std::move(db));

    auto content = R"(date Mon Jan 01 00:00:00 AM 2024
base hex timestamps absolute
no internal events logged
)";

    auto path = write_temp_file("test_decoder_empty.asc", content);
    {
        c_asc_reader reader;
        REQUIRE(reader.open(path).has_value());

        auto result = decoder.decode_trace(reader);
        REQUIRE(result.has_value());
        REQUIRE(result->empty());
    }
    std::filesystem::remove(path);
}

TEST_CASE("Decode frame with multiple signals", "[can_db][decoder]") {
    c_database db{
        .name = "multi_sig_db",
        .messages = {
            c_message_def{
                .id = 0x300,
                .name = "SensorData",
                .dlc = 8,
                .signals = {
                    c_signal_def{
                        .name = "Pressure",
                        .start_bit = 0,
                        .length = 16,
                        .byte_order = e_byte_order::little_endian,
                        .value_type = e_value_type::unsigned_int,
                        .factor = 0.1,
                        .offset = 0.0,
                        .unit = "bar",
                    },
                    c_signal_def{
                        .name = "Humidity",
                        .start_bit = 16,
                        .length = 8,
                        .byte_order = e_byte_order::little_endian,
                        .value_type = e_value_type::unsigned_int,
                        .factor = 0.5,
                        .offset = 0.0,
                        .unit = "%",
                    },
                    c_signal_def{
                        .name = "Voltage",
                        .start_bit = 24,
                        .length = 16,
                        .byte_order = e_byte_order::little_endian,
                        .value_type = e_value_type::unsigned_int,
                        .factor = 0.01,
                        .offset = 0.0,
                        .unit = "V",
                    },
                },
            },
        },
    };

    c_trace_decoder decoder(std::move(db));

    // Pressure raw = 0x03E8 = 1000, physical = 1000 * 0.1 = 100.0 bar
    // Humidity raw = 0xC8 = 200, physical = 200 * 0.5 = 100.0 %
    // Voltage raw = 0x04B0 = 1200, physical = 1200 * 0.01 = 12.0 V
    can::c_can_frame frame{
        .id = 0x300,
        .dlc = 8,
        .data = {0xE8, 0x03, 0xC8, 0xB0, 0x04, 0, 0, 0},
    };

    auto result = decoder.decode_frame(frame);

    REQUIRE(result.known == true);
    REQUIRE(result.message_name == "SensorData");
    REQUIRE(result.signals.size() == 3);

    REQUIRE(result.signals[0].name == "Pressure");
    REQUIRE_THAT(result.signals[0].value, WithinRel(100.0, 0.001));
    REQUIRE(result.signals[0].unit == "bar");

    REQUIRE(result.signals[1].name == "Humidity");
    REQUIRE_THAT(result.signals[1].value, WithinRel(100.0, 0.001));
    REQUIRE(result.signals[1].unit == "%");

    REQUIRE(result.signals[2].name == "Voltage");
    REQUIRE_THAT(result.signals[2].value, WithinRel(12.0, 0.001));
    REQUIRE(result.signals[2].unit == "V");
}

TEST_CASE("Database accessor returns loaded database", "[can_db][decoder]") {
    auto db = make_test_database();
    c_trace_decoder decoder(db);

    const auto& returned_db = decoder.database();
    REQUIRE(returned_db.name == "test_db");
    REQUIRE(returned_db.messages.size() == 1);
    REQUIRE(returned_db.messages[0].name == "EngineData");
}
