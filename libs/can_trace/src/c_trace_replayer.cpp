/// @file c_trace_replayer.cpp
/// @brief Trace replay engine implementation.

#include "interface/can_trace/c_trace_replayer.hpp"

#include <thread>

namespace interface::can_trace {

c_trace_replayer::c_trace_replayer(
    std::shared_ptr<i_trace_reader> reader,
    std::shared_ptr<can_hal::i_can_adapter> adapter
)
    : m_reader{std::move(reader)}
    , m_adapter{std::move(adapter)} {}

auto c_trace_replayer::set_speed_multiplier(double multiplier) -> void {
    m_speed_multiplier = multiplier;
}

auto c_trace_replayer::replay_all() -> void_result_t {
    while (true) {
        auto result = replay_next();
        if (!result) {
            return std::unexpected(result.error());
        }
        if (!result.value()) {
            break; // No more frames
        }
    }
    return {};
}

auto c_trace_replayer::replay_next() -> result_t<bool> {
    auto frame_result = m_reader->read_next();
    if (!frame_result) {
        return std::unexpected(frame_result.error());
    }

    if (!frame_result->has_value()) {
        return false; // EOF
    }

    auto& frame = frame_result->value();

    // Apply inter-frame delay based on timestamp differences
    if (m_last_timestamp >= 0 && m_speed_multiplier > 0.0) {
        auto delta_us = frame.timestamp - m_last_timestamp;
        if (delta_us > 0) {
            auto delay_us = static_cast<std::int64_t>(
                static_cast<double>(delta_us) / m_speed_multiplier
            );
            std::this_thread::sleep_for(std::chrono::microseconds(delay_us));
        }
    }

    // Fire callback before sending
    if (m_frame_callback) {
        m_frame_callback(frame);
    }

    auto send_result = m_adapter->send(frame);
    if (!send_result) {
        return std::unexpected(send_result.error());
    }

    m_last_timestamp = frame.timestamp;
    ++m_frames_replayed;
    return true;
}

auto c_trace_replayer::frames_replayed() const noexcept -> std::size_t {
    return m_frames_replayed;
}

auto c_trace_replayer::set_frame_callback(replay_callback_t callback) -> void {
    m_frame_callback = std::move(callback);
}

} // namespace interface::can_trace
