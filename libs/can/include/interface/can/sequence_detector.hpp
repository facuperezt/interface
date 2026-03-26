#pragma once

/// @file sequence_detector.hpp
/// @brief CAN protocol sequence detector — detects multi-frame patterns in CAN traffic.

#include "interface/can/frame.hpp"
#include "interface/core/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace interface::can {

// =============================================================================
// Byte matcher — matches a single byte in a CAN payload
// =============================================================================

class c_byte_matcher {
public:
    enum class e_match_type : std::uint8_t {
        any,
        exact,
        masked,
        range,
    };

    /// Matches any byte value.
    static auto any() -> c_byte_matcher;

    /// Matches an exact byte value.
    static auto exact(byte_t value) -> c_byte_matcher;

    /// Matches (byte & mask) == (value & mask).
    static auto masked(byte_t value, byte_t mask) -> c_byte_matcher;

    /// Matches low <= byte <= high.
    static auto range(byte_t low, byte_t high) -> c_byte_matcher;

    /// Check if a byte matches this matcher.
    [[nodiscard]] auto matches(byte_t byte) const noexcept -> bool;

private:
    e_match_type m_type{e_match_type::any};
    byte_t m_value{0};
    byte_t m_mask{0xFF};
    byte_t m_low{0};
    byte_t m_high{0xFF};
};

// =============================================================================
// Sequence step — a single expected frame in a sequence
// =============================================================================

struct c_sequence_step {
    std::string label;                      ///< Human-readable label (e.g., "SecurityAccess SeedRequest")
    can_id_t id{0};                         ///< CAN ID to match
    can_id_t id_mask{0xFFFFFFFF};           ///< Mask for ID matching (0xFFFFFFFF = exact match)
    std::vector<c_byte_matcher> payload;    ///< Payload byte matchers (empty = match any payload)
    timestamp_us_t timeout_us{1'000'000};   ///< Max time after previous step (default 1s)
};

// =============================================================================
// Sequence rule — an ordered list of steps forming a protocol pattern
// =============================================================================

struct c_sequence_rule {
    std::string name;                       ///< Rule name (e.g., "UDS SecurityAccess Handshake")
    std::vector<c_sequence_step> steps;     ///< Ordered steps
    bool allow_interleaved{true};           ///< If true, non-matching frames don't abort the sequence
    bool repeatable{true};                  ///< If true, detector restarts watching after completion
};

// =============================================================================
// Sequence events
// =============================================================================

enum class e_sequence_event_type {
    sequence_started,
    step_matched,
    sequence_completed,
    step_timeout,
    unexpected_frame,
};

enum class e_sequence_severity {
    info,
    warning,
    error,
};

struct c_sequence_event {
    e_sequence_event_type type;
    e_sequence_severity severity;
    std::string rule_name;
    std::size_t step_index{0};
    std::string step_label;
    std::optional<c_can_frame> frame;       ///< The triggering frame (if any)
    timestamp_us_t timestamp{0};
    std::string description;                ///< Human-readable description
};

using sequence_event_callback_t = std::function<void(const c_sequence_event&)>;

// =============================================================================
// Active sequence tracking info
// =============================================================================

struct c_active_sequence {
    std::string rule_name;
    std::size_t current_step{0};
    std::size_t total_steps{0};
    timestamp_us_t started_at{0};
    timestamp_us_t last_step_at{0};
};

// =============================================================================
// Sequence detector — the state machine
// =============================================================================

class c_sequence_detector {
public:
    c_sequence_detector() = default;

    /// Register a rule to watch for.
    auto add_rule(c_sequence_rule rule) -> void;

    /// Remove a rule by name. Returns true if a rule was removed.
    auto remove_rule(const std::string& name) -> bool;

    /// List registered rule names.
    [[nodiscard]] auto rules() const -> std::vector<std::string>;

    /// Feed a frame to the detector. Checks all active rules and advances matching sequences.
    auto process_frame(const c_can_frame& frame) -> void;

    /// Explicitly check for timeouts at the given timestamp.
    auto check_timeouts(timestamp_us_t now) -> void;

    /// Set the callback for sequence events.
    auto set_event_callback(sequence_event_callback_t callback) -> void;

    /// Return currently in-progress sequences.
    [[nodiscard]] auto active_sequences() const -> std::vector<c_active_sequence>;

    /// Clear all in-progress tracking (rules remain registered).
    auto reset() -> void;

private:
    struct s_tracker {
        std::size_t rule_index{0};
        std::size_t current_step{0};
        timestamp_us_t started_at{0};
        timestamp_us_t last_step_at{0};
    };

    auto emit_event(const c_sequence_event& event) -> void;
    auto step_matches_frame(const c_sequence_step& step, const c_can_frame& frame) const -> bool;
    auto check_timeouts_locked(timestamp_us_t now) -> void;

    mutable std::mutex m_mutex;
    std::vector<c_sequence_rule> m_rules;
    std::vector<s_tracker> m_trackers;
    sequence_event_callback_t m_callback;
};

} // namespace interface::can

// =============================================================================
// Pre-built rule factory functions
// =============================================================================

namespace interface::can::rules {

/// UDS request/response pair detector.
auto uds_request_response(
    can_id_t tx_id, can_id_t rx_id,
    service_id_t sid,
    const std::string& name = ""
) -> c_sequence_rule;

/// UDS SecurityAccess handshake: SeedRequest -> SeedResponse -> KeySend -> KeyResponse.
auto uds_security_access(
    can_id_t tx_id, can_id_t rx_id,
    std::uint8_t level,
    const std::string& name = ""
) -> c_sequence_rule;

/// CANopen NMT boot-up sequence: NMT command -> boot-up message.
auto canopen_nmt_bootup(
    node_id_t node_id,
    const std::string& name = ""
) -> c_sequence_rule;

/// CANopen SDO expedited upload: request -> response.
auto canopen_sdo_upload(
    node_id_t node_id,
    std::uint16_t index,
    std::uint8_t sub_index,
    const std::string& name = ""
) -> c_sequence_rule;

/// Generic request/response pair by CAN IDs and first-byte matching.
auto request_response(
    can_id_t request_id, byte_t request_first_byte,
    can_id_t response_id, byte_t response_first_byte,
    timestamp_us_t timeout_us = 1'000'000,
    const std::string& name = ""
) -> c_sequence_rule;

} // namespace interface::can::rules
