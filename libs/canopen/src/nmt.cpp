/// @file nmt.cpp
/// @brief NMT state/command string utilities.

#include "interface/canopen/nmt.hpp"

#include <format>

namespace interface::canopen {

auto nmt_state_to_string(e_nmt_state state) -> std::string {
    switch (state) {
        case e_nmt_state::initialising:    return "Initialising";
        case e_nmt_state::stopped:         return "Stopped";
        case e_nmt_state::operational:     return "Operational";
        case e_nmt_state::pre_operational: return "Pre-operational";
    }
    return std::format("Unknown(0x{:02X})", static_cast<std::uint8_t>(state));
}

auto nmt_command_to_string(e_nmt_command cmd) -> std::string {
    switch (cmd) {
        case e_nmt_command::start_remote_node:     return "Start Remote Node";
        case e_nmt_command::stop_remote_node:       return "Stop Remote Node";
        case e_nmt_command::enter_pre_operational:  return "Enter Pre-operational";
        case e_nmt_command::reset_node:             return "Reset Node";
        case e_nmt_command::reset_communication:    return "Reset Communication";
    }
    return std::format("Unknown(0x{:02X})", static_cast<std::uint8_t>(cmd));
}

} // namespace interface::canopen
