#include <catch2/catch_test_macros.hpp>
#include "interface/can/sequence_detector.hpp"

using namespace interface;
using namespace interface::can;

namespace {

auto make_frame(can_id_t id, std::initializer_list<byte_t> payload, timestamp_us_t ts = 0) -> c_can_frame {
    c_can_frame f{};
    f.id = id;
    f.dlc = static_cast<std::uint8_t>(payload.size());
    std::size_t i = 0;
    for (auto b : payload) {
        if (i < k_can_max_dlc) {
            f.data[i++] = b;
        }
    }
    f.timestamp = ts;
    return f;
}

auto make_simple_rule() -> c_sequence_rule {
    c_sequence_step step1;
    step1.label = "Request";
    step1.id = 0x100;
    step1.payload.push_back(c_byte_matcher::exact(0x01));

    c_sequence_step step2;
    step2.label = "Response";
    step2.id = 0x200;
    step2.payload.push_back(c_byte_matcher::exact(0x02));

    return c_sequence_rule{
        .name = "TestRule",
        .steps = {std::move(step1), std::move(step2)},
        .allow_interleaved = true,
        .repeatable = true,
    };
}

} // anonymous namespace

// =============================================================================
// Basic matching (tests 1-3)
// =============================================================================

TEST_CASE("Two-step sequence completes on matching frames", "[can][sequence_detector]") {
    c_sequence_detector detector;
    detector.add_rule(make_simple_rule());

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    detector.process_frame(make_frame(0x100, {0x01}, 1000));
    detector.process_frame(make_frame(0x200, {0x02}, 2000));

    REQUIRE(events.size() == 2);
    CHECK(events[0].type == e_sequence_event_type::sequence_started);
    CHECK(events[0].rule_name == "TestRule");
    CHECK(events[1].type == e_sequence_event_type::sequence_completed);
    CHECK(events[1].rule_name == "TestRule");
}

TEST_CASE("Three-step rule times out on missing step", "[can][sequence_detector]") {
    c_sequence_step step1;
    step1.label = "Step1";
    step1.id = 0x100;
    step1.timeout_us = 1'000'000;

    c_sequence_step step2;
    step2.label = "Step2";
    step2.id = 0x200;
    step2.timeout_us = 500'000;

    c_sequence_step step3;
    step3.label = "Step3";
    step3.id = 0x300;
    step3.timeout_us = 500'000;

    c_sequence_rule rule{
        .name = "ThreeStep",
        .steps = {std::move(step1), std::move(step2), std::move(step3)},
    };

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Feed steps 1 and 2
    detector.process_frame(make_frame(0x100, {}, 1'000'000));
    detector.process_frame(make_frame(0x200, {}, 1'100'000));

    // Wait too long for step 3
    detector.check_timeouts(1'700'000);

    bool found_timeout = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::step_timeout) {
            found_timeout = true;
            CHECK(e.rule_name == "ThreeStep");
            CHECK(e.step_label == "Step3");
            CHECK(e.severity == e_sequence_severity::error);
        }
    }
    REQUIRE(found_timeout);
}

TEST_CASE("Frames not matching any rule produce no events", "[can][sequence_detector]") {
    c_sequence_detector detector;
    detector.add_rule(make_simple_rule());

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // None of these match the rule's first step (0x100 with payload 0x01)
    detector.process_frame(make_frame(0x300, {0xFF}, 1000));
    detector.process_frame(make_frame(0x400, {0xAB}, 2000));

    REQUIRE(events.empty());
}

// =============================================================================
// Byte matchers (tests 4-8)
// =============================================================================

TEST_CASE("Exact byte matcher", "[can][sequence_detector][byte_matcher]") {
    auto m = c_byte_matcher::exact(0x42);
    CHECK(m.matches(0x42));
    CHECK_FALSE(m.matches(0x43));
    CHECK_FALSE(m.matches(0x00));
}

TEST_CASE("Masked byte matcher", "[can][sequence_detector][byte_matcher]") {
    // Match upper nibble == 0xA0
    auto m = c_byte_matcher::masked(0xA0, 0xF0);
    CHECK(m.matches(0xA0));
    CHECK(m.matches(0xA5));
    CHECK(m.matches(0xAF));
    CHECK_FALSE(m.matches(0xB0));
    CHECK_FALSE(m.matches(0x0A));
}

TEST_CASE("Range byte matcher", "[can][sequence_detector][byte_matcher]") {
    auto m = c_byte_matcher::range(0x10, 0x20);
    CHECK(m.matches(0x10));      // boundary low
    CHECK(m.matches(0x20));      // boundary high
    CHECK(m.matches(0x15));      // inside
    CHECK_FALSE(m.matches(0x0F)); // below
    CHECK_FALSE(m.matches(0x21)); // above
}

