#pragma once

/// @file nmt.hpp
/// @brief CANopen NMT (Network Management) state machine.

#include "interface/core/types.hpp"

#include <cstdint>
#include <string>

namespace interface::canopen {

/// NMT states (CiA 301 §7.3.2).
enum class e_nmt_state : std::uint8_t {
    initialising    = 0x00,
    stopped         = 0x04,
    operational     = 0x05,
    pre_operational = 0x7F,
};

/// NMT commands sent to slaves.
enum class e_nmt_command : std::uint8_t {
    start_remote_node      = 0x01,
    stop_remote_node       = 0x02,
    enter_pre_operational  = 0x80,
    reset_node             = 0x81,
    reset_communication    = 0x82,
};

/// Get human-readable name for an NMT state.
[[nodiscard]] auto nmt_state_to_string(e_nmt_state state) -> std::string;

/// Get human-readable name for an NMT command.
[[nodiscard]] auto nmt_command_to_string(e_nmt_command cmd) -> std::string;

} // namespace interface::canopen
