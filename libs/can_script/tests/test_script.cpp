#include <catch2/catch_test_macros.hpp>
#include "interface/can_script/script.hpp"

#include <filesystem>
#include <fstream>

using namespace interface;
using namespace interface::can_script;

// =============================================================================
// Test 1: Parse a minimal single-step script from JSON
// =============================================================================

TEST_CASE("Parse minimal single-step script from JSON", "[can_script][parse]") {
    auto j = nlohmann::json::parse(R"({
        "name": "minimal",
        "steps": [
            {
                "label": "send_one",
                "trigger": {"type": "immediate"},
                "action": {
                    "type": "send_frame",
                    "frame": {"id": "0x100", "dlc": 2, "data": ["0x01", "0x02"]}
                }
            }
        ]
    })");

    auto result = c_script::from_json(j);
    REQUIRE(result.has_value());

    const auto& script = *result;
    CHECK(script.name == "minimal");
    REQUIRE(script.steps.size() == 1);
    CHECK(script.steps[0].label == "send_one");
    CHECK(script.steps[0].trigger_type == e_trigger_type::immediate);
    CHECK(script.steps[0].action.type == e_action_type::send_frame);
    CHECK(script.steps[0].action.frame.id == 0x100);
    CHECK(script.steps[0].action.frame.dlc == 2);
    CHECK(script.steps[0].action.frame.data[0] == 0x01);
    CHECK(script.steps[0].action.frame.data[1] == 0x02);
}

// =============================================================================
// Test 2: Parse a multi-step script with all trigger types
// =============================================================================

