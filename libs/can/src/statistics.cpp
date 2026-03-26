/// @file statistics.cpp
/// @brief CAN bus statistics collector implementation.

#include "interface/can/statistics.hpp"

#include <algorithm>
#include <numeric>

namespace interface::can {

c_bus_statistics::c_bus_statistics(std::uint32_t nominal_bitrate_bps, timestamp_us_t window_us)
    : m_bitrate_bps{nominal_bitrate_bps}
    , m_window_us{window_us} {}

auto c_bus_statistics::record(const c_can_frame& frame) -> void {
    m_records.push_back(frame_record{
        .id = frame.id,
        .timestamp = frame.timestamp,
        .dlc = frame.dlc,
    });
    m_id_timestamps[frame.id].push_back(frame.timestamp);
}

auto c_bus_statistics::prune_window() const -> void {
    if (m_window_us <= 0 || m_records.empty()) {
        return;
    }

    auto cutoff = m_records.back().timestamp - m_window_us;

    // Prune main records
    while (!m_records.empty() && m_records.front().timestamp < cutoff) {
        auto pruned_id = m_records.front().id;
        m_records.pop_front();

        // Prune per-ID timestamps
        auto it = m_id_timestamps.find(pruned_id);
        if (it != m_id_timestamps.end()) {
            while (!it->second.empty() && it->second.front() < cutoff) {
                it->second.pop_front();
            }
            if (it->second.empty()) {
                m_id_timestamps.erase(it);
            }
        }
    }
}

auto c_bus_statistics::frame_count() const -> std::size_t {
    prune_window();
    return m_records.size();
}

auto c_bus_statistics::frame_count(can_id_t id) const -> std::size_t {
    prune_window();
    auto it = m_id_timestamps.find(id);
    if (it == m_id_timestamps.end()) {
        return 0;
    }
    return it->second.size();
}

auto c_bus_statistics::bus_load_percent() const -> double {
    prune_window();

    if (m_records.size() < 2 || m_bitrate_bps == 0) {
        return 0.0;
    }

    auto time_span_us = m_records.back().timestamp - m_records.front().timestamp;
    if (time_span_us <= 0) {
        return 0.0;
    }

    // Estimate total bits on the bus.
    // A standard CAN frame is approximately: SOF(1) + ID(11) + RTR(1) + IDE(1) + r0(1) +
    // DLC(4) + Data(0-64) + CRC(15) + CRC_del(1) + ACK(2) + EOF(7) + IFS(3) = ~47 + 8*DLC bits.
    // Plus ~20% overhead from bit stuffing on average.
    std::uint64_t total_bits = 0;
    for (const auto& rec : m_records) {
        auto data_bits = static_cast<std::uint64_t>(rec.dlc <= 8 ? rec.dlc : 8) * 8;
        auto frame_bits = 47 + data_bits;
        // ~20% bit-stuffing overhead
        total_bits += frame_bits + frame_bits / 5;
    }

    auto time_span_sec = static_cast<double>(time_span_us) / 1'000'000.0;
    auto capacity_bits = static_cast<double>(m_bitrate_bps) * time_span_sec;

    if (capacity_bits <= 0.0) {
        return 0.0;
    }

    auto load = (static_cast<double>(total_bits) / capacity_bits) * 100.0;
    return std::min(load, 100.0);
}

auto c_bus_statistics::delta_stats(can_id_t id) const -> c_delta_stats {
    prune_window();

    auto it = m_id_timestamps.find(id);
    if (it == m_id_timestamps.end() || it->second.size() < 2) {
        return c_delta_stats{};
    }

    const auto& timestamps = it->second;
    timestamp_us_t min_delta = std::numeric_limits<timestamp_us_t>::max();
    timestamp_us_t max_delta = 0;
    timestamp_us_t total_delta = 0;
    std::size_t count = 0;

    for (std::size_t i = 1; i < timestamps.size(); ++i) {
        auto delta = timestamps[i] - timestamps[i - 1];
        if (delta < min_delta) min_delta = delta;
        if (delta > max_delta) max_delta = delta;
        total_delta += delta;
        ++count;
    }

    return c_delta_stats{
        .min_us = min_delta,
        .max_us = max_delta,
        .avg_us = count > 0 ? total_delta / static_cast<timestamp_us_t>(count) : 0,
    };
}

auto c_bus_statistics::reset() -> void {
    m_records.clear();
    m_id_timestamps.clear();
}

} // namespace interface::can
