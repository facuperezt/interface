#pragma once

/// @file c_trace_decoder.hpp
/// @brief Decode raw CAN frames into structured output using a DBC database.

#include "interface/can_db/i_database_parser.hpp"
#include "interface/can_trace/i_trace_reader.hpp"

#include <string>
#include <vector>

namespace interface::can_db {

/// A single decoded signal value.
struct c_decoded_signal {
    std::string name;
    double      raw_value{0.0};   ///< before factor/offset
    double      value{0.0};       ///< physical value (raw * factor + offset)
    std::string unit;
};

/// A decoded CAN frame = raw frame + message name + decoded signals.
struct c_decoded_frame {
    can::c_can_frame                raw;
    std::string                     message_name;   ///< from DBC, empty if unknown
    std::vector<c_decoded_signal>   signals;         ///< empty if unknown message
    bool                            known{false};    ///< true if message was found in DB
};

/// Decodes raw CAN frames into structured output with message names and
/// physical signal values using a parsed CAN database.
class c_trace_decoder {
public:
    /// Construct with a parsed database.
    explicit c_trace_decoder(c_database db);

    /// Decode a single raw frame using the loaded database.
    [[nodiscard]] auto decode_frame(const can::c_can_frame& frame) const
        -> c_decoded_frame;

    /// Decode all frames from a trace reader.
    [[nodiscard]] auto decode_trace(can_trace::i_trace_reader& reader)
        -> result_t<std::vector<c_decoded_frame>>;

    /// Access the loaded database.
    [[nodiscard]] auto database() const -> const c_database&;

private:
    c_database m_database;
};

} // namespace interface::can_db
