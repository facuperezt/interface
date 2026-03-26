#pragma once

/// @file script.hpp
/// @brief CAN script data model -- defines scripts, steps, triggers, actions, and frame matching.

#include "interface/can/frame.hpp"
#include "interface/can/sequence_detector.hpp"
#include "interface/core/error.hpp"
#include "interface/core/types.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace interface::can_script {

// =============================================================================
// Trigger types
// =============================================================================

enum class e_trigger_type {
    immediate,
    delay,
    on_receive,
    on_receive_or_timeout,
};

// =============================================================================
// Frame matching
// =============================================================================

struct c_frame_match {
    can_id_t id{0};
    can_id_t id_mask{0x1FFFFFFF};
    std::vector<can::c_byte_matcher> payload_matchers;

    [[nodiscard]] auto matches(const can::c_can_frame& frame) const -> bool;
};

// =============================================================================
// Action types
// =============================================================================

enum class e_action_type {
    send_frame,
    send_sequence,
    log_message,
    set_variable,
    no_op,
};

// =============================================================================
// Frame template
// =============================================================================

struct c_frame_template {
    can_id_t id{0};
    std::uint8_t dlc{8};
    std::array<byte_t, 8> data{};
    can::c_frame_flags flags{};

    [[nodiscard]] auto to_frame(timestamp_us_t ts = 0) const -> can::c_can_frame;
};

// =============================================================================
// Script action
// =============================================================================

struct c_script_action {
    e_action_type type{e_action_type::send_frame};

    // For send_frame
    c_frame_template frame{};

    // For send_sequence
    struct sequence_entry {
        c_frame_template frame;
        std::chrono::microseconds delay_before{0};
    };
    std::vector<sequence_entry> sequence;

    // For log_message
    std::string message;

    // For set_variable
    std::string variable_name;
    std::string variable_value;
};

// =============================================================================
// Script step
// =============================================================================

struct c_script_step {
    std::string label;

    // Trigger
    e_trigger_type trigger_type{e_trigger_type::immediate};
    std::chrono::microseconds delay{0};
    c_frame_match match{};
    std::chrono::microseconds timeout{0};

    // Action
    c_script_action action{};

    // Flow control
    std::string on_timeout_goto;
    std::string on_match_goto;
    bool repeat{false};
    std::uint32_t repeat_count{0};
};

// =============================================================================
// Script
// =============================================================================

struct c_script {
    std::string name;
    std::string description;
    std::vector<c_script_step> steps;
    bool loop{false};

    static auto from_json(const nlohmann::json& j) -> result_t<c_script>;
    [[nodiscard]] auto to_json() const -> nlohmann::json;
    static auto load_from_file(const std::filesystem::path& path) -> result_t<c_script>;
    [[nodiscard]] auto save_to_file(const std::filesystem::path& path) const -> result_t<void>;
};

} // namespace interface::can_script
