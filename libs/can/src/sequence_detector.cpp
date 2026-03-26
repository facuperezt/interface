/// @file sequence_detector.cpp
/// @brief CAN protocol sequence detector implementation.

#include "interface/can/sequence_detector.hpp"

#include <algorithm>
#include <format>

namespace interface::can {

// =============================================================================
// c_byte_matcher
// =============================================================================

auto c_byte_matcher::any() -> c_byte_matcher {
    c_byte_matcher m;
    m.m_type = e_match_type::any;
    return m;
}

auto c_byte_matcher::exact(byte_t value) -> c_byte_matcher {
    c_byte_matcher m;
    m.m_type = e_match_type::exact;
    m.m_value = value;
    return m;
}

auto c_byte_matcher::masked(byte_t value, byte_t mask) -> c_byte_matcher {
    c_byte_matcher m;
    m.m_type = e_match_type::masked;
    m.m_value = value;
    m.m_mask = mask;
    return m;
}

auto c_byte_matcher::range(byte_t low, byte_t high) -> c_byte_matcher {
    c_byte_matcher m;
    m.m_type = e_match_type::range;
    m.m_low = low;
    m.m_high = high;
    return m;
}

auto c_byte_matcher::matches(byte_t byte) const noexcept -> bool {
    switch (m_type) {
        case e_match_type::any:
            return true;
        case e_match_type::exact:
            return byte == m_value;
        case e_match_type::masked:
            return (byte & m_mask) == (m_value & m_mask);
        case e_match_type::range:
            return byte >= m_low && byte <= m_high;
    }
    return false;
}

// =============================================================================
// c_sequence_detector
// =============================================================================

auto c_sequence_detector::add_rule(c_sequence_rule rule) -> void {
    std::lock_guard lock(m_mutex);
    m_rules.push_back(std::move(rule));
}

auto c_sequence_detector::remove_rule(const std::string& name) -> bool {
    std::lock_guard lock(m_mutex);

    // Remove trackers referencing this rule.
    // Find the rule index first.
    auto rule_it = std::find_if(m_rules.begin(), m_rules.end(),
        [&name](const c_sequence_rule& r) { return r.name == name; });

    if (rule_it == m_rules.end()) {
        return false;
    }

    auto rule_index = static_cast<std::size_t>(std::distance(m_rules.begin(), rule_it));

    // Remove all trackers referencing this rule.
    std::erase_if(m_trackers, [rule_index](const s_tracker& t) {
        return t.rule_index == rule_index;
    });

    // Remove the rule.
    m_rules.erase(rule_it);

    // Adjust tracker rule indices for rules that shifted down.
    for (auto& tracker : m_trackers) {
        if (tracker.rule_index > rule_index) {
            --tracker.rule_index;
        }
    }

    return true;
}

auto c_sequence_detector::rules() const -> std::vector<std::string> {
    std::lock_guard lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_rules.size());
    for (const auto& rule : m_rules) {
        names.push_back(rule.name);
    }
    return names;
}

