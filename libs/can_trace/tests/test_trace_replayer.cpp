#include <catch2/catch_test_macros.hpp>
#include "interface/can_trace/c_trace_replayer.hpp"
#include "interface/can_hal/c_mock_adapter.hpp"

#include <fstream>
#include <filesystem>

// A minimal in-memory trace reader for testing
namespace {

class c_mock_trace_reader final : public interface::can_trace::i_trace_reader {
public:
    auto add_frame(const interface::can::c_can_frame& frame) -> void {
        m_frames.push_back(frame);
    }

    [[nodiscard]] auto open(const std::filesystem::path& /*path*/) -> interface::void_result_t override {
        m_index = 0;
        return {};
    }

    [[nodiscard]] auto read_next() -> interface::result_t<std::optional<interface::can::c_can_frame>> override {
        if (m_index >= m_frames.size()) {
            return std::optional<interface::can::c_can_frame>{std::nullopt};
        }
        return m_frames[m_index++];
    }

    [[nodiscard]] auto read_all() -> interface::result_t<std::vector<interface::can::c_can_frame>> override {
        m_index = m_frames.size();
        return m_frames;
    }

    [[nodiscard]] auto info() const -> interface::can_trace::c_trace_info override {
        return interface::can_trace::c_trace_info{.format = "MOCK"};
    }

    [[nodiscard]] auto supported_extensions() const -> std::vector<std::string> override {
        return {};
    }

    [[nodiscard]] auto reset() -> interface::void_result_t override {
        m_index = 0;
        return {};
    }

private:
    std::vector<interface::can::c_can_frame> m_frames;
    std::size_t m_index{0};
};

auto make_frame(interface::can_id_t id, std::uint8_t dlc, interface::timestamp_us_t ts)
    -> interface::can::c_can_frame {
    interface::can::c_can_frame f{};
    f.id = id;
    f.dlc = dlc;
    f.timestamp = ts;
    f.data[0] = static_cast<interface::byte_t>(id & 0xFF);
    return f;
}

} // anonymous namespace

using namespace interface;
using namespace interface::can_trace;

TEST_CASE("Trace replayer replays all frames", "[can_trace][replayer]") {
    auto reader = std::make_shared<c_mock_trace_reader>();
    reader->add_frame(make_frame(0x100, 2, 0));
    reader->add_frame(make_frame(0x200, 4, 1000));
    reader->add_frame(make_frame(0x300, 8, 2000));

    auto adapter = std::make_shared<can_hal::c_mock_adapter>();
    adapter->open(can_hal::c_bitrate_config{});

    c_trace_replayer replayer(reader, adapter);
    replayer.set_speed_multiplier(0.0); // As fast as possible

    auto result = replayer.replay_all();
    REQUIRE(result.has_value());
    REQUIRE(replayer.frames_replayed() == 3);

    auto history = adapter->get_tx_history();
    REQUIRE(history.size() == 3);
    REQUIRE(history[0].id == 0x100);
    REQUIRE(history[1].id == 0x200);
    REQUIRE(history[2].id == 0x300);
}

TEST_CASE("Trace replayer replay_next steps through frames", "[can_trace][replayer]") {
    auto reader = std::make_shared<c_mock_trace_reader>();
    reader->add_frame(make_frame(0x100, 2, 0));
    reader->add_frame(make_frame(0x200, 4, 1000));

    auto adapter = std::make_shared<can_hal::c_mock_adapter>();
    adapter->open(can_hal::c_bitrate_config{});

    c_trace_replayer replayer(reader, adapter);
    replayer.set_speed_multiplier(0.0);

    auto r1 = replayer.replay_next();
    REQUIRE(r1.has_value());
    REQUIRE(*r1 == true);
    REQUIRE(replayer.frames_replayed() == 1);

    auto r2 = replayer.replay_next();
    REQUIRE(r2.has_value());
    REQUIRE(*r2 == true);
    REQUIRE(replayer.frames_replayed() == 2);

    // EOF
    auto r3 = replayer.replay_next();
    REQUIRE(r3.has_value());
    REQUIRE(*r3 == false);
    REQUIRE(replayer.frames_replayed() == 2);
}

TEST_CASE("Trace replayer callback fires before each frame", "[can_trace][replayer]") {
    auto reader = std::make_shared<c_mock_trace_reader>();
    reader->add_frame(make_frame(0x100, 2, 0));
    reader->add_frame(make_frame(0x200, 4, 1000));

    auto adapter = std::make_shared<can_hal::c_mock_adapter>();
    adapter->open(can_hal::c_bitrate_config{});

    c_trace_replayer replayer(reader, adapter);
    replayer.set_speed_multiplier(0.0);

    std::vector<can_id_t> callback_ids;
    replayer.set_frame_callback([&](const can::c_can_frame& frame) {
        callback_ids.push_back(frame.id);
    });

    replayer.replay_all();
    REQUIRE(callback_ids.size() == 2);
    REQUIRE(callback_ids[0] == 0x100);
    REQUIRE(callback_ids[1] == 0x200);
}

TEST_CASE("Trace replayer empty trace", "[can_trace][replayer]") {
    auto reader = std::make_shared<c_mock_trace_reader>();
    auto adapter = std::make_shared<can_hal::c_mock_adapter>();
    adapter->open(can_hal::c_bitrate_config{});

    c_trace_replayer replayer(reader, adapter);
    replayer.set_speed_multiplier(0.0);

    auto result = replayer.replay_all();
    REQUIRE(result.has_value());
    REQUIRE(replayer.frames_replayed() == 0);
}

TEST_CASE("Trace replayer speed multiplier affects timing", "[can_trace][replayer]") {
    auto reader = std::make_shared<c_mock_trace_reader>();
    // 100ms gap between frames
    reader->add_frame(make_frame(0x100, 2, 0));
    reader->add_frame(make_frame(0x200, 2, 100'000)); // 100ms later

    auto adapter = std::make_shared<can_hal::c_mock_adapter>();
    adapter->open(can_hal::c_bitrate_config{});

    // At 100x speed, 100ms gap → 1ms actual delay
    c_trace_replayer replayer(reader, adapter);
    replayer.set_speed_multiplier(100.0);

    auto start = std::chrono::steady_clock::now();
    replayer.replay_all();
    auto elapsed = std::chrono::steady_clock::now() - start;

    // Should take roughly 1ms (100ms / 100x), definitely less than 50ms
    REQUIRE(elapsed < std::chrono::milliseconds(50));
    REQUIRE(replayer.frames_replayed() == 2);
}
