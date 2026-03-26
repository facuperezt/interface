#pragma once

/// @file i_database_parser.hpp
/// @brief Abstract interface for CAN database file parsers (.dbc, .eds, .cdd).

#include "interface/core/error.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace interface::can_db {

/// Signal byte order.
enum class e_byte_order {
    little_endian,
    big_endian,
};

/// Signal value type.
enum class e_value_type {
    unsigned_int,
    signed_int,
    ieee_float,
    ieee_double,
};

/// A signal definition within a CAN message.
struct c_signal_def {
    std::string name;
    std::uint32_t start_bit{0};
    std::uint32_t length{0};
    e_byte_order byte_order{e_byte_order::little_endian};
    e_value_type value_type{e_value_type::unsigned_int};
    double factor{1.0};
    double offset{0.0};
    double min_value{0.0};
    double max_value{0.0};
    std::string unit;
    std::string comment;
};

/// A message definition in a CAN database.
struct c_message_def {
    std::uint32_t id{0};
    std::string name;
    std::uint8_t dlc{0};
    std::string sender;
    std::string comment;
    std::vector<c_signal_def> signals;
};

/// A parsed CAN database (from .dbc, .eds, or .cdd).
struct c_database {
    std::string name;
    std::string source_file;
    std::string version;
    std::vector<c_message_def> messages;

    /// Find a message by CAN ID.
    [[nodiscard]] auto find_message(std::uint32_t id) const
        -> std::optional<std::reference_wrapper<const c_message_def>>;
};

/// Abstract parser interface. Implement for each file format.
class i_database_parser {
public:
    virtual ~i_database_parser() = default;

    /// Parse a database file and return the result.
    [[nodiscard]] virtual auto parse(const std::filesystem::path& path)
        -> result_t<c_database> = 0;

    /// File extensions this parser supports (e.g., {".dbc"}).
    [[nodiscard]] virtual auto supported_extensions() const
        -> std::vector<std::string> = 0;

protected:
    i_database_parser() = default;
};

/// Signal decoder: decode raw CAN bytes using a signal definition.
class c_signal_decoder {
public:
    /// Decode a signal from raw data bytes.
    [[nodiscard]] static auto decode(
        const c_signal_def& signal,
        byte_span_t data
    ) -> double;

    /// Encode a physical value into raw data bytes.
    [[nodiscard]] static auto encode(
        const c_signal_def& signal,
        double value,
        mutable_byte_span_t data
    ) -> void_result_t;
};

} // namespace interface::can_db