TEST_CASE("Any byte matcher", "[can][sequence_detector][byte_matcher]") {
    auto m = c_byte_matcher::any();
    CHECK(m.matches(0x00));
    CHECK(m.matches(0x42));
    CHECK(m.matches(0xFF));
}

TEST_CASE("Mixed matchers in a step payload", "[can][sequence_detector][byte_matcher]") {
    c_sequence_step step1;
    step1.label = "MixedStep";
    step1.id = 0x100;
    step1.payload.push_back(c_byte_matcher::exact(0x27));     // SID must be exact
    step1.payload.push_back(c_byte_matcher::range(0x01, 0x0F)); // SubFunction in range
    step1.payload.push_back(c_byte_matcher::any());             // Don't care about byte 3

    c_sequence_step step2;
    step2.label = "Done";
    step2.id = 0x200;

    c_sequence_rule rule{
        .name = "MixedRule",
        .steps = {std::move(step1), std::move(step2)},
    };

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Should match: exact 0x27, range 0x05 in [0x01,0x0F], any 0xFF
    detector.process_frame(make_frame(0x100, {0x27, 0x05, 0xFF}, 1000));
    REQUIRE(!events.empty());
    CHECK(events[0].type == e_sequence_event_type::sequence_started);

    // Reset and try a non-matching frame
    detector.reset();
    events.clear();
    detector.process_frame(make_frame(0x100, {0x27, 0x10, 0xFF}, 2000));  // 0x10 outside range
    REQUIRE(events.empty());
}

// =============================================================================
// Sequence behavior (tests 9-13)
// =============================================================================

TEST_CASE("Multiple concurrent sequences tracking different rules", "[can][sequence_detector]") {
    c_sequence_step ruleA_step1;
    ruleA_step1.label = "A1";
    ruleA_step1.id = 0x100;

    c_sequence_step ruleA_step2;
    ruleA_step2.label = "A2";
    ruleA_step2.id = 0x101;

    c_sequence_step ruleB_step1;
    ruleB_step1.label = "B1";
    ruleB_step1.id = 0x200;

    c_sequence_step ruleB_step2;
    ruleB_step2.label = "B2";
    ruleB_step2.id = 0x201;

    c_sequence_rule ruleA{
        .name = "RuleA",
        .steps = {std::move(ruleA_step1), std::move(ruleA_step2)},
    };

    c_sequence_rule ruleB{
        .name = "RuleB",
        .steps = {std::move(ruleB_step1), std::move(ruleB_step2)},
    };

    c_sequence_detector detector;
    detector.add_rule(std::move(ruleA));
    detector.add_rule(std::move(ruleB));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Start both sequences
    detector.process_frame(make_frame(0x100, {}, 1000));
    detector.process_frame(make_frame(0x200, {}, 2000));

    auto active = detector.active_sequences();
    REQUIRE(active.size() == 2);

    // Complete both sequences
    detector.process_frame(make_frame(0x101, {}, 3000));
    detector.process_frame(make_frame(0x201, {}, 4000));

    int completed_count = 0;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::sequence_completed) {
            ++completed_count;
        }
    }
    REQUIRE(completed_count == 2);
}

TEST_CASE("allow_interleaved=true does not abort on non-matching frames", "[can][sequence_detector]") {
    c_sequence_detector detector;
    detector.add_rule(make_simple_rule());

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Start the sequence
    detector.process_frame(make_frame(0x100, {0x01}, 1000));
    REQUIRE(!events.empty());
    CHECK(events.back().type == e_sequence_event_type::sequence_started);

    // Send unrelated frames — should be ignored (no unexpected_frame event)
    detector.process_frame(make_frame(0x300, {0xFF}, 1500));
    detector.process_frame(make_frame(0x400, {0xAB}, 1600));

    // Complete the sequence
    detector.process_frame(make_frame(0x200, {0x02}, 2000));

    bool found_unexpected = false;
    bool found_completed = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::unexpected_frame) {
            found_unexpected = true;
        }
        if (e.type == e_sequence_event_type::sequence_completed) {
            found_completed = true;
        }
    }
    CHECK_FALSE(found_unexpected);
    CHECK(found_completed);
}

TEST_CASE("allow_interleaved=false aborts on non-matching frames", "[can][sequence_detector]") {
    auto rule = make_simple_rule();
    rule.allow_interleaved = false;

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Start the sequence
    detector.process_frame(make_frame(0x100, {0x01}, 1000));

    // Send an unrelated frame — should emit unexpected_frame and abort
    detector.process_frame(make_frame(0x300, {0xFF}, 1500));

    bool found_unexpected = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::unexpected_frame) {
            found_unexpected = true;
            CHECK(e.severity == e_sequence_severity::warning);
        }
    }
    REQUIRE(found_unexpected);

    // Sequence should have been aborted — no active sequences
    auto active = detector.active_sequences();
    REQUIRE(active.empty());
}

