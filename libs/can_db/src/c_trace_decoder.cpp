/// @file c_trace_decoder.cpp
/// @brief c_trace_decoder implementation.

#include "interface/can_db/c_trace_decoder.hpp"

namespace interface::can_db {

c_trace_decoder::c_trace_decoder(c_database db)
    : m_database(std::move(db)) {}

auto c_trace_decoder::decode_frame(const can::c_can_frame& frame) const
    -> c_decoded_frame
{
    auto msg_opt = m_database.find_message(frame.id);
    if (!msg_opt.has_value()) {
        return c_decoded_frame{
            .raw          = frame,
            .message_name = {},
            .signals      = {},
            .known        = false,
        };
    }

    const auto& msg = msg_opt->get();
    std::vector<c_decoded_signal> signals;
    signals.reserve(msg.signals.size());

    for (const auto& sig_def : msg.signals) {
        auto physical = c_signal_decoder::decode(sig_def, frame.payload());
        auto raw_value = (sig_def.factor != 0.0)
            ? (physical - sig_def.offset) / sig_def.factor
            : 0.0;

        signals.push_back(c_decoded_signal{
            .name      = sig_def.name,
            .raw_value = raw_value,
            .value     = physical,
            .unit      = sig_def.unit,
        });
    }

    return c_decoded_frame{
        .raw          = frame,
        .message_name = msg.name,
        .signals      = std::move(signals),
        .known        = true,
    };
}

auto c_trace_decoder::decode_trace(can_trace::i_trace_reader& reader)
    -> result_t<std::vector<c_decoded_frame>>
{
    auto frames_result = reader.read_all();
    if (!frames_result.has_value()) {
        return std::unexpected(frames_result.error());
    }

    std::vector<c_decoded_frame> decoded;
    decoded.reserve(frames_result->size());

    for (const auto& frame : *frames_result) {
        decoded.push_back(decode_frame(frame));
    }

    return decoded;
}

auto c_trace_decoder::database() const -> const c_database& {
    return m_database;
}

} // namespace interface::can_db
