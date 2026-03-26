#pragma once

/// @file client.hpp
/// @brief UDS client (ISO 14229) — send diagnostic requests and interpret responses.

#include "interface/can/frame.hpp"
#include "interface/can_hal/i_can_adapter.hpp"
#include "interface/core/error.hpp"
#include "interface/core/types.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace interface::uds {

// -------------------------------------------------------------------------
// UDS Service IDs (ISO 14229-1)
// -------------------------------------------------------------------------
namespace sid {
    inline constexpr service_id_t k_diagnostic_session_control = 0x10;
    inline constexpr service_id_t k_ecu_reset                  = 0x11;
    inline constexpr service_id_t k_security_access            = 0x27;
    inline constexpr service_id_t k_communication_control      = 0x28;
    inline constexpr service_id_t k_tester_present             = 0x3E;
    inline constexpr service_id_t k_read_data_by_identifier    = 0x22;
    inline constexpr service_id_t k_write_data_by_identifier   = 0x2E;
    inline constexpr service_id_t k_routine_control            = 0x31;
    inline constexpr service_id_t k_request_download           = 0x34;
    inline constexpr service_id_t k_request_upload             = 0x35;
    inline constexpr service_id_t k_transfer_data              = 0x36;
    inline constexpr service_id_t k_request_transfer_exit      = 0x37;
} // namespace sid

// -------------------------------------------------------------------------
// Diagnostic sessions
// -------------------------------------------------------------------------
enum class e_session : std::uint8_t {
    default_session          = 0x01,
    programming_session      = 0x02,
    extended_diagnostic      = 0x03,
    // OEM-specific: 0x40–0x5F
};

// -------------------------------------------------------------------------
// Negative Response Codes (NRC)
// -------------------------------------------------------------------------
enum class e_nrc : std::uint8_t {
    general_reject                 = 0x10,
    service_not_supported          = 0x11,
    sub_function_not_supported     = 0x12,
    incorrect_message_length       = 0x13,
    response_too_long              = 0x14,
    busy_repeat_request            = 0x21,
    conditions_not_correct         = 0x22,
    request_sequence_error         = 0x24,
    request_out_of_range           = 0x31,
    security_access_denied         = 0x33,
    invalid_key                    = 0x35,
    exceeded_number_of_attempts    = 0x36,
    required_time_delay_not_expired = 0x37,
    upload_download_not_accepted   = 0x70,
    transfer_data_suspended        = 0x71,
    general_programming_failure    = 0x72,
    service_not_supported_in_session = 0x7F,
};

/// Get human-readable name for an NRC.
[[nodiscard]] auto nrc_to_string(e_nrc nrc) -> std::string;

// -------------------------------------------------------------------------
// UDS Response
// -------------------------------------------------------------------------

/// A parsed UDS response (positive or negative).
struct c_uds_response {
    bool positive{false};
    service_id_t service_id{0};
    byte_buffer_t data;               ///< Payload (excluding SID byte)
    std::optional<e_nrc> nrc;         ///< Set if negative response

    /// Get the response data as a hex string for display.
    [[nodiscard]] auto data_hex() const -> std::string;
};

// -------------------------------------------------------------------------
// Security Access
// -------------------------------------------------------------------------

/// Callback to compute a key from a seed.
/// seed_level is the security access sub-function (odd number).
using seed_key_callback_t = std::function<
    result_t<byte_buffer_t>(std::uint8_t seed_level, byte_span_t seed)
>;

// -------------------------------------------------------------------------
// UDS Client Configuration
// -------------------------------------------------------------------------
struct c_uds_client_config {
    can_id_t tx_id{0x7DF};                         ///< Request CAN ID
    can_id_t rx_id{0x7E8};                         ///< Response CAN ID
    std::chrono::milliseconds p2_timeout{50};       ///< P2 server timing
    std::chrono::milliseconds p2_star_timeout{5000}; ///< P2* extended timing
    bool use_physical_addressing{true};             ///< Physical vs functional
};

// -------------------------------------------------------------------------
// UDS Client
// -------------------------------------------------------------------------

/// UDS diagnostic client. Sends requests over CAN and interprets responses.
class c_uds_client {
public:
    explicit c_uds_client(
        std::shared_ptr<can_hal::i_can_adapter> adapter,
        c_uds_client_config config = {}
    );

    ~c_uds_client();

    // -- Session management ------------------------------------------------

    /// Switch diagnostic session.
    [[nodiscard]] auto diagnostic_session_control(e_session session)
        -> result_t<c_uds_response>;

    /// Send TesterPresent (keep session alive).
    [[nodiscard]] auto tester_present(bool suppress_response = true)
        -> result_t<c_uds_response>;

    /// Get the currently active session.
    [[nodiscard]] auto current_session() const noexcept -> e_session;

    // -- Security access ---------------------------------------------------

    /// Perform SecurityAccess handshake for a given level.
    /// Requires a seed-key callback to compute the key.
    [[nodiscard]] auto security_access(
        std::uint8_t level,
        seed_key_callback_t key_callback
    ) -> result_t<c_uds_response>;

    // -- Data transfer -----------------------------------------------------

    /// ReadDataByIdentifier (0x22).
    [[nodiscard]] auto read_data_by_identifier(std::uint16_t did)
        -> result_t<c_uds_response>;

    /// WriteDataByIdentifier (0x2E).
    [[nodiscard]] auto write_data_by_identifier(
        std::uint16_t did,
        byte_span_t data
    ) -> result_t<c_uds_response>;

    // -- Routine control ---------------------------------------------------

    /// RoutineControl — start routine.
    [[nodiscard]] auto start_routine(
        std::uint16_t routine_id,
        byte_span_t option_record = {}
    ) -> result_t<c_uds_response>;

    /// RoutineControl — stop routine.
    [[nodiscard]] auto stop_routine(
        std::uint16_t routine_id,
        byte_span_t option_record = {}
    ) -> result_t<c_uds_response>;

    /// RoutineControl — request routine results.
    [[nodiscard]] auto request_routine_results(std::uint16_t routine_id)
        -> result_t<c_uds_response>;

    // -- ECU reset ---------------------------------------------------------

    /// ECUReset (0x11).
    [[nodiscard]] auto ecu_reset(std::uint8_t reset_type = 0x01)
        -> result_t<c_uds_response>;

    // -- Raw request -------------------------------------------------------

    /// Send a raw UDS request and wait for a response.
    [[nodiscard]] auto send_raw(byte_span_t request)
        -> result_t<c_uds_response>;

    // -- Configuration -----------------------------------------------------

    /// Update TX/RX IDs at runtime.
    auto set_addressing(can_id_t tx_id, can_id_t rx_id) -> void;

private:
    struct impl;
    std::unique_ptr<impl> m_impl;
};

} // namespace interface::uds