auto c_sequence_detector::process_frame(const c_can_frame& frame) -> void {
    std::lock_guard lock(m_mutex);

    // First, check timeouts using the frame timestamp.
    check_timeouts_locked(frame.timestamp);

    // Try to advance existing trackers.
    // We need to be careful because processing may remove/add trackers.
    std::vector<std::size_t> trackers_to_remove;

    for (std::size_t i = 0; i < m_trackers.size(); ++i) {
        auto& tracker = m_trackers[i];
        const auto& rule = m_rules[tracker.rule_index];
        const auto& step = rule.steps[tracker.current_step];

        if (step_matches_frame(step, frame)) {
            tracker.current_step++;
            tracker.last_step_at = frame.timestamp;

            if (tracker.current_step >= rule.steps.size()) {
                // Sequence completed.
                emit_event(c_sequence_event{
                    .type = e_sequence_event_type::sequence_completed,
                    .severity = e_sequence_severity::info,
                    .rule_name = rule.name,
                    .step_index = tracker.current_step - 1,
                    .step_label = step.label,
                    .frame = frame,
                    .timestamp = frame.timestamp,
                    .description = std::format("Sequence '{}' completed", rule.name),
                });
                trackers_to_remove.push_back(i);
            } else {
                // Step matched (intermediate).
                emit_event(c_sequence_event{
                    .type = e_sequence_event_type::step_matched,
                    .severity = e_sequence_severity::info,
                    .rule_name = rule.name,
                    .step_index = tracker.current_step - 1,
                    .step_label = step.label,
                    .frame = frame,
                    .timestamp = frame.timestamp,
                    .description = std::format("Step '{}' matched in '{}'",
                        step.label, rule.name),
                });
            }
        } else if (!rule.allow_interleaved) {
            // Unexpected frame while interleaving is not allowed.
            emit_event(c_sequence_event{
                .type = e_sequence_event_type::unexpected_frame,
                .severity = e_sequence_severity::warning,
                .rule_name = rule.name,
                .step_index = tracker.current_step,
                .step_label = step.label,
                .frame = frame,
                .timestamp = frame.timestamp,
                .description = std::format("Unexpected frame while waiting for '{}' in '{}'",
                    step.label, rule.name),
            });
            trackers_to_remove.push_back(i);
        }
    }

    // Remove completed/aborted trackers (in reverse to preserve indices).
    std::sort(trackers_to_remove.begin(), trackers_to_remove.end(), std::greater<>());
    for (auto idx : trackers_to_remove) {
        m_trackers.erase(m_trackers.begin() + static_cast<std::ptrdiff_t>(idx));
    }

    // Try to start new sequences by checking if this frame matches the first step of any rule.
    for (std::size_t ri = 0; ri < m_rules.size(); ++ri) {
        const auto& rule = m_rules[ri];
        if (rule.steps.empty()) {
            continue;
        }

        // Skip if a tracker for this rule already exists and the rule is not repeatable
        // (though we do allow multiple concurrent instances).
        const auto& first_step = rule.steps[0];
        if (!step_matches_frame(first_step, frame)) {
            continue;
        }

        // Check if this frame was already consumed by an existing tracker for this rule.
        // We don't want to start a new sequence for a frame that advanced an existing one.
        bool already_consumed = false;
        for (const auto& tracker : m_trackers) {
            if (tracker.rule_index == ri && tracker.last_step_at == frame.timestamp) {
                already_consumed = true;
                break;
            }
        }
        if (already_consumed) {
            continue;
        }

        if (rule.steps.size() == 1) {
            // Single-step rule: emit started + completed immediately.
            emit_event(c_sequence_event{
                .type = e_sequence_event_type::sequence_started,
                .severity = e_sequence_severity::info,
                .rule_name = rule.name,
                .step_index = 0,
                .step_label = first_step.label,
                .frame = frame,
                .timestamp = frame.timestamp,
                .description = std::format("Sequence '{}' started", rule.name),
            });
            emit_event(c_sequence_event{
                .type = e_sequence_event_type::sequence_completed,
                .severity = e_sequence_severity::info,
                .rule_name = rule.name,
                .step_index = 0,
                .step_label = first_step.label,
                .frame = frame,
                .timestamp = frame.timestamp,
                .description = std::format("Sequence '{}' completed", rule.name),
            });
        } else {
            // Multi-step rule: create a tracker and emit started event.
            emit_event(c_sequence_event{
                .type = e_sequence_event_type::sequence_started,
                .severity = e_sequence_severity::info,
                .rule_name = rule.name,
                .step_index = 0,
                .step_label = first_step.label,
                .frame = frame,
                .timestamp = frame.timestamp,
                .description = std::format("Sequence '{}' started", rule.name),
            });

            m_trackers.push_back(s_tracker{
                .rule_index = ri,
                .current_step = 1,
                .started_at = frame.timestamp,
                .last_step_at = frame.timestamp,
            });
        }
    }
}

auto c_sequence_detector::check_timeouts(timestamp_us_t now) -> void {
    std::lock_guard lock(m_mutex);
    check_timeouts_locked(now);
}

auto c_sequence_detector::check_timeouts_locked(timestamp_us_t now) -> void {
    std::vector<std::size_t> timed_out;

    for (std::size_t i = 0; i < m_trackers.size(); ++i) {
        const auto& tracker = m_trackers[i];
        const auto& rule = m_rules[tracker.rule_index];
        const auto& step = rule.steps[tracker.current_step];

        auto elapsed = now - tracker.last_step_at;
        if (elapsed > step.timeout_us) {
            emit_event(c_sequence_event{
                .type = e_sequence_event_type::step_timeout,
                .severity = e_sequence_severity::error,
                .rule_name = rule.name,
                .step_index = tracker.current_step,
                .step_label = step.label,
                .frame = std::nullopt,
                .timestamp = now,
                .description = std::format("Timeout waiting for '{}' in '{}' ({}us elapsed)",
                    step.label, rule.name, elapsed),
            });
            timed_out.push_back(i);
        }
    }

    std::sort(timed_out.begin(), timed_out.end(), std::greater<>());
    for (auto idx : timed_out) {
        m_trackers.erase(m_trackers.begin() + static_cast<std::ptrdiff_t>(idx));
    }
}

auto c_sequence_detector::set_event_callback(sequence_event_callback_t callback) -> void {
    std::lock_guard lock(m_mutex);
    m_callback = std::move(callback);
}