TEST_CASE("Parse multi-step script with all trigger types", "[can_script][parse]") {
    auto j = nlohmann::json::parse(R"({
        "name": "all_triggers",
        "description": "Tests all trigger types",
        "loop": true,
        "steps": [
            {
                "label": "step_immediate",
                "trigger": {"type": "immediate"},
                "action": {"type": "no_op"}
            },
            {
                "label": "step_delay",
                "trigger": {"type": "delay", "delay_ms": 500},
                "action": {"type": "no_op"}
            },
            {
                "label": "step_receive",
                "trigger": {
                    "type": "on_receive",
                    "match": {"id": "0x200"}
                },
                "action": {"type": "no_op"}
            },
            {
                "label": "step_receive_or_timeout",
                "trigger": {
                    "type": "on_receive_or_timeout",
                    "match": {"id": "0x300"},
                    "timeout_ms": 2000
                },
                "action": {"type": "no_op"}
            }
        ]
    })");

    auto result = c_script::from_json(j);
    REQUIRE(result.has_value());

    const auto& script = *result;
    CHECK(script.name == "all_triggers");
    CHECK(script.description == "Tests all trigger types");
    CHECK(script.loop == true);
    REQUIRE(script.steps.size() == 4);

    CHECK(script.steps[0].trigger_type == e_trigger_type::immediate);
    CHECK(script.steps[1].trigger_type == e_trigger_type::delay);
    CHECK(script.steps[1].delay == std::chrono::microseconds(500'000));
    CHECK(script.steps[2].trigger_type == e_trigger_type::on_receive);
    CHECK(script.steps[2].match.id == 0x200);
    CHECK(script.steps[3].trigger_type == e_trigger_type::on_receive_or_timeout);
    CHECK(script.steps[3].match.id == 0x300);
    CHECK(script.steps[3].timeout == std::chrono::microseconds(2'000'000));
}

// =============================================================================
// Test 3: Parse send_sequence action with delays
// =============================================================================

TEST_CASE("Parse send_sequence action with delays", "[can_script][parse]") {
    auto j = nlohmann::json::parse(R"({
        "name": "seq_test",
        "steps": [
            {
                "label": "multi_frame",
                "trigger": {"type": "immediate"},
                "action": {
                    "type": "send_sequence",
                    "sequence": [
                        {"delay_ms": 0,  "frame": {"id": "0x100", "dlc": 2, "data": ["0xAA", "0xBB"]}},
                        {"delay_ms": 10, "frame": {"id": "0x100", "dlc": 2, "data": ["0xCC", "0xDD"]}},
                        {"delay_ms": 20, "frame": {"id": "0x100", "dlc": 1, "data": ["0xEE"]}}
                    ]
                }
            }
        ]
    })");

    auto result = c_script::from_json(j);
    REQUIRE(result.has_value());

    const auto& action = result->steps[0].action;
    CHECK(action.type == e_action_type::send_sequence);
    REQUIRE(action.sequence.size() == 3);

    CHECK(action.sequence[0].delay_before == std::chrono::microseconds(0));
    CHECK(action.sequence[0].frame.data[0] == 0xAA);
    CHECK(action.sequence[1].delay_before == std::chrono::microseconds(10'000));
    CHECK(action.sequence[1].frame.data[0] == 0xCC);
    CHECK(action.sequence[2].delay_before == std::chrono::microseconds(20'000));
    CHECK(action.sequence[2].frame.dlc == 1);
}

// =============================================================================
// Test 4: Parse frame match with various byte matchers
// =============================================================================

TEST_CASE("Parse frame match with various byte matchers", "[can_script][parse]") {
    auto j = nlohmann::json::parse(R"({
        "name": "matcher_test",
        "steps": [
            {
                "label": "complex_match",
                "trigger": {
                    "type": "on_receive",
                    "match": {
                        "id": "0x7E0",
                        "payload": [
                            {"type": "exact", "value": "0x10"},
                            {"type": "masked", "value": "0xA0", "mask": "0xF0"},
                            {"type": "range", "low": "0x01", "high": "0x0F"},
                            {"type": "any"}
                        ]
                    }
                },
                "action": {"type": "no_op"}
            }
        ]
    })");

    auto result = c_script::from_json(j);
    REQUIRE(result.has_value());

    const auto& match = result->steps[0].match;
    CHECK(match.id == 0x7E0);
    REQUIRE(match.payload_matchers.size() == 4);

    // Test the matchers work correctly
    CHECK(match.payload_matchers[0].matches(0x10));
    CHECK_FALSE(match.payload_matchers[0].matches(0x11));

    CHECK(match.payload_matchers[1].matches(0xA5));
    CHECK_FALSE(match.payload_matchers[1].matches(0xB0));

    CHECK(match.payload_matchers[2].matches(0x05));
    CHECK_FALSE(match.payload_matchers[2].matches(0x10));

    CHECK(match.payload_matchers[3].matches(0x00));
    CHECK(match.payload_matchers[3].matches(0xFF));
}

// =============================================================================
// Test 5: to_json/from_json round-trip
// =============================================================================

TEST_CASE("Round-trip to_json and from_json for complete script", "[can_script][parse]") {
    auto j = nlohmann::json::parse(R"({
        "name": "roundtrip",
        "description": "Round trip test",
        "loop": true,
        "steps": [
            {
                "label": "step1",
                "trigger": {"type": "delay", "delay_ms": 100},
                "action": {
                    "type": "send_frame",
                    "frame": {"id": "0x123", "dlc": 3, "data": ["0xDE", "0xAD", "0xBE"]}
                },
                "repeat": true,
                "repeat_count": 5
            }
        ]
    })");

    auto original = c_script::from_json(j);
    REQUIRE(original.has_value());

    auto serialized = original->to_json();
    auto restored = c_script::from_json(serialized);
    REQUIRE(restored.has_value());

    CHECK(restored->name == original->name);
    CHECK(restored->description == original->description);
    CHECK(restored->loop == original->loop);
    REQUIRE(restored->steps.size() == original->steps.size());

    CHECK(restored->steps[0].label == "step1");
    CHECK(restored->steps[0].trigger_type == e_trigger_type::delay);
    CHECK(restored->steps[0].delay == std::chrono::microseconds(100'000));
    CHECK(restored->steps[0].action.type == e_action_type::send_frame);
    CHECK(restored->steps[0].action.frame.id == 0x123);
    CHECK(restored->steps[0].repeat == true);
    CHECK(restored->steps[0].repeat_count == 5);
}

// =============================================================================
// Test 6: save_to_file/load_from_file round-trip
// =============================================================================

TEST_CASE("File round-trip save and load", "[can_script][parse][file]") {
    auto j = nlohmann::json::parse(R"({
        "name": "file_test",
        "description": "File round trip",
        "loop": false,
        "steps": [
            {
                "label": "send_it",
                "trigger": {"type": "immediate"},
                "action": {
                    "type": "send_frame",
                    "frame": {"id": "0x456", "dlc": 1, "data": ["0xFF"]}
                }
            }
        ]
    })");

    auto original = c_script::from_json(j);
    REQUIRE(original.has_value());

    auto tmp_path = std::filesystem::temp_directory_path() / "test_can_script.json";

    auto save_result = original->save_to_file(tmp_path);
    REQUIRE(save_result.has_value());

    auto loaded = c_script::load_from_file(tmp_path);
    REQUIRE(loaded.has_value());

    CHECK(loaded->name == "file_test");
    CHECK(loaded->description == "File round trip");
    REQUIRE(loaded->steps.size() == 1);
    CHECK(loaded->steps[0].action.frame.id == 0x456);
    CHECK(loaded->steps[0].action.frame.data[0] == 0xFF);

    // Cleanup
    std::filesystem::remove(tmp_path);
}

// =============================================================================
// Test 7: Reject invalid JSON (missing required fields)
// =============================================================================

TEST_CASE("Reject invalid JSON missing required fields", "[can_script][parse][error]") {
    // Missing name
    auto j1 = nlohmann::json::parse(R"({"steps": []})");
    auto r1 = c_script::from_json(j1);
    CHECK_FALSE(r1.has_value());

    // Missing steps
    auto j2 = nlohmann::json::parse(R"({"name": "test"})");
    auto r2 = c_script::from_json(j2);
    CHECK_FALSE(r2.has_value());

    // Steps not an array
    auto j3 = nlohmann::json::parse(R"({"name": "test", "steps": "not_array"})");
    auto r3 = c_script::from_json(j3);
    CHECK_FALSE(r3.has_value());
}

// =============================================================================
// Test 8: Reject unknown trigger type
// =============================================================================

TEST_CASE("Reject unknown trigger type", "[can_script][parse][error]") {
    auto j = nlohmann::json::parse(R"({
        "name": "bad_trigger",
        "steps": [
            {
                "label": "bad",
                "trigger": {"type": "unknown_trigger"},
                "action": {"type": "no_op"}
            }
        ]
    })");

    auto result = c_script::from_json(j);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Test 9: Reject unknown action type
// =============================================================================

TEST_CASE("Reject unknown action type", "[can_script][parse][error]") {
    auto j = nlohmann::json::parse(R"({
        "name": "bad_action",
        "steps": [
            {
                "label": "bad",
                "trigger": {"type": "immediate"},
                "action": {"type": "unknown_action"}
            }
        ]
    })");

    auto result = c_script::from_json(j);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Test 10: Parse hex string values in frame data
// =============================================================================

TEST_CASE("Parse hex string values in frame data", "[can_script][parse]") {
    auto j = nlohmann::json::parse(R"({
        "name": "hex_test",
        "steps": [
            {
                "label": "hex",
                "trigger": {"type": "immediate"},
                "action": {
                    "type": "send_frame",
                    "frame": {
                        "id": "0x7FF",
                        "dlc": 4,
                        "data": ["0xFF", "0x00", "0xab", "0xCD"]
                    }
                }
            }
        ]
    })");

    auto result = c_script::from_json(j);
    REQUIRE(result.has_value());

    const auto& frame = result->steps[0].action.frame;
    CHECK(frame.id == 0x7FF);
    CHECK(frame.data[0] == 0xFF);
    CHECK(frame.data[1] == 0x00);
    CHECK(frame.data[2] == 0xAB);
    CHECK(frame.data[3] == 0xCD);
}

// =============================================================================
// Test 11: Parse script with goto labels and repeat settings
// =============================================================================

TEST_CASE("Parse script with goto labels and repeat settings", "[can_script][parse]") {
    auto j = nlohmann::json::parse(R"({
        "name": "goto_test",
        "steps": [
            {
                "label": "start",
                "trigger": {"type": "immediate"},
                "action": {"type": "no_op"},
                "on_match_goto": "end"
            },
            {
                "label": "middle",
                "trigger": {
                    "type": "on_receive_or_timeout",
                    "match": {"id": "0x100"},
                    "timeout_ms": 1000
                },
                "action": {"type": "no_op"},
                "on_timeout_goto": "start",
                "repeat": true,
                "repeat_count": 3
            },
            {
                "label": "end",
                "trigger": {"type": "immediate"},
                "action": {"type": "log_message", "message": "Done!"}
            }
        ]
    })");

    auto result = c_script::from_json(j);
    REQUIRE(result.has_value());

    const auto& steps = result->steps;
    REQUIRE(steps.size() == 3);

    CHECK(steps[0].on_match_goto == "end");
    CHECK(steps[1].on_timeout_goto == "start");
    CHECK(steps[1].repeat == true);
    CHECK(steps[1].repeat_count == 3);
    CHECK(steps[2].action.type == e_action_type::log_message);
    CHECK(steps[2].action.message == "Done!");
}