TEST_CASE("repeatable=true restarts watching after completion", "[can][sequence_detector]") {
    c_sequence_detector detector;
    detector.add_rule(make_simple_rule());

    int completed_count = 0;
    detector.set_event_callback([&](const c_sequence_event& e) {
        if (e.type == e_sequence_event_type::sequence_completed) {
            ++completed_count;
        }
    });

    // Complete the sequence once
    detector.process_frame(make_frame(0x100, {0x01}, 1000));
    detector.process_frame(make_frame(0x200, {0x02}, 2000));
    REQUIRE(completed_count == 1);

    // Complete it again — rule should still be active
    detector.process_frame(make_frame(0x100, {0x01}, 3000));
    detector.process_frame(make_frame(0x200, {0x02}, 4000));
    REQUIRE(completed_count == 2);
}

TEST_CASE("check_timeouts causes timeout events", "[can][sequence_detector]") {
    c_sequence_step step1;
    step1.label = "Start";
    step1.id = 0x100;
    step1.timeout_us = 1'000'000;

    c_sequence_step step2;
    step2.label = "End";
    step2.id = 0x200;
    step2.timeout_us = 500'000;

    c_sequence_rule rule{
        .name = "TimeoutRule",
        .steps = {std::move(step1), std::move(step2)},
    };

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Start the sequence
    detector.process_frame(make_frame(0x100, {}, 1'000'000));

    // Check timeouts after 500ms — should be fine
    detector.check_timeouts(1'400'000);
    bool found_timeout = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::step_timeout) {
            found_timeout = true;
        }
    }
    CHECK_FALSE(found_timeout);

    // Check timeouts after timeout elapsed
    detector.check_timeouts(1'600'000);
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::step_timeout) {
            found_timeout = true;
            CHECK(e.step_label == "End");
        }
    }
    REQUIRE(found_timeout);
}

// =============================================================================
// Pre-built rules (tests 14-19)
// =============================================================================

TEST_CASE("uds_request_response — positive response completes", "[can][sequence_detector][rules]") {
    auto rule = rules::uds_request_response(0x7E0, 0x7E8, 0x10, "UDS DiagSession");

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Request: DiagnosticSessionControl (0x10)
    detector.process_frame(make_frame(0x7E0, {0x10, 0x01}, 1000));
    // Positive response: 0x50 (0x10 + 0x40)
    detector.process_frame(make_frame(0x7E8, {0x50, 0x01}, 2000));

    bool found_completed = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::sequence_completed) {
            found_completed = true;
            CHECK(e.rule_name == "UDS DiagSession");
        }
    }
    REQUIRE(found_completed);
}

TEST_CASE("uds_request_response — negative response also completes", "[can][sequence_detector][rules]") {
    auto rule = rules::uds_request_response(0x7E0, 0x7E8, 0x10);

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Request: DiagnosticSessionControl (0x10)
    detector.process_frame(make_frame(0x7E0, {0x10, 0x01}, 1000));
    // Negative response: 0x7F, 0x10, NRC
    detector.process_frame(make_frame(0x7E8, {0x7F, 0x10, 0x12}, 2000));

    bool found_completed = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::sequence_completed) {
            found_completed = true;
        }
    }
    REQUIRE(found_completed);
}

TEST_CASE("uds_security_access — full handshake completes", "[can][sequence_detector][rules]") {
    auto rule = rules::uds_security_access(0x7E0, 0x7E8, 0x01);

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Step 1: Seed request (0x27, 0x01)
    detector.process_frame(make_frame(0x7E0, {0x27, 0x01}, 1000));
    // Step 2: Seed response (0x67, 0x01, seed...)
    detector.process_frame(make_frame(0x7E8, {0x67, 0x01, 0xDE, 0xAD}, 2000));
    // Step 3: Key send (0x27, 0x02)
    detector.process_frame(make_frame(0x7E0, {0x27, 0x02, 0xBE, 0xEF}, 3000));
    // Step 4: Key response (0x67, 0x02)
    detector.process_frame(make_frame(0x7E8, {0x67, 0x02}, 4000));

    bool found_completed = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::sequence_completed) {
            found_completed = true;
        }
    }
    REQUIRE(found_completed);
}

TEST_CASE("uds_security_access — timeout on seed response", "[can][sequence_detector][rules]") {
    auto rule = rules::uds_security_access(0x7E0, 0x7E8, 0x01);

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Step 1: Seed request
    detector.process_frame(make_frame(0x7E0, {0x27, 0x01}, 1'000'000));

    // Timeout — no seed response
    detector.check_timeouts(2'100'000);

    bool found_timeout = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::step_timeout) {
            found_timeout = true;
            CHECK(e.severity == e_sequence_severity::error);
        }
    }
    REQUIRE(found_timeout);
}

