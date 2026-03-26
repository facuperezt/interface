#pragma once

/// @file statistics.hpp
/// @brief CAN bus statistics collector — tracks frame counts, bus load, and timing.

#include "interface/can/frame.hpp"
#include "interface/core/types.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <unordered_map>

namespace interface::can {

/// Delta-time statistics for frames with a given CAN ID.
struct c_delta_stats {
    timestamp_us_t min_us{0};
    timestamp_us_t max_us{0};
    timestamp_us_t avg_us{0};
};

/// Collects statistics from a CAN bus frame stream.
///
/// Tracks total frame count, per-ID counts, bus load estimation,
/// and min/max/average delta-time between consecutive frames per CAN ID.
/// Supports a sliding time window for recent-only statistics.
class c_bus_statistics {
public:
    /// Construct with a nominal bitrate (for bus load estimation)
    /// and an optional sliding window duration (0 = no window, keep all).
    explicit c_bus_statistics(
        std::uint32_t nominal_bitrate_bps = 500'000,
        timestamp_us_t window_us = 0
    );

    /// Record a received CAN frame.
    auto record(const c_can_frame& frame) -> void;

    /// Total number of recorded frames (within window if set).
    [[nodiscard]] auto frame_count() const -> std::size_t;

    /// Frame count for a specific CAN ID (within window if set).
    [[nodiscard]] auto frame_count(can_id_t id) const -> std::size_t;

    /// Estimated bus load as a percentage [0..100].
    [[nodiscard]] auto bus_load_percent() const -> double;

    /// Delta-time statistics for a specific CAN ID.
    /// Returns zeroed struct if fewer than 2 frames recorded for that ID.
    [[nodiscard]] auto delta_stats(can_id_t id) const -> c_delta_stats;

    /// Clear all collected statistics.
    auto reset() -> void;

private:
    auto prune_window() const -> void;

    struct frame_record {
        can_id_t id;
        timestamp_us_t timestamp;
        std::uint8_t dlc;
    };

    std::uint32_t m_bitrate_bps;
    timestamp_us_t m_window_us;

    mutable std::deque<frame_record> m_records;
    mutable std::unordered_map<can_id_t, std::deque<timestamp_us_t>> m_id_timestamps;
};

} // namespace interface::can
