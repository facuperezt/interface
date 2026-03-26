#include <catch2/catch_test_macros.hpp>
#include "interface/can_script/script_engine.hpp"
#include "interface/can_hal/c_mock_adapter.hpp"

#include <memory>

using namespace interface;
using namespace interface::can_script;
using namespace interface::can_hal;

namespace {

auto make_frame(can_id_t id, std::initializer_list<byte_t> payload, timestamp_us_t ts = 0) -> can::c_can_frame {
    can::c_can_frame f{};
    f.id = id;
    f.dlc = static_cast<std::uint8_t>(payload.size());
    std::size_t i = 0;
    for (auto b : payload) {
        if (i < can::k_can_max_dlc) {
            f.data[i++] = b;
        }
    }
    f.timestamp = ts;
    return f;
}

auto make_adapter() -> std::shared_ptr<c_mock_adapter> {
    auto adapter = std::make_shared<c_mock_adapter>();
    (void)adapter->open(c_bitrate_config{});
    return adapter;
}

auto make_immediate_send_script(can_id_t id, std::initializer_list<byte_t> data) -> c_script {
    c_script script;
    script.name = "test";

    c_script_step step;
    step.label = "send";
    step.trigger_type = e_trigger_type::immediate;
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = id;
    step.action.frame.dlc = static_cast<std::uint8_t>(data.size());
    std::size_t i = 0;
    for (auto b : data) {
        if (i < 8) step.action.frame.data[i++] = b;
    }

    script.steps.push_back(std::move(step));
    return script;
}

} // anonymous namespace

// =============================================================================
// Test 12: Engine starts in idle state
// =============================================================================

TEST_CASE("Engine starts in idle state", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    CHECK(engine.state() == e_engine_state::idle);
    CHECK(engine.current_step() == 0);
}

// =============================================================================
// Test 13: Immediate trigger executes action instantly
// =============================================================================

TEST_CASE("Immediate trigger executes action instantly", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    engine.load_script(make_immediate_send_script(0x100, {0xAA, 0xBB}));
    auto result = engine.start();
    REQUIRE(result.has_value());
    CHECK(engine.state() == e_engine_state::running);

    engine.process(1000);

    // After processing, the frame should have been sent
    auto tx = adapter->get_tx_history();
    REQUIRE(tx.size() == 1);
    CHECK(tx[0].id == 0x100);
    CHECK(tx[0].data[0] == 0xAA);
    CHECK(tx[0].data[1] == 0xBB);

    // Engine should be finished (single step, no loop)
    engine.process(2000);
    CHECK(engine.state() == e_engine_state::finished);
}

// =============================================================================
// Test 14: Delay trigger waits correct duration before executing
// =============================================================================