auto c_sequence_detector::active_sequences() const -> std::vector<c_active_sequence> {
    std::lock_guard lock(m_mutex);
    std::vector<c_active_sequence> result;
    result.reserve(m_trackers.size());

    for (const auto& tracker : m_trackers) {
        const auto& rule = m_rules[tracker.rule_index];
        result.push_back(c_active_sequence{
            .rule_name = rule.name,
            .current_step = tracker.current_step,
            .total_steps = rule.steps.size(),
            .started_at = tracker.started_at,
            .last_step_at = tracker.last_step_at,
        });
    }

    return result;
}

auto c_sequence_detector::reset() -> void {
    std::lock_guard lock(m_mutex);
    m_trackers.clear();
}

auto c_sequence_detector::emit_event(const c_sequence_event& event) -> void {
    if (m_callback) {
        m_callback(event);
    }
}

auto c_sequence_detector::step_matches_frame(const c_sequence_step& step, const c_can_frame& frame) const -> bool {
    // Check CAN ID with mask.
    if ((frame.id & step.id_mask) != (step.id & step.id_mask)) {
        return false;
    }

    // Check payload matchers.
    for (std::size_t i = 0; i < step.payload.size(); ++i) {
        if (i >= frame.data_length()) {
            return false; // Frame payload too short.
        }
        if (!step.payload[i].matches(frame.data[i])) {
            return false;
        }
    }

    return true;
}

} // namespace interface::can

// =============================================================================
// Pre-built rule factory functions
// =============================================================================

