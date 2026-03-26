#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "interface/can_db/c_dbc_parser.hpp"

using namespace interface::can_db;
using Catch::Matchers::WithinRel;

TEST_CASE("DBC parser supported extensions", "[can_db][dbc]") {
    c_dbc_parser parser;
    auto exts = parser.supported_extensions();
    REQUIRE(exts.size() == 1);
    REQUIRE(exts[0] == ".dbc");
}

TEST_CASE("DBC parser parse_string: basic message and signal", "[can_db][dbc]") {
    auto content = R"(VERSION "1.0"

NS_ :

BS_:

BU_: ECU1 ECU2

BO_ 256 EngineData: 8 ECU1
 SG_ EngineSpeed : 0|16@1+ (0.25,0) [0|16383.75] "rpm" ECU2
 SG_ EngineTemp : 16|8@1- (1,-40) [-40|215] "degC" ECU2

)";

    c_dbc_parser parser;
    auto result = parser.parse_string(content);
    REQUIRE(result.has_value());

    auto& db = *result;
    REQUIRE(db.version == "1.0");
    REQUIRE(db.messages.size() == 1);

    auto& msg = db.messages[0];
    REQUIRE(msg.id == 256);
    REQUIRE(msg.name == "EngineData");
    REQUIRE(msg.dlc == 8);
    REQUIRE(msg.sender == "ECU1");
    REQUIRE(msg.signals.size() == 2);

    auto& rpm = msg.signals[0];
    REQUIRE(rpm.name == "EngineSpeed");
    REQUIRE(rpm.start_bit == 0);
    REQUIRE(rpm.length == 16);
    REQUIRE(rpm.byte_order == e_byte_order::little_endian);
    REQUIRE(rpm.value_type == e_value_type::unsigned_int);
    REQUIRE_THAT(rpm.factor, WithinRel(0.25, 0.001));
    REQUIRE_THAT(rpm.offset, WithinRel(0.0, 0.001));
    REQUIRE(rpm.unit == "rpm");

    auto& temp = msg.signals[1];
    REQUIRE(temp.name == "EngineTemp");
    REQUIRE(temp.start_bit == 16);
    REQUIRE(temp.length == 8);
    REQUIRE(temp.value_type == e_value_type::signed_int);
    REQUIRE_THAT(temp.offset, WithinRel(-40.0, 0.001));
    REQUIRE(temp.unit == "degC");
}

TEST_CASE("DBC parser parse_string: multiple messages", "[can_db][dbc]") {
    auto content = R"(VERSION ""

BO_ 100 Msg1: 8 Node1
 SG_ Signal1 : 0|8@1+ (1,0) [0|255] "" Node2

BO_ 200 Msg2: 4 Node2
 SG_ Signal2 : 0|16@1+ (0.1,0) [0|6553.5] "V" Node1

BO_ 300 Msg3: 2 Node1
 SG_ Signal3 : 0|16@1- (1,0) [-32768|32767] "" Node2

)";

    c_dbc_parser parser;
    auto result = parser.parse_string(content);
    REQUIRE(result.has_value());
    REQUIRE(result->messages.size() == 3);
    REQUIRE(result->messages[0].id == 100);
    REQUIRE(result->messages[1].id == 200);
    REQUIRE(result->messages[2].id == 300);
}

TEST_CASE("DBC parser parse_string: big-endian (Motorola) signal", "[can_db][dbc]") {
    auto content = R"(VERSION ""

BO_ 0x100 TestMsg: 8 ECU
 SG_ BigEndianSig : 7|16@0+ (1,0) [0|65535] "" Vector__XXX

)";

    c_dbc_parser parser;
    auto result = parser.parse_string(content);
    REQUIRE(result.has_value());
    REQUIRE(result->messages.size() == 1);

    auto& sig = result->messages[0].signals[0];
    REQUIRE(sig.name == "BigEndianSig");
    REQUIRE(sig.byte_order == e_byte_order::big_endian);
}

TEST_CASE("DBC parser parse_string: comments", "[can_db][dbc]") {
    auto content = R"(VERSION ""

BO_ 256 EngineData: 8 ECU
 SG_ RPM : 0|16@1+ (1,0) [0|65535] "" Vector__XXX

CM_ BO_ 256 "Engine data message";
CM_ SG_ 256 RPM "Engine revolutions per minute";

)";

    c_dbc_parser parser;
    auto result = parser.parse_string(content);
    REQUIRE(result.has_value());

    REQUIRE(result->messages[0].comment == "Engine data message");
    REQUIRE(result->messages[0].signals[0].comment == "Engine revolutions per minute");
}

TEST_CASE("DBC parser parse_string: empty content", "[can_db][dbc]") {
    c_dbc_parser parser;
    auto result = parser.parse_string("");
    REQUIRE(result.has_value());
    REQUIRE(result->messages.empty());
}

TEST_CASE("DBC parser parse_string: version only", "[can_db][dbc]") {
    c_dbc_parser parser;
    auto result = parser.parse_string(R"(VERSION "2.5")");
    REQUIRE(result.has_value());
    REQUIRE(result->version == "2.5");
}

TEST_CASE("DBC parser parse: non-existent file", "[can_db][dbc]") {
    c_dbc_parser parser;
    auto result = parser.parse("/tmp/nonexistent_dbc_12345.dbc");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().category == interface::e_error_category::io);
}

TEST_CASE("DBC parser parse_string: signal min/max", "[can_db][dbc]") {
    auto content = R"(VERSION ""

BO_ 100 TestMsg: 8 ECU
 SG_ TestSig : 0|8@1+ (0.5,10) [10|137.5] "units" Vector__XXX

)";

    c_dbc_parser parser;
    auto result = parser.parse_string(content);
    REQUIRE(result.has_value());

    auto& sig = result->messages[0].signals[0];
    REQUIRE_THAT(sig.factor, WithinRel(0.5, 0.001));
    REQUIRE_THAT(sig.offset, WithinRel(10.0, 0.001));
    REQUIRE_THAT(sig.min_value, WithinRel(10.0, 0.001));
    REQUIRE_THAT(sig.max_value, WithinRel(137.5, 0.001));
}
