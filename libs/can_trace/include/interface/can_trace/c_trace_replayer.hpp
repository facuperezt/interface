#pragma once

/// @file c_trace_replayer.hpp
/// @brief Trace replay engine — replays captured CAN frames through an adapter.

#include "interface/can_trace/i_trace_reader.hpp"
#include "interface/can_hal/i_can_adapter.hpp"
#include "interface/core/error.hpp"

#include <cstddef>
#include <functional>
#include <memory>

namespace interface::can_trace {

/// Callback fired before each frame is sent: (frame) -> void.
using replay_callback_t = std::function<void(const can::c_can_frame&)>;

/// Replays a trace file through a CAN adapter with configurable timing.
///
/// The replayer reads frames from an i_trace_reader and sends them through
/// an i_can_adapter, respecting the original inter-frame timing scaled by
/// a speed multiplier.
class c_trace_replayer {
public:
    /// Construct with a trace reader and a CAN adapter.
    c_trace_replayer(
        std::shared_ptr<i_trace_reader> reader,
        std::shared_ptr<can_hal::i_can_adapter> adapter
    );

    /// Set the replay speed multiplier.
    /// 1.0 = original timing, 2.0 = 2x speed, 0.0 = as fast as possible.
    auto set_speed_multiplier(double multiplier) -> void;

    /// Replay all remaining frames (blocking).
    [[nodiscard]] auto replay_all() -> void_result_t;

    /// Replay the next single frame (step mode).
    [[nodiscard]] auto replay_next() -> result_t<bool>;

    /// Number of frames replayed so far.
    [[nodiscard]] auto frames_replayed() const noexcept -> std::size_t;

    /// Register a callback fired before each frame is sent.
    auto set_frame_callback(replay_callback_t callback) -> void;

private:
    std::shared_ptr<i_trace_reader> m_reader;
    std::shared_ptr<can_hal::i_can_adapter> m_adapter;
    double m_speed_multiplier{1.0};
    std::size_t m_frames_replayed{0};
    timestamp_us_t m_last_timestamp{-1};
    replay_callback_t m_frame_callback;
};

} // namespace interface::can_trace