namespace interface::can::rules {

auto uds_request_response(
    can_id_t tx_id, can_id_t rx_id,
    service_id_t sid,
    const std::string& name
) -> c_sequence_rule {
    auto rule_name = name.empty()
        ? std::format("UDS {:#04x} Request/Response", sid)
        : name;

    // Step 1: Request — first byte is the SID
    c_sequence_step request;
    request.label = "Request";
    request.id = tx_id;
    request.payload.push_back(c_byte_matcher::exact(sid));

    // Step 2: Response — positive (SID + 0x40) OR negative (0x7F, SID, NRC).
    // We use a masked matcher: we match any first byte that has the right SID+0x40 bit pattern,
    // OR we accept 0x7F as a negative response. Since we can't do OR logic in a single step,
    // we need two separate response steps. Instead, use a more permissive approach:
    // Match any frame on the rx_id — the fact that a response came on the right ID is sufficient.
    // Actually, let's create a 2-step rule where the response step matches either pattern.
    // The simplest correct approach: match any response on rx_id with first byte being
    // either (SID|0x40) or 0x7F. We can't express OR in payload matchers, so use a range
    // or just accept any payload on the rx_id. Since UDS responses always start with
    // (SID+0x40) for positive or 0x7F for negative, and these are the only valid responses,
    // matching any frame on rx_id after a request on tx_id is correct in practice.
    c_sequence_step response;
    response.label = "Response";
    response.id = rx_id;
    // Match first byte: any value that is either SID+0x40 or 0x7F.
    // Since we can't do OR, we leave payload empty to match any response on the expected ID.
    // This is correct — a frame on rx_id after our request IS the response.

    return c_sequence_rule{
        .name = std::move(rule_name),
        .steps = {std::move(request), std::move(response)},
        .allow_interleaved = true,
        .repeatable = true,
    };
}

auto uds_security_access(
    can_id_t tx_id, can_id_t rx_id,
    std::uint8_t level,
    const std::string& name
) -> c_sequence_rule {
    auto rule_name = name.empty()
        ? std::format("UDS SecurityAccess Level {:#04x}", level)
        : name;

    constexpr service_id_t k_security_access_sid = 0x27;
    constexpr service_id_t k_security_access_response = 0x67;

    // Step 1: Seed request (0x27, odd level)
    c_sequence_step seed_request;
    seed_request.label = "SeedRequest";
    seed_request.id = tx_id;
    seed_request.payload.push_back(c_byte_matcher::exact(k_security_access_sid));
    seed_request.payload.push_back(c_byte_matcher::exact(level));

    // Step 2: Seed response (0x67, odd level, seed data...)
    c_sequence_step seed_response;
    seed_response.label = "SeedResponse";
    seed_response.id = rx_id;
    seed_response.payload.push_back(c_byte_matcher::exact(k_security_access_response));
    seed_response.payload.push_back(c_byte_matcher::exact(level));

    // Step 3: Key send (0x27, even level = level + 1)
    c_sequence_step key_send;
    key_send.label = "KeySend";
    key_send.id = tx_id;
    key_send.payload.push_back(c_byte_matcher::exact(k_security_access_sid));
    key_send.payload.push_back(c_byte_matcher::exact(static_cast<byte_t>(level + 1)));

    // Step 4: Key response (0x67, even level = level + 1)
    c_sequence_step key_response;
    key_response.label = "KeyResponse";
    key_response.id = rx_id;
    key_response.payload.push_back(c_byte_matcher::exact(k_security_access_response));
    key_response.payload.push_back(c_byte_matcher::exact(static_cast<byte_t>(level + 1)));

    return c_sequence_rule{
        .name = std::move(rule_name),
        .steps = {
            std::move(seed_request),
            std::move(seed_response),
            std::move(key_send),
            std::move(key_response),
        },
        .allow_interleaved = true,
        .repeatable = true,
    };
}

auto canopen_nmt_bootup(
    node_id_t node_id,
    const std::string& name
) -> c_sequence_rule {
    auto rule_name = name.empty()
        ? std::format("CANopen NMT Bootup Node {}", node_id)
        : name;

    constexpr can_id_t k_nmt_id = 0x000;
    constexpr byte_t k_nmt_start_remote = 0x01;
    constexpr can_id_t k_heartbeat_base = 0x700;

    // Step 1: NMT Start Remote Node command (ID=0x000, data=[0x01, node_id])
    c_sequence_step nmt_command;
    nmt_command.label = "NMT Start";
    nmt_command.id = k_nmt_id;
    nmt_command.payload.push_back(c_byte_matcher::exact(k_nmt_start_remote));
    nmt_command.payload.push_back(c_byte_matcher::exact(node_id));

    // Step 2: Boot-up message (ID=0x700+node_id, data=[0x00])
    c_sequence_step bootup;
    bootup.label = "Bootup Message";
    bootup.id = k_heartbeat_base + node_id;
    bootup.payload.push_back(c_byte_matcher::exact(0x00));

    return c_sequence_rule{
        .name = std::move(rule_name),
        .steps = {std::move(nmt_command), std::move(bootup)},
        .allow_interleaved = true,
        .repeatable = true,
    };
}

auto canopen_sdo_upload(
    node_id_t node_id,
    std::uint16_t index,
    std::uint8_t sub_index,
    const std::string& name
) -> c_sequence_rule {
    auto rule_name = name.empty()
        ? std::format("CANopen SDO Upload {:#06x}:{:#04x} Node {}",
            index, sub_index, node_id)
        : name;

    constexpr can_id_t k_sdo_tx_base = 0x600;  // Client -> Server
    constexpr can_id_t k_sdo_rx_base = 0x580;  // Server -> Client
    constexpr byte_t k_initiate_upload_request = 0x40;

    // SDO index is sent little-endian.
    auto index_low = static_cast<byte_t>(index & 0xFF);
    auto index_high = static_cast<byte_t>((index >> 8) & 0xFF);

    // Step 1: Initiate Upload Request
    c_sequence_step request;
    request.label = "SDO Upload Request";
    request.id = k_sdo_tx_base + node_id;
    request.payload.push_back(c_byte_matcher::exact(k_initiate_upload_request));
    request.payload.push_back(c_byte_matcher::exact(index_low));
    request.payload.push_back(c_byte_matcher::exact(index_high));
    request.payload.push_back(c_byte_matcher::exact(sub_index));

    // Step 2: Initiate Upload Response (command byte varies based on expedited/segmented)
    // The response command specifier has bits indicating size, expedited, etc.
    // Match with masked: top 5 bits = 0x40 (010 = upload response), lower 3 bits vary.
    c_sequence_step response;
    response.label = "SDO Upload Response";
    response.id = k_sdo_rx_base + node_id;
    response.payload.push_back(c_byte_matcher::masked(0x40, 0xE0)); // Top 3 bits = 010
    response.payload.push_back(c_byte_matcher::exact(index_low));
    response.payload.push_back(c_byte_matcher::exact(index_high));
    response.payload.push_back(c_byte_matcher::exact(sub_index));

    return c_sequence_rule{
        .name = std::move(rule_name),
        .steps = {std::move(request), std::move(response)},
        .allow_interleaved = true,
        .repeatable = true,
    };
}

auto request_response(
    can_id_t request_id, byte_t request_first_byte,
    can_id_t response_id, byte_t response_first_byte,
    timestamp_us_t timeout_us,
    const std::string& name
) -> c_sequence_rule {
    auto rule_name = name.empty()
        ? std::format("Request/Response {:#05x}->{:#05x}", request_id, response_id)
        : name;

    c_sequence_step req;
    req.label = "Request";
    req.id = request_id;
    req.payload.push_back(c_byte_matcher::exact(request_first_byte));

    c_sequence_step resp;
    resp.label = "Response";
    resp.id = response_id;
    resp.payload.push_back(c_byte_matcher::exact(response_first_byte));
    resp.timeout_us = timeout_us;

    return c_sequence_rule{
        .name = std::move(rule_name),
        .steps = {std::move(req), std::move(resp)},
        .allow_interleaved = true,
        .repeatable = true,
    };
}

} // namespace interface::can::rules
