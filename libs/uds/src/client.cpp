/// @file client.cpp
/// @brief UDS client implementation.

#include "interface/uds/client.hpp"
#include "interface/core/log.hpp"

#include <format>

namespace interface::uds {

// -------------------------------------------------------------------------
// NRC to string
// -------------------------------------------------------------------------
auto nrc_to_string(e_nrc nrc) -> std::string {
    switch (nrc) {
        case e_nrc::general_reject:                   return "generalReject";
        case e_nrc::service_not_supported:            return "serviceNotSupported";
        case e_nrc::sub_function_not_supported:       return "subFunctionNotSupported";
        case e_nrc::incorrect_message_length:         return "incorrectMessageLengthOrInvalidFormat";
        case e_nrc::response_too_long:                return "responseTooLong";
        case e_nrc::busy_repeat_request:              return "busyRepeatRequest";
        case e_nrc::conditions_not_correct:           return "conditionsNotCorrect";
        case e_nrc::request_sequence_error:           return "requestSequenceError";
        case e_nrc::request_out_of_range:             return "requestOutOfRange";
        case e_nrc::security_access_denied:           return "securityAccessDenied";
        case e_nrc::invalid_key:                      return "invalidKey";
        case e_nrc::exceeded_number_of_attempts:      return "exceededNumberOfAttempts";
        case e_nrc::required_time_delay_not_expired:  return "requiredTimeDelayNotExpired";
        case e_nrc::upload_download_not_accepted:     return "uploadDownloadNotAccepted";
        case e_nrc::transfer_data_suspended:          return "transferDataSuspended";
        case e_nrc::general_programming_failure:      return "generalProgrammingFailure";
        case e_nrc::service_not_supported_in_session: return "serviceNotSupportedInActiveSession";
    }
    return std::format("unknownNRC(0x{:02X})", static_cast<std::uint8_t>(nrc));
}

auto c_uds_response::data_hex() const -> std::string {
    std::string hex;
    for (auto byte : data) {
        if (!hex.empty()) hex += ' ';
        hex += std::format("{:02X}", byte);
    }
    return hex;
}

// -------------------------------------------------------------------------
// Implementation detail
// -------------------------------------------------------------------------
struct c_uds_client::impl {
    std::shared_ptr<can_hal::i_can_adapter> adapter;
    c_uds_client_config config;
    e_session current_session{e_session::default_session};

    impl(std::shared_ptr<can_hal::i_can_adapter> a, c_uds_client_config c)
        : adapter{std::move(a)}, config{std::move(c)} {}

    /// Build a CAN frame from a UDS payload (single-frame ISO-TP for now).
    auto build_request_frame(byte_span_t payload) const -> can::c_can_frame {
        can::c_can_frame frame{};
        frame.id = config.tx_id;
        // Single Frame ISO-TP: first byte = PCI (0x0N where N = length)
        auto len = static_cast<byte_t>(payload.size());
        frame.dlc = 8;
        frame.data[0] = len; // SF PCI
        for (std::size_t i = 0; i < payload.size() && i < 7; ++i) {
            frame.data[i + 1] = payload[i];
        }
        return frame;
    }

    /// Send a request and wait for a response.
    auto transact(byte_span_t request) -> result_t<c_uds_response> {
        if (!adapter || !adapter->is_open()) {
            return make_error("CAN adapter not open", e_error_category::hardware);
        }

        auto tx_frame = build_request_frame(request);
        auto send_result = adapter->send(tx_frame);
        if (!send_result) {
            return std::unexpected(send_result.error());
        }

        // Wait for response
        auto rx_result = adapter->receive(config.p2_timeout);
        if (!rx_result) {
            return std::unexpected(rx_result.error());
        }

        if (!rx_result->has_value()) {
            return make_error("Response timeout", e_error_category::timeout);
        }

        auto& rx_frame = rx_result->value();
        return parse_response(rx_frame);
    }

