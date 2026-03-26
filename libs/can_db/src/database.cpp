/// @file database.cpp
/// @brief c_database and c_signal_decoder implementations.

#include "interface/can_db/i_database_parser.hpp"

#include <algorithm>
#include <cstring>

namespace interface::can_db {

auto c_database::find_message(std::uint32_t id) const
    -> std::optional<std::reference_wrapper<const c_message_def>>
{
    auto it = std::ranges::find_if(messages, [id](const c_message_def& msg) {
        return msg.id == id;
    });
    if (it != messages.end()) {
        return std::cref(*it);
    }
    return std::nullopt;
}

auto c_signal_decoder::decode(
    const c_signal_def& signal,
    byte_span_t data
) -> double {
    // Extract raw bits from the data based on start_bit, length, and byte order.
    // This is a simplified implementation for little-endian signals.
    if (data.empty() || signal.length == 0) {
        return 0.0;
    }

    std::uint64_t raw = 0;

    if (signal.byte_order == e_byte_order::little_endian) {
        // Intel byte order: LSB first
        for (std::uint32_t i = 0; i < signal.length; ++i) {
            auto bit_pos = signal.start_bit + i;
            auto byte_idx = bit_pos / 8;
            auto bit_idx  = bit_pos % 8;
            if (byte_idx < data.size()) {
                if ((data[byte_idx] >> bit_idx) & 1U) {
                    raw |= (1ULL << i);
                }
            }
        }
    } else {
        // Motorola byte order: MSB first
        // TODO: Full Motorola bit numbering implementation
        for (std::uint32_t i = 0; i < signal.length; ++i) {
            auto bit_pos = signal.start_bit - i;
            auto byte_idx = bit_pos / 8;
            auto bit_idx  = bit_pos % 8;
            if (byte_idx < data.size()) {
                if ((data[byte_idx] >> bit_idx) & 1U) {
                    raw |= (1ULL << (signal.length - 1 - i));
                }
            }
        }
    }

    // Apply sign extension for signed types
    double physical = 0.0;
    if (signal.value_type == e_value_type::signed_int && signal.length > 0) {
        auto sign_bit = 1ULL << (signal.length - 1);
        if (raw & sign_bit) {
            // Sign extend
            auto mask = (1ULL << signal.length) - 1;
            raw = raw | ~mask;
        }
        physical = static_cast<double>(static_cast<std::int64_t>(raw));
    } else {
        physical = static_cast<double>(raw);
    }

    return physical * signal.factor + signal.offset;
}

auto c_signal_decoder::encode(
    const c_signal_def& signal,
    double value,
    mutable_byte_span_t data
) -> void_result_t {
    if (signal.length == 0) {
        return make_error("Signal length is zero", e_error_category::parse);
    }

    // Reverse the factor/offset
    auto raw_double = (value - signal.offset) / signal.factor;
    auto raw = static_cast<std::uint64_t>(static_cast<std::int64_t>(raw_double));

    // Write bits (little-endian only for now)
    if (signal.byte_order == e_byte_order::little_endian) {
        for (std::uint32_t i = 0; i < signal.length; ++i) {
            auto bit_pos = signal.start_bit + i;
            auto byte_idx = bit_pos / 8;
            auto bit_idx  = bit_pos % 8;
            if (byte_idx < data.size()) {
                if ((raw >> i) & 1ULL) {
                    data[byte_idx] |= static_cast<byte_t>(1U << bit_idx);
                } else {
                    data[byte_idx] &= static_cast<byte_t>(~(1U << bit_idx));
                }
            }
        }
    } else {
        return make_error("Motorola encoding not yet implemented", e_error_category::parse);
    }

    return {};
}

} // namespace interface::can_db