TEST_CASE("Delay trigger waits correct duration", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "delay_test";

    c_script_step step;
    step.label = "delayed_send";
    step.trigger_type = e_trigger_type::delay;
    step.delay = std::chrono::microseconds(500'000);
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = 0x200;
    step.action.frame.dlc = 1;
    step.action.frame.data[0] = 0xFF;
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();

    // Process at t=100000 -- too early, should not send
    engine.process(100'000);
    CHECK(adapter->get_tx_history().empty());

    // Process at t=300000 -- still too early
    engine.process(300'000);
    CHECK(adapter->get_tx_history().empty());

    // Process at t=600000 -- past the delay, should send
    engine.process(600'000);
    auto tx = adapter->get_tx_history();
    REQUIRE(tx.size() == 1);
    CHECK(tx[0].id == 0x200);
}

// =============================================================================
// Test 15: on_receive trigger fires when matching frame arrives
// =============================================================================

TEST_CASE("on_receive trigger fires on matching frame", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "receive_test";

    c_script_step step;
    step.label = "wait_and_respond";
    step.trigger_type = e_trigger_type::on_receive;
    step.match.id = 0x7E0;
    step.match.payload_matchers.push_back(can::c_byte_matcher::exact(0x10));
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = 0x7E8;
    step.action.frame.dlc = 2;
    step.action.frame.data[0] = 0x50;
    step.action.frame.data[1] = 0x01;
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();

    // Process without receiving -- should not send
    engine.process(1000);
    CHECK(adapter->get_tx_history().empty());

    // Feed matching frame
    engine.on_frame_received(make_frame(0x7E0, {0x10, 0x01}, 2000));
    engine.process(2000);

    auto tx = adapter->get_tx_history();
    REQUIRE(tx.size() == 1);
    CHECK(tx[0].id == 0x7E8);
    CHECK(tx[0].data[0] == 0x50);
}

// =============================================================================
// Test 16: on_receive trigger ignores non-matching frames
// =============================================================================

TEST_CASE("on_receive trigger ignores non-matching frames", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "nomatch_test";

    c_script_step step;
    step.label = "wait";
    step.trigger_type = e_trigger_type::on_receive;
    step.match.id = 0x7E0;
    step.match.payload_matchers.push_back(can::c_byte_matcher::exact(0x10));
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = 0x7E8;
    step.action.frame.dlc = 1;
    step.action.frame.data[0] = 0x50;
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();

    // Feed non-matching frames
    engine.on_frame_received(make_frame(0x7E0, {0x22}, 1000));  // wrong payload
    engine.process(1000);
    CHECK(adapter->get_tx_history().empty());

    engine.on_frame_received(make_frame(0x123, {0x10}, 2000));  // wrong ID
    engine.process(2000);
    CHECK(adapter->get_tx_history().empty());

    CHECK(engine.state() == e_engine_state::running);
}

// =============================================================================
// Test 17: on_receive_or_timeout fires on match before timeout
// =============================================================================

TEST_CASE("on_receive_or_timeout fires on match before timeout", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "recv_or_timeout";

    c_script_step step;
    step.label = "wait_or_timeout";
    step.trigger_type = e_trigger_type::on_receive_or_timeout;
    step.match.id = 0x500;
    step.timeout = std::chrono::microseconds(1'000'000);
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = 0x501;
    step.action.frame.dlc = 1;
    step.action.frame.data[0] = 0xAA;
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();

    // Feed matching frame before timeout
    engine.process(100'000);
    engine.on_frame_received(make_frame(0x500, {}, 200'000));
    engine.process(200'000);

    auto tx = adapter->get_tx_history();
    REQUIRE(tx.size() == 1);
    CHECK(tx[0].id == 0x501);

    // Check no timeout event was emitted
    for (const auto& e : engine.event_log()) {
        CHECK(e.type != c_engine_event::e_type::timeout);
    }
}

// =============================================================================
// Test 18: on_receive_or_timeout fires timeout event when no match
// =============================================================================

TEST_CASE("on_receive_or_timeout fires timeout when no match", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "timeout_test";

    c_script_step step;
    step.label = "will_timeout";
    step.trigger_type = e_trigger_type::on_receive_or_timeout;
    step.match.id = 0x500;
    step.timeout = std::chrono::microseconds(500'000);
    step.action.type = e_action_type::no_op;
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();

    // Process without feeding any frame, past the timeout
    engine.process(100'000);
    CHECK(adapter->get_tx_history().empty());

    engine.process(700'000);

    bool found_timeout = false;
    for (const auto& e : engine.event_log()) {
        if (e.type == c_engine_event::e_type::timeout) {
            found_timeout = true;
            CHECK(e.step_label == "will_timeout");
        }
    }
    REQUIRE(found_timeout);
}

// =============================================================================
// Test 19: on_timeout_goto jumps to correct step
// =============================================================================

TEST_CASE("on_timeout_goto jumps to correct step", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "goto_test";

    // Step 0: wait with timeout, goto "fallback" on timeout
    c_script_step step0;
    step0.label = "wait_step";
    step0.trigger_type = e_trigger_type::on_receive_or_timeout;
    step0.match.id = 0x100;
    step0.timeout = std::chrono::microseconds(500'000);
    step0.action.type = e_action_type::no_op;
    step0.on_timeout_goto = "fallback";

    // Step 1: should be skipped
    c_script_step step1;
    step1.label = "skipped";
    step1.trigger_type = e_trigger_type::immediate;
    step1.action.type = e_action_type::send_frame;
    step1.action.frame.id = 0xBAD;
    step1.action.frame.dlc = 1;
    step1.action.frame.data[0] = 0xFF;

    // Step 2: fallback target
    c_script_step step2;
    step2.label = "fallback";
    step2.trigger_type = e_trigger_type::immediate;
    step2.action.type = e_action_type::send_frame;
    step2.action.frame.id = 0x999;
    step2.action.frame.dlc = 1;
    step2.action.frame.data[0] = 0x01;

    script.steps.push_back(std::move(step0));
    script.steps.push_back(std::move(step1));
    script.steps.push_back(std::move(step2));

    engine.load_script(std::move(script));
    (void)engine.start();

    // Let it timeout
    engine.process(100'000);
    engine.process(700'000);

    // The fallback step should execute immediately
    engine.process(700'001);

    auto tx = adapter->get_tx_history();
    REQUIRE(!tx.empty());

    // Should have sent 0x999, not 0xBAD
    bool sent_fallback = false;
    bool sent_skipped = false;
    for (const auto& f : tx) {
        if (f.id == 0x999) sent_fallback = true;
        if (f.id == 0xBAD) sent_skipped = true;
    }
    CHECK(sent_fallback);
    CHECK_FALSE(sent_skipped);
}

// =============================================================================
// Test 20: send_frame action sends correct frame via adapter
// =============================================================================

TEST_CASE("send_frame action sends correct frame via adapter", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "send_test";

    c_script_step step;
    step.label = "send";
    step.trigger_type = e_trigger_type::immediate;
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = 0x321;
    step.action.frame.dlc = 4;
    step.action.frame.data = {0xDE, 0xAD, 0xBE, 0xEF, 0, 0, 0, 0};
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();
    engine.process(1000);

    auto tx = adapter->get_tx_history();
    REQUIRE(tx.size() == 1);
    CHECK(tx[0].id == 0x321);
    CHECK(tx[0].dlc == 4);
    CHECK(tx[0].data[0] == 0xDE);
    CHECK(tx[0].data[1] == 0xAD);
    CHECK(tx[0].data[2] == 0xBE);
    CHECK(tx[0].data[3] == 0xEF);
}

// =============================================================================
// Test 21: send_sequence sends all frames
// =============================================================================

TEST_CASE("send_sequence action sends all frames", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "seq_send";

    c_script_step step;
    step.label = "multi";
    step.trigger_type = e_trigger_type::immediate;
    step.action.type = e_action_type::send_sequence;

    c_script_action::sequence_entry e1;
    e1.frame.id = 0x100;
    e1.frame.dlc = 1;
    e1.frame.data[0] = 0x01;
    e1.delay_before = std::chrono::microseconds(0);

    c_script_action::sequence_entry e2;
    e2.frame.id = 0x100;
    e2.frame.dlc = 1;
    e2.frame.data[0] = 0x02;
    e2.delay_before = std::chrono::microseconds(5000);

    c_script_action::sequence_entry e3;
    e3.frame.id = 0x100;
    e3.frame.dlc = 1;
    e3.frame.data[0] = 0x03;
    e3.delay_before = std::chrono::microseconds(5000);

    step.action.sequence = {e1, e2, e3};
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();
    engine.process(1000);

    auto tx = adapter->get_tx_history();
    REQUIRE(tx.size() == 3);
    CHECK(tx[0].data[0] == 0x01);
    CHECK(tx[1].data[0] == 0x02);
    CHECK(tx[2].data[0] == 0x03);
}

// =============================================================================
// Test 22: repeat=true re-arms the step trigger
// =============================================================================

TEST_CASE("repeat=true re-arms the step trigger", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "repeat_test";

    c_script_step step;
    step.label = "repeating";
    step.trigger_type = e_trigger_type::on_receive;
    step.match.id = 0x100;
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = 0x200;
    step.action.frame.dlc = 1;
    step.action.frame.data[0] = 0xAA;
    step.repeat = true;
    step.repeat_count = 0;  // infinite
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();

    // First trigger
    engine.on_frame_received(make_frame(0x100, {}, 1000));
    engine.process(1000);
    CHECK(adapter->get_tx_history().size() == 1);

    // Second trigger
    engine.on_frame_received(make_frame(0x100, {}, 2000));
    engine.process(2000);
    CHECK(adapter->get_tx_history().size() == 2);

    // Third trigger
    engine.on_frame_received(make_frame(0x100, {}, 3000));
    engine.process(3000);
    CHECK(adapter->get_tx_history().size() == 3);

    // Still running (infinite repeat)
    CHECK(engine.state() == e_engine_state::running);
}

// =============================================================================
// Test 23: repeat_count limits repetitions
// =============================================================================

TEST_CASE("repeat_count limits number of repetitions", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "repeat_limit";

    c_script_step step;
    step.label = "limited";
    step.trigger_type = e_trigger_type::on_receive;
    step.match.id = 0x100;
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = 0x200;
    step.action.frame.dlc = 1;
    step.action.frame.data[0] = 0xBB;
    step.repeat = true;
    step.repeat_count = 2;
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();

    // First repeat
    engine.on_frame_received(make_frame(0x100, {}, 1000));
    engine.process(1000);
    CHECK(adapter->get_tx_history().size() == 1);
    CHECK(engine.state() == e_engine_state::running);

    // Second repeat -- should be the last
    engine.on_frame_received(make_frame(0x100, {}, 2000));
    engine.process(2000);
    CHECK(adapter->get_tx_history().size() == 2);

    // After repeat_count reached, should advance past the step
    engine.process(3000);
    CHECK(engine.state() == e_engine_state::finished);
}

// =============================================================================
// Test 24: Script loop=true restarts from beginning
// =============================================================================

TEST_CASE("Script loop=true restarts from beginning", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "loop_test";
    script.loop = true;

    c_script_step step;
    step.label = "send_loop";
    step.trigger_type = e_trigger_type::immediate;
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = 0x300;
    step.action.frame.dlc = 1;
    step.action.frame.data[0] = 0xCC;
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();

    // First iteration
    engine.process(1000);
    CHECK(adapter->get_tx_history().size() == 1);

    // Second iteration (looped back)
    engine.process(2000);
    CHECK(adapter->get_tx_history().size() == 2);

    // Third iteration
    engine.process(3000);
    CHECK(adapter->get_tx_history().size() == 3);

    // Should still be running
    CHECK(engine.state() == e_engine_state::running);
}

// =============================================================================
// Test 25: Engine stop() halts execution
// =============================================================================

TEST_CASE("Engine stop halts execution", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "stop_test";
    script.loop = true;

    c_script_step step;
    step.label = "continuous";
    step.trigger_type = e_trigger_type::immediate;
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = 0x400;
    step.action.frame.dlc = 1;
    step.action.frame.data[0] = 0xDD;
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();

    engine.process(1000);
    auto count_before = adapter->get_tx_history().size();
    REQUIRE(count_before > 0);

    engine.stop();
    CHECK(engine.state() == e_engine_state::idle);

    // Process should do nothing after stop
    engine.process(2000);
    CHECK(adapter->get_tx_history().size() == count_before);
}

// =============================================================================
// Test 26: Engine pause/resume
// =============================================================================

TEST_CASE("Engine pause and resume suspends and continues", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    c_script script;
    script.name = "pause_test";

    c_script_step step;
    step.label = "delayed";
    step.trigger_type = e_trigger_type::delay;
    step.delay = std::chrono::microseconds(500'000);
    step.action.type = e_action_type::send_frame;
    step.action.frame.id = 0x500;
    step.action.frame.dlc = 1;
    step.action.frame.data[0] = 0xEE;
    script.steps.push_back(std::move(step));

    engine.load_script(std::move(script));
    (void)engine.start();

    engine.process(100'000);
    CHECK(engine.state() == e_engine_state::running);

    engine.pause();
    CHECK(engine.state() == e_engine_state::paused);

    // Process while paused -- should do nothing
    engine.process(700'000);
    CHECK(adapter->get_tx_history().empty());

    engine.resume();
    CHECK(engine.state() == e_engine_state::running);

    // Now process -- delay should trigger based on original start time
    engine.process(700'000);
    CHECK(adapter->get_tx_history().size() == 1);
}

// =============================================================================
// Test 27: event_log records all events
// =============================================================================

TEST_CASE("event_log records all events", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    engine.load_script(make_immediate_send_script(0x100, {0x01}));
    (void)engine.start();

    engine.process(1000);
    engine.process(2000);  // triggers finish

    const auto& log = engine.event_log();
    REQUIRE(log.size() >= 3);  // step_started + step_completed + script_finished

    bool has_started = false;
    bool has_completed = false;
    bool has_finished = false;
    for (const auto& e : log) {
        if (e.type == c_engine_event::e_type::step_started) has_started = true;
        if (e.type == c_engine_event::e_type::step_completed) has_completed = true;
        if (e.type == c_engine_event::e_type::script_finished) has_finished = true;
    }
    CHECK(has_started);
    CHECK(has_completed);
    CHECK(has_finished);
}

// =============================================================================
// Test 28: event_callback fires on each engine event
// =============================================================================

TEST_CASE("event_callback fires on each engine event", "[can_script][engine]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    std::vector<c_engine_event> callback_events;
    engine.set_event_callback([&](const c_engine_event& e) {
        callback_events.push_back(e);
    });

    engine.load_script(make_immediate_send_script(0x100, {0x01}));
    (void)engine.start();
    engine.process(1000);
    engine.process(2000);

    REQUIRE(callback_events.size() >= 3);

    // Callback events should match event_log
    const auto& log = engine.event_log();
    REQUIRE(callback_events.size() == log.size());
    for (std::size_t i = 0; i < log.size(); ++i) {
        CHECK(callback_events[i].type == log[i].type);
        CHECK(callback_events[i].step_label == log[i].step_label);
    }
}

// =============================================================================
// Test 29: Complete CANopen node simulation (NMT + SDO)
// =============================================================================

TEST_CASE("Complete CANopen node simulation scenario", "[can_script][engine][integration]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    // Parse a CANopen simulation script from JSON
    auto j = nlohmann::json::parse(R"({
        "name": "CANopen Node 5",
        "description": "Simulates CANopen node responding to NMT start",
        "loop": false,
        "steps": [
            {
                "label": "wait_for_nmt_start",
                "trigger": {
                    "type": "on_receive",
                    "match": {
                        "id": "0x000",
                        "payload": [
                            {"type": "exact", "value": "0x01"},
                            {"type": "exact", "value": "0x05"}
                        ]
                    }
                },
                "action": {
                    "type": "send_frame",
                    "frame": {"id": "0x705", "dlc": 1, "data": ["0x00"]}
                }
            },
            {
                "label": "sdo_response",
                "trigger": {
                    "type": "on_receive",
                    "match": {
                        "id": "0x605",
                        "payload": [
                            {"type": "exact", "value": "0x40"},
                            {"type": "exact", "value": "0x18"},
                            {"type": "exact", "value": "0x10"},
                            {"type": "exact", "value": "0x01"}
                        ]
                    }
                },
                "action": {
                    "type": "send_frame",
                    "frame": {
                        "id": "0x585",
                        "dlc": 8,
                        "data": ["0x43", "0x18", "0x10", "0x01", "0xAB", "0xCD", "0xEF", "0x01"]
                    }
                }
            }
        ]
    })");

    auto script_result = c_script::from_json(j);
    REQUIRE(script_result.has_value());

    engine.load_script(std::move(*script_result));
    (void)engine.start();

    // Process: waiting for NMT
    engine.process(1000);
    CHECK(adapter->get_tx_history().empty());

    // Send NMT start command for node 5
    engine.on_frame_received(make_frame(0x000, {0x01, 0x05}, 2000));
    engine.process(2000);

    // Should respond with bootup (0x705, 0x00)
    auto tx = adapter->get_tx_history();
    REQUIRE(tx.size() == 1);
    CHECK(tx[0].id == 0x705);
    CHECK(tx[0].data[0] == 0x00);

    // Now waiting for SDO request
    engine.process(3000);

    // Send SDO upload request (read 0x1018:01)
    engine.on_frame_received(
        make_frame(0x605, {0x40, 0x18, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00}, 4000));
    engine.process(4000);

    tx = adapter->get_tx_history();
    REQUIRE(tx.size() == 2);
    CHECK(tx[1].id == 0x585);
    CHECK(tx[1].data[0] == 0x43);
    CHECK(tx[1].data[4] == 0xAB);
}

// =============================================================================
// Test 30: Complete UDS server simulation (DiagSession + ReadByID multi-frame)
// =============================================================================

TEST_CASE("Complete UDS server simulation scenario", "[can_script][engine][integration]") {
    auto adapter = make_adapter();
    c_script_engine engine(adapter);

    auto j = nlohmann::json::parse(R"({
        "name": "UDS Server Sim",
        "description": "Simulates UDS DiagSession and multi-frame ReadByID",
        "loop": false,
        "steps": [
            {
                "label": "diag_session_response",
                "trigger": {
                    "type": "on_receive",
                    "match": {
                        "id": "0x7E0",
                        "payload": [{"type": "exact", "value": "0x10"}]
                    }
                },
                "action": {
                    "type": "send_frame",
                    "frame": {
                        "id": "0x7E8",
                        "dlc": 6,
                        "data": ["0x50", "0x01", "0x00", "0x32", "0x01", "0xF4"]
                    }
                }
            },
            {
                "label": "read_by_id_multiframe",
                "trigger": {
                    "type": "on_receive",
                    "match": {
                        "id": "0x7E0",
                        "payload": [{"type": "exact", "value": "0x22"}]
                    }
                },
                "action": {
                    "type": "send_sequence",
                    "sequence": [
                        {
                            "delay_ms": 0,
                            "frame": {"id": "0x7E8", "dlc": 8, "data": ["0x10", "0x14", "0x62", "0xF1", "0x90", "0x57", "0x30", "0x4C"]}
                        },
                        {
                            "delay_ms": 5,
                            "frame": {"id": "0x7E8", "dlc": 8, "data": ["0x21", "0x5A", "0x4E", "0x44", "0x41", "0x31", "0x32", "0x33"]}
                        },
                        {
                            "delay_ms": 5,
                            "frame": {"id": "0x7E8", "dlc": 8, "data": ["0x22", "0x34", "0x35", "0x36", "0x37", "0x38", "0x39", "0x30"]}
                        }
                    ]
                }
            }
        ]
    })");

    auto script_result = c_script::from_json(j);
    REQUIRE(script_result.has_value());

    engine.load_script(std::move(*script_result));
    (void)engine.start();

    // Send DiagSession request
    engine.on_frame_received(make_frame(0x7E0, {0x10, 0x01}, 1000));
    engine.process(1000);

    auto tx = adapter->get_tx_history();
    REQUIRE(tx.size() == 1);
    CHECK(tx[0].id == 0x7E8);
    CHECK(tx[0].data[0] == 0x50);  // positive response

    // Now send ReadByIdentifier request
    engine.process(2000);
    engine.on_frame_received(make_frame(0x7E0, {0x22, 0xF1, 0x90}, 3000));
    engine.process(3000);

    tx = adapter->get_tx_history();
    REQUIRE(tx.size() == 4);  // 1 DiagSession + 3 ISO-TP frames

    // Verify first frame of multi-frame response
    CHECK(tx[1].id == 0x7E8);
    CHECK(tx[1].data[0] == 0x10);  // First Frame indicator
    CHECK(tx[1].data[2] == 0x62);  // ReadByIdentifier positive response SID

    // Verify consecutive frames
    CHECK(tx[2].data[0] == 0x21);  // CF sequence number 1
    CHECK(tx[3].data[0] == 0x22);  // CF sequence number 2
}