    /// Parse an ISO-TP single-frame response.
    auto parse_response(const can::c_can_frame& frame) -> result_t<c_uds_response> {
        // Single Frame: data[0] = PCI (0x0N), data[1..] = payload
        auto pci_len = frame.data[0] & 0x0F;
        if (pci_len == 0 || pci_len > 7) {
            return make_error("Invalid ISO-TP single frame length", e_error_category::protocol);
        }

        c_uds_response response{};
        auto sid_byte = frame.data[1];

        if (sid_byte == 0x7F) {
            // Negative response: 0x7F, rejected_SID, NRC
            response.positive = false;
            response.service_id = frame.data[2];
            response.nrc = static_cast<e_nrc>(frame.data[3]);
            response.data.assign(frame.data.begin() + 1, frame.data.begin() + 1 + pci_len);
        } else {
            // Positive response: SID + 0x40, ...
            response.positive = true;
            response.service_id = static_cast<service_id_t>(sid_byte - 0x40);
            response.data.assign(frame.data.begin() + 2, frame.data.begin() + 1 + pci_len);
        }

        return response;
    }
};

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------
c_uds_client::c_uds_client(
    std::shared_ptr<can_hal::i_can_adapter> adapter,
    c_uds_client_config config
) : m_impl{std::make_unique<impl>(std::move(adapter), std::move(config))} {}

c_uds_client::~c_uds_client() = default;

auto c_uds_client::diagnostic_session_control(e_session session)
    -> result_t<c_uds_response>
{
    std::array<byte_t, 2> req{sid::k_diagnostic_session_control, static_cast<byte_t>(session)};
    auto result = m_impl->transact(byte_span_t{req});
    if (result && result->positive) {
        m_impl->current_session = session;
    }
    return result;
}

auto c_uds_client::tester_present(bool suppress_response) -> result_t<c_uds_response> {
    std::array<byte_t, 2> req{sid::k_tester_present, static_cast<byte_t>(suppress_response ? 0x80 : 0x00)};
    return m_impl->transact(byte_span_t{req});
}

auto c_uds_client::current_session() const noexcept -> e_session {
    return m_impl->current_session;
}

auto c_uds_client::security_access(
    std::uint8_t level,
    seed_key_callback_t key_callback
) -> result_t<c_uds_response> {
    // Step 1: Request seed
    std::array<byte_t, 2> seed_req{sid::k_security_access, level};
    auto seed_result = m_impl->transact(byte_span_t{seed_req});
    if (!seed_result || !seed_result->positive) {
        return seed_result;
    }

    // Step 2: Compute key from seed
    auto seed_data = byte_span_t{seed_result->data}.subspan(1); // skip sub-function
    auto key_result = key_callback(level, seed_data);
    if (!key_result) {
        return std::unexpected(key_result.error());
    }

    // Step 3: Send key
    byte_buffer_t key_req;
    key_req.push_back(sid::k_security_access);
    key_req.push_back(static_cast<byte_t>(level + 1)); // key sub-function = seed + 1
    key_req.insert(key_req.end(), key_result->begin(), key_result->end());
    return m_impl->transact(byte_span_t{key_req});
}

auto c_uds_client::read_data_by_identifier(std::uint16_t did)
    -> result_t<c_uds_response>
{
    std::array<byte_t, 3> req{
        sid::k_read_data_by_identifier,
        static_cast<byte_t>((did >> 8) & 0xFF),
        static_cast<byte_t>(did & 0xFF),
    };
    return m_impl->transact(byte_span_t{req});
}

auto c_uds_client::write_data_by_identifier(
    std::uint16_t did,
    byte_span_t data
) -> result_t<c_uds_response> {
    byte_buffer_t req;
    req.push_back(sid::k_write_data_by_identifier);
    req.push_back(static_cast<byte_t>((did >> 8) & 0xFF));
    req.push_back(static_cast<byte_t>(did & 0xFF));
    req.insert(req.end(), data.begin(), data.end());
    return m_impl->transact(byte_span_t{req});
}

auto c_uds_client::start_routine(
    std::uint16_t routine_id,
    byte_span_t option_record
) -> result_t<c_uds_response> {
    byte_buffer_t req;
    req.push_back(sid::k_routine_control);
    req.push_back(0x01); // startRoutine
    req.push_back(static_cast<byte_t>((routine_id >> 8) & 0xFF));
    req.push_back(static_cast<byte_t>(routine_id & 0xFF));
    req.insert(req.end(), option_record.begin(), option_record.end());
    return m_impl->transact(byte_span_t{req});
}

auto c_uds_client::stop_routine(
    std::uint16_t routine_id,
    byte_span_t option_record
) -> result_t<c_uds_response> {
    byte_buffer_t req;
    req.push_back(sid::k_routine_control);
    req.push_back(0x02); // stopRoutine
    req.push_back(static_cast<byte_t>((routine_id >> 8) & 0xFF));
    req.push_back(static_cast<byte_t>(routine_id & 0xFF));
    req.insert(req.end(), option_record.begin(), option_record.end());
    return m_impl->transact(byte_span_t{req});
}

auto c_uds_client::request_routine_results(std::uint16_t routine_id)
    -> result_t<c_uds_response>
{
    std::array<byte_t, 4> req{
        sid::k_routine_control,
        0x03, // requestRoutineResults
        static_cast<byte_t>((routine_id >> 8) & 0xFF),
        static_cast<byte_t>(routine_id & 0xFF),
    };
    return m_impl->transact(byte_span_t{req});
}

auto c_uds_client::ecu_reset(std::uint8_t reset_type) -> result_t<c_uds_response> {
    std::array<byte_t, 2> req{sid::k_ecu_reset, reset_type};
    return m_impl->transact(byte_span_t{req});
}

auto c_uds_client::send_raw(byte_span_t request) -> result_t<c_uds_response> {
    return m_impl->transact(request);
}

auto c_uds_client::set_addressing(can_id_t tx_id, can_id_t rx_id) -> void {
    m_impl->config.tx_id = tx_id;
    m_impl->config.rx_id = rx_id;
}

} // namespace interface::uds