TEST_CASE("canopen_nmt_bootup — NMT start + bootup completes", "[can][sequence_detector][rules]") {
    auto rule = rules::canopen_nmt_bootup(0x05);

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // NMT Start Remote Node (ID=0x000, data=[0x01, 0x05])
    detector.process_frame(make_frame(0x000, {0x01, 0x05}, 1000));
    // Bootup message (ID=0x705, data=[0x00])
    detector.process_frame(make_frame(0x705, {0x00}, 2000));

    bool found_completed = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::sequence_completed) {
            found_completed = true;
        }
    }
    REQUIRE(found_completed);
}

TEST_CASE("canopen_sdo_upload — request + response completes", "[can][sequence_detector][rules]") {
    auto rule = rules::canopen_sdo_upload(0x01, 0x1018, 0x01);

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // SDO Upload Request (ID=0x601, data=[0x40, 0x18, 0x10, 0x01, ...])
    detector.process_frame(make_frame(0x601, {0x40, 0x18, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00}, 1000));
    // SDO Upload Response (ID=0x581, data=[0x43, 0x18, 0x10, 0x01, ...] expedited with 4 bytes)
    detector.process_frame(make_frame(0x581, {0x43, 0x18, 0x10, 0x01, 0xAB, 0xCD, 0xEF, 0x01}, 2000));

    bool found_completed = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::sequence_completed) {
            found_completed = true;
        }
    }
    REQUIRE(found_completed);
}

// =============================================================================
// Edge cases (tests 20-23)
// =============================================================================

TEST_CASE("Remove rule while sequence is in progress", "[can][sequence_detector]") {
    c_sequence_detector detector;
    detector.add_rule(make_simple_rule());

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Start the sequence
    detector.process_frame(make_frame(0x100, {0x01}, 1000));
    REQUIRE(!detector.active_sequences().empty());

    // Remove the rule
    bool removed = detector.remove_rule("TestRule");
    REQUIRE(removed);

    // Active sequences should be cleared
    REQUIRE(detector.active_sequences().empty());

    // The response frame should not trigger anything
    events.clear();
    detector.process_frame(make_frame(0x200, {0x02}, 2000));
    REQUIRE(events.empty());
}

TEST_CASE("Reset clears all active sequences", "[can][sequence_detector]") {
    c_sequence_detector detector;
    detector.add_rule(make_simple_rule());

    // Start a sequence
    detector.process_frame(make_frame(0x100, {0x01}, 1000));
    REQUIRE(!detector.active_sequences().empty());

    // Reset
    detector.reset();
    REQUIRE(detector.active_sequences().empty());

    // Rules should still be registered
    auto names = detector.rules();
    REQUIRE(names.size() == 1);
    CHECK(names[0] == "TestRule");
}

TEST_CASE("Empty payload matcher matches any payload", "[can][sequence_detector]") {
    c_sequence_step step1;
    step1.label = "AnyPayload";
    step1.id = 0x100;
    // No payload matchers — should match any payload.

    c_sequence_step step2;
    step2.label = "Done";
    step2.id = 0x200;

    c_sequence_rule rule{
        .name = "EmptyPayloadRule",
        .steps = {std::move(step1), std::move(step2)},
    };

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Any payload on the right ID should match step 1
    detector.process_frame(make_frame(0x100, {0xAA, 0xBB, 0xCC}, 1000));
    REQUIRE(!events.empty());
    CHECK(events[0].type == e_sequence_event_type::sequence_started);
}

TEST_CASE("Step with id_mask=0 matches any CAN ID", "[can][sequence_detector]") {
    c_sequence_step step1;
    step1.label = "AnyID";
    step1.id = 0x000;
    step1.id_mask = 0x00000000;  // Match any ID
    step1.payload.push_back(c_byte_matcher::exact(0xAA));

    c_sequence_step step2;
    step2.label = "Done";
    step2.id = 0x200;

    c_sequence_rule rule{
        .name = "AnyIDRule",
        .steps = {std::move(step1), std::move(step2)},
    };

    c_sequence_detector detector;
    detector.add_rule(std::move(rule));

    std::vector<c_sequence_event> events;
    detector.set_event_callback([&](const c_sequence_event& e) {
        events.push_back(e);
    });

    // Any CAN ID with payload 0xAA should trigger
    detector.process_frame(make_frame(0x555, {0xAA}, 1000));
    REQUIRE(!events.empty());
    CHECK(events[0].type == e_sequence_event_type::sequence_started);

    // Complete
    detector.process_frame(make_frame(0x200, {}, 2000));

    bool found_completed = false;
    for (const auto& e : events) {
        if (e.type == e_sequence_event_type::sequence_completed) {
            found_completed = true;
        }
    }
    REQUIRE(found_completed);
}
