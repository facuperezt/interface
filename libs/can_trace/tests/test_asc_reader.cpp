#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "interface/can_trace/c_asc_reader.hpp"

#include <fstream>
#include <filesystem>

using namespace interface;
using namespace interface::can_trace;
using Catch::Matchers::WithinAbs;

namespace {

auto write_temp_file(const std::string& name, const std::string& content) -> std::filesystem::path {
    auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream f(path);
    f << content;
    return path;
}

} // anonymous namespace

TEST_CASE("ASC reader supported extensions", "[can_trace][asc]") {
    c_asc_reader reader;
    auto exts = reader.supported_extensions();
    REQUIRE(exts.size() == 1);
    REQUIRE(exts[0] == ".asc");
}

TEST_CASE("ASC reader fails on non-existent file", "[can_trace][asc]") {
    c_asc_reader reader;
    auto result = reader.open("/tmp/nonexistent_file_12345.asc");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == e_error_category::io);
}

TEST_CASE("ASC reader read_next fails when not opened", "[can_trace][asc]") {
    c_asc_reader reader;
    auto result = reader.read_next();
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ASC reader parses basic frames", "[can_trace][asc]") {
    auto content = R"(date Mon Jan 01 00:00:00 AM 2024
base hex timestamps absolute
no internal events logged
   0.000100 1  100             Rx   d 8 01 02 03 04 05 06 07 08
   0.001200 1  200             Tx   d 4 AA BB CC DD
   0.005000 2  1FF             Rx   d 2 FE ED
)";

    auto path = write_temp_file("test_basic.asc", content);
    {
        c_asc_reader reader;
        REQUIRE(reader.open(path).has_value());

        // Frame 1
        auto r1 = reader.read_next();
        REQUIRE(r1.has_value());
        REQUIRE(r1->has_value());
        auto& f1 = r1->value();
        REQUIRE(f1.id == 0x100);
        REQUIRE(f1.dlc == 8);
        REQUIRE(f1.data[0] == 0x01);
        REQUIRE(f1.data[7] == 0x08);
        REQUIRE_THAT(static_cast<double>(f1.timestamp) / 1'000'000.0, WithinAbs(0.0001, 0.00001));

        // Frame 2
        auto r2 = reader.read_next();
        REQUIRE(r2.has_value());
        REQUIRE(r2->has_value());
        auto& f2 = r2->value();
        REQUIRE(f2.id == 0x200);
        REQUIRE(f2.dlc == 4);
        REQUIRE(f2.data[0] == 0xAA);
        REQUIRE(f2.data[3] == 0xDD);

        // Frame 3
        auto r3 = reader.read_next();
        REQUIRE(r3.has_value());
        REQUIRE(r3->has_value());
        auto& f3 = r3->value();
        REQUIRE(f3.id == 0x1FF);
        REQUIRE(f3.dlc == 2);

        // EOF
        auto r4 = reader.read_next();
        REQUIRE(r4.has_value());
        REQUIRE_FALSE(r4->has_value());
    }
    std::filesystem::remove(path);
}

TEST_CASE("ASC reader read_all returns all frames", "[can_trace][asc]") {
    auto content = R"(date Mon Jan 01 00:00:00 AM 2024
base hex timestamps absolute
   0.001000 1  100             Rx   d 8 01 02 03 04 05 06 07 08
   0.002000 1  200             Rx   d 4 AA BB CC DD
   0.003000 1  300             Rx   d 1 FF
)";

    auto path = write_temp_file("test_read_all.asc", content);
    {
        c_asc_reader reader;
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 3);
        REQUIRE(result->at(0).id == 0x100);
        REQUIRE(result->at(1).id == 0x200);
        REQUIRE(result->at(2).id == 0x300);
    }
    std::filesystem::remove(path);
}

TEST_CASE("ASC reader skips comment lines", "[can_trace][asc]") {
    auto content = R"(; This is a comment
; Another comment
   0.001000 1  100             Rx   d 2 AB CD
; Mid-file comment
   0.002000 1  200             Rx   d 1 EF
)";

    auto path = write_temp_file("test_comments.asc", content);
    {
        c_asc_reader reader;
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());
        REQUIRE(result->size() == 2);
    }
    std::filesystem::remove(path);
}

TEST_CASE("ASC reader info tracks metadata", "[can_trace][asc]") {
    auto content = R"(   0.001000 1  100             Rx   d 2 AB CD
   0.050000 1  200             Rx   d 1 EF
)";

    auto path = write_temp_file("test_info.asc", content);
    {
        c_asc_reader reader;
        REQUIRE(reader.open(path).has_value());

        auto result = reader.read_all();
        REQUIRE(result.has_value());

        auto info = reader.info();
        REQUIRE(info.format == "ASC");
        REQUIRE(info.frame_count == 2);
        REQUIRE(info.start_time == 1000);  // 0.001s = 1000us
        REQUIRE(info.end_time == 50000);   // 0.050s = 50000us
    }
    std::filesystem::remove(path);
}

TEST_CASE("ASC reader reset re-reads from beginning", "[can_trace][asc]") {
    auto content = "   0.001000 1  100             Rx   d 1 AA\n";

    auto path = write_temp_file("test_reset.asc", content);
    {
        c_asc_reader reader;
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

TEST_CASE("ASC reader handles extended IDs", "[can_trace][asc]") {
    auto content = "   0.001000 1  1ABCDEF0x       Rx   d 2 01 02\n";

    auto path = write_temp_file("test_ext.asc", content);
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
