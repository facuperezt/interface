#pragma once

/// @file pdo.hpp
/// @brief CANopen PDO (Process Data Object) mapping.

#include "interface/canopen/object_dictionary.hpp"
#include "interface/can/frame.hpp"
#include "interface/core/types.hpp"

#include <cstdint>
#include <vector>

namespace interface::canopen {

/// A single PDO mapping entry: which OD entry/sub-index and how many bits.
struct c_pdo_map_entry {
    std::uint16_t index{0};
    std::uint8_t sub_index{0};
    std::uint8_t bit_length{0};
};

/// PDO transmission type.
enum class e_pdo_transmission_type : std::uint8_t {
    synchronous_acyclic  = 0x00,
    synchronous_cyclic_1 = 0x01,   // Every SYNC
    // 0x02–0xF0: every Nth SYNC
    event_driven_mfr     = 0xFE,
    event_driven         = 0xFF,
};

/// PDO configuration (TPDO or RPDO).
struct c_pdo_config {
    std::uint8_t pdo_number{0};     ///< PDO number (0-based)
    bool enabled{false};
    can_id_t cob_id{0};
    e_pdo_transmission_type transmission_type{e_pdo_transmission_type::event_driven};
    std::uint16_t inhibit_time_100us{0};
    std::uint16_t event_timer_ms{0};
    std::vector<c_pdo_map_entry> mapping;

    /// Total mapped bit length.
    [[nodiscard]] auto total_bits() const noexcept -> std::uint32_t;

    /// Total mapped byte length (rounded up).
    [[nodiscard]] auto total_bytes() const noexcept -> std::uint8_t;
};

/// Decode a PDO frame using its mapping and the Object Dictionary.
/// Returns the decoded values as a vector of (index, sub_index, value) tuples.
struct c_pdo_decoded_value {
    std::uint16_t index;
    std::uint8_t sub_index;
    std::string name;
    double value;
};

[[nodiscard]] auto decode_pdo(
    const can::c_can_frame& frame,
    const c_pdo_config& config,
    const c_object_dictionary& od
) -> result_t<std::vector<c_pdo_decoded_value>>;

} // namespace interface::canopen
