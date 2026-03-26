#pragma once

/// @file c_dbc_parser.hpp
/// @brief Custom DBC file parser (no dbcppp dependency).

#include "interface/can_db/i_database_parser.hpp"

#include <string>
#include <unordered_map>

namespace interface::can_db {

/// Parser for Vector DBC (Database CAN) files.
/// Parses key sections: VERSION, NS_, BS_, BU_, BO_ (messages),
/// SG_ (signals), CM_ (comments), BA_DEF_, BA_, VAL_.
class c_dbc_parser final : public i_database_parser {
public:
    c_dbc_parser() = default;

    [[nodiscard]] auto parse(const std::filesystem::path& path)
        -> result_t<c_database> override;

    [[nodiscard]] auto supported_extensions() const
        -> std::vector<std::string> override;

    /// Parse DBC content from a string (for testing).
    [[nodiscard]] auto parse_string(const std::string& content)
        -> result_t<c_database>;

private:
    auto parse_version(const std::string& line, c_database& db) -> void;
    auto parse_message(const std::string& line, c_database& db) -> bool;
    auto parse_signal(const std::string& line, c_message_def& msg) -> bool;
    auto parse_comment(const std::string& line, c_database& db) -> void;
    auto parse_value_descriptions(const std::string& line, c_database& db) -> void;
};

} // namespace interface::can_db
