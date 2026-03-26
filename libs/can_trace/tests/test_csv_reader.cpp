#include <catch2/catch_test_macros.hpp>
#include "interface/can_trace/c_csv_reader.hpp"

#include <fstream>
#include <filesystem>

using namespace interface;
using namespace interface::can_trace;

namespace {

auto write_temp_file(const std::string& name, const std::string& content) -> std::filesystem::path {
    auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream f(path);
    f << content;
    return path;
}

} // anonymous namespace

TEST_CASE("CSV reader supported extensions", "[can_trace][csv]") {
    c_csv_reader reader;
    auto exts = reader.supported_extensions();
    REQUIRE(exts.size() == 2);
    REQUIRE(exts[0] == ".csv");
    REQUIRE(exts[1] == ".tsv");
}

TEST_CASE("CSV reader fails on non-existent file", "[can_trace][csv]") {
    c_csv_reader reader;
    auto result = reader.open("/tmp/nonexistent_csv_12345.csv");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("CSV reader read_next fails when not opened", "[can_trace][csv]") {
    c_csv_reader reader;
    auto result = reader.read_next();
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("CSV reader parses basic comma-delimited data", "[can_trace][csv]") {
    auto content = R"(Timestamp,ID,DLC,Data
0.001000,100,8,01 02 03 04 05 06 07 08
0.002000,200,4,AA BB CC DD
0.003000,1FF,2,FE ED
)";

    auto path = write_temp_file("test_basic.csv", content);
    {
        c_csv_reader reader;
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 3);

        auto& f1 = result->at(0);
        REQUIRE(f1.id == 0x100);
        REQUIRE(f1.dlc == 8);
        REQUIRE(f1.data[0] == 0x01);
        REQUIRE(f1.data[7] == 0x08);

        auto& f2 = result->at(1);
        REQUIRE(f2.id == 0x200);
        REQUIRE(f2.dlc == 4);
        REQUIRE(f2.data[0] == 0xAA);

        auto& f3 = result->at(2);
        REQUIRE(f3.id == 0x1FF);
        REQUIRE(f3.dlc == 2);
    }
    std::filesystem::remove(path);
}

TEST_CASE("CSV reader with semicolon delimiter", "[can_trace][csv]") {
    auto content = R"(Timestamp;ID;DLC;Data
0.001000;100;2;AB CD
0.002000;200;1;EF
)";

    auto path = write_temp_file("test_semicolon.csv", content);
    {
        c_csv_column_config cfg{.delimiter = ';'};
        c_csv_reader reader(cfg);
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 2);
        REQUIRE(result->at(0).id == 0x100);
        REQUIRE(result->at(0).data[0] == 0xAB);
    }
    std::filesystem::remove(path);
}

TEST_CASE("CSV reader with separate data columns", "[can_trace][csv]") {
    auto content = R"(Timestamp,ID,DLC,D0,D1,D2
0.001000,100,3,AA,BB,CC
0.002000,200,2,DD,EE,00
)";

    auto path = write_temp_file("test_separate_cols.csv", content);
    {
        c_csv_column_config cfg{
            .data_in_single_column = false,
        };
        c_csv_reader reader(cfg);
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 2);
        REQUIRE(result->at(0).data[0] == 0xAA);
        REQUIRE(result->at(0).data[1] == 0xBB);
        REQUIRE(result->at(0).data[2] == 0xCC);
    }
    std::filesystem::remove(path);
}

TEST_CASE("CSV reader with no header", "[can_trace][csv]") {
    auto content = R"(0.001000,100,2,AB CD
0.002000,200,1,EF
)";

    auto path = write_temp_file("test_no_header.csv", content);
    {
        c_csv_column_config cfg{.has_header = false};
        c_csv_reader reader(cfg);
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 2);
    }
    std::filesystem::remove(path);
}

TEST_CASE("CSV reader with hex prefix IDs", "[can_trace][csv]") {
    auto content = R"(Time,ID,DLC,Data
0.001,0x1AB,3,01 02 03
)";

    auto path = write_temp_file("test_hex_prefix.csv", content);
    {
        c_csv_reader reader;
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 1);
        REQUIRE(result->at(0).id == 0x1AB);
    }
    std::filesystem::remove(path);
}

TEST_CASE("CSV reader reset works", "[can_trace][csv]") {
    auto content = R"(Time,ID,DLC,Data
0.001,100,1,AA
)";

    auto path = write_temp_file("test_csv_reset.csv", content);
    {
        c_csv_reader reader;
        REQUIRE(reader.open(path).has_value());

        auto r1 = reader.read_all();
        REQUIRE(r1.has_value());
        REQUIRE(r1->size() == 1);

        auto r2 = reader.read_all();
        REQUIRE(r2.has_value());
        REQUIRE(r2->size() == 1);
    }
    std::filesystem::remove(path);
}

TEST_CASE("CSV reader info metadata", "[can_trace][csv]") {
    auto content = R"(Time,ID,DLC,Data
0.001,100,1,AA
0.050,200,1,BB
)";

    auto path = write_temp_file("test_csv_info.csv", content);
    {
        c_csv_reader reader;
        REQUIRE(reader.open(path).has_value());
        auto result = reader.read_all();
        REQUIRE(result.has_value());

        auto info = reader.info();
        REQUIRE(info.format == "CSV");
        REQUIRE(info.frame_count == 2);
    }
    std::filesystem::remove(path);
}
