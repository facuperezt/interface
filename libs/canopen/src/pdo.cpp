/// @file pdo.cpp
/// @brief PDO mapping and decoding implementation.

#include "interface/canopen/pdo.hpp"

namespace interface::canopen {

auto c_pdo_config::total_bits() const noexcept -> std::uint32_t {
    std::uint32_t bits = 0;
    for (const auto& entry : mapping) {
        bits += entry.bit_length;
    }
    return bits;
}

auto c_pdo_config::total_bytes() const noexcept -> std::uint8_t {
    auto bits = total_bits();
    return static_cast<std::uint8_t>((bits + 7) / 8);
}

auto decode_pdo(
    const can::c_can_frame& frame,
    const c_pdo_config& config,
    const c_object_dictionary& od
) -> result_t<std::vector<c_pdo_decoded_value>> {
    if (!config.enabled) {
        return make_error("PDO not enabled", e_error_category::protocol);
    }

    std::vector<c_pdo_decoded_value> values;
    std::uint32_t bit_offset = 0;

    for (const auto& map_entry : config.mapping) {
        // Find the OD entry for this mapping
        auto od_entry = od.find(map_entry.index);
        std::string name = "unknown";
        if (od_entry) {
            auto sub = od_entry->get().find_sub(map_entry.sub_index);
            if (sub) {
                name = sub->get().name;
            } else {
                name = od_entry->get().name;
            }
        }

        // Extract raw value from frame data at bit_offset
        std::uint64_t raw = 0;
        for (std::uint8_t i = 0; i < map_entry.bit_length; ++i) {
            auto abs_bit = bit_offset + i;
            auto byte_idx = abs_bit / 8;
            auto bit_idx = abs_bit % 8;
            if (byte_idx < frame.data_length()) {
                if ((frame.data[byte_idx] >> bit_idx) & 1U) {
                    raw |= (1ULL << i);
                }
            }
        }

        values.push_back(c_pdo_decoded_value{
            .index = map_entry.index,
            .sub_index = map_entry.sub_index,
            .name = std::move(name),
            .value = static_cast<double>(raw),
        });

        bit_offset += map_entry.bit_length;
    }

    return values;
}

} // namespace interface::canopen
