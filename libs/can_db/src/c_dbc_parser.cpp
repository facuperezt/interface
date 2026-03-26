/// @file c_dbc_parser.cpp
/// @brief DBC file parser implementation.

#include "interface/can_db/c_dbc_parser.hpp"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace interface::can_db {

auto c_dbc_parser::parse(const std::filesystem::path& path) -> result_t<c_database> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return make_error("Failed to open DBC file: " + path.string(), e_error_category::io);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    auto result = parse_string(content);
    if (result) {
        result->source_file = path.string();
    }
    return result;
}

auto c_dbc_parser::supported_extensions() const -> std::vector<std::string> {
    return {".dbc"};
}

auto c_dbc_parser::parse_string(const std::string& content) -> result_t<c_database> {
    c_database db{};
    std::istringstream stream(content);
    std::string line;
    c_message_def* current_msg = nullptr;

    while (std::getline(stream, line)) {
        // Trim leading whitespace
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;
        }
        line = line.substr(start);

        if (line.starts_with("VERSION")) {
            parse_version(line, db);
        } else if (line.starts_with("BO_ ")) {
            if (parse_message(line, db)) {
                current_msg = &db.messages.back();
            } else {
                current_msg = nullptr;
            }
        } else if (line.starts_with("SG_ ") && current_msg != nullptr) {
            parse_signal(line, *current_msg);
        } else if (line.starts_with("CM_ ")) {
            // Comments can span multiple lines ending with ';'
            std::string full_comment = line;
            while (full_comment.find(';') == std::string::npos && std::getline(stream, line)) {
                full_comment += "\n" + line;
            }
            parse_comment(full_comment, db);
        } else if (line.starts_with("VAL_ ")) {
            std::string full_val = line;
            while (full_val.find(';') == std::string::npos && std::getline(stream, line)) {
                full_val += " " + line;
            }
            parse_value_descriptions(full_val, db);
        } else if (!line.starts_with("SG_ ")) {
            // If we encounter a non-signal line, the current message definition is done
            if (!line.starts_with(" ") && !line.starts_with("\t")) {
                current_msg = nullptr;
            }
        }
    }

    return db;
}

auto c_dbc_parser::parse_version(const std::string& line, c_database& db) -> void {
    // VERSION "1.0"
    auto quote1 = line.find('"');
    auto quote2 = line.rfind('"');
    if (quote1 != std::string::npos && quote2 != std::string::npos && quote2 > quote1) {
        db.version = line.substr(quote1 + 1, quote2 - quote1 - 1);
    }
}

auto c_dbc_parser::parse_message(const std::string& line, c_database& db) -> bool {
    // BO_ <id> <name>: <dlc> <sender>
    // Example: BO_ 256 EngineData: 8 ECU
    std::istringstream iss(line);
    std::string bo_token;
    iss >> bo_token; // "BO_"

    std::uint32_t id = 0;
    iss >> id;

    std::string name_with_colon;
    iss >> name_with_colon;
    if (name_with_colon.empty()) {
        return false;
    }
    // Remove trailing colon
    std::string name = name_with_colon;
    if (name.back() == ':') {
        name.pop_back();
    }

    std::uint32_t dlc_val = 0;
    iss >> dlc_val;

    std::string sender;
    iss >> sender;

    // In DBC, bit 31 of ID indicates extended frame
    id &= 0x1FFFFFFFU; // Mask to 29 bits

    c_message_def msg{};
    msg.id = id;
    msg.name = name;
    msg.dlc = static_cast<std::uint8_t>(dlc_val);
    msg.sender = sender;

    db.messages.push_back(std::move(msg));
    return true;
}

auto c_dbc_parser::parse_signal(const std::string& line, c_message_def& msg) -> bool {
    // SG_ <name> : <start_bit>|<length>@<byte_order><value_type> (<factor>,<offset>) [<min>|<max>] "<unit>" <receivers>
    // Example: SG_ EngineSpeed : 0|16@1+ (0.25,0) [0|16383.75] "rpm" Vector__XXX
    std::istringstream iss(line);
    std::string sg_token;
    iss >> sg_token; // "SG_"

    std::string name;
    iss >> name;

    // Check for multiplexer indicator (skip it)
    std::string next;
    iss >> next;
    if (next != ":") {
        // It might be a multiplexer indicator like "m0" or "M", read past it
        iss >> next; // This should be ':'
    }

    // Now parse: <start_bit>|<length>@<byte_order><value_type>
    std::string bit_spec;
    iss >> bit_spec;

    auto pipe_pos = bit_spec.find('|');
    auto at_pos = bit_spec.find('@');
    if (pipe_pos == std::string::npos || at_pos == std::string::npos) {
        return false;
    }

    auto start_bit_str = bit_spec.substr(0, pipe_pos);
    auto length_str = bit_spec.substr(pipe_pos + 1, at_pos - pipe_pos - 1);
    auto order_char = bit_spec[at_pos + 1];
    auto sign_char = (at_pos + 2 < bit_spec.size()) ? bit_spec[at_pos + 2] : '+';

    c_signal_def signal{};
    signal.name = name;

    {
        auto [p, e] = std::from_chars(start_bit_str.data(),
            start_bit_str.data() + start_bit_str.size(), signal.start_bit);
        if (e != std::errc{}) return false;
    }
    {
        auto [p, e] = std::from_chars(length_str.data(),
            length_str.data() + length_str.size(), signal.length);
        if (e != std::errc{}) return false;
    }

    signal.byte_order = (order_char == '0') ? e_byte_order::big_endian : e_byte_order::little_endian;
    signal.value_type = (sign_char == '-') ? e_value_type::signed_int : e_value_type::unsigned_int;

    // Parse (factor,offset)
    std::string factor_offset;
    iss >> factor_offset;
    if (factor_offset.size() >= 2 && factor_offset.front() == '(') {
        // Remove parentheses
        factor_offset = factor_offset.substr(1);
        if (factor_offset.back() == ')') {
            factor_offset.pop_back();
        }
        auto comma_pos = factor_offset.find(',');
        if (comma_pos != std::string::npos) {
            signal.factor = std::strtod(factor_offset.substr(0, comma_pos).c_str(), nullptr);
            signal.offset = std::strtod(factor_offset.substr(comma_pos + 1).c_str(), nullptr);
        }
    }

    // Parse [min|max]
    std::string min_max;
    iss >> min_max;
    if (min_max.size() >= 2 && min_max.front() == '[') {
        min_max = min_max.substr(1);
        if (min_max.back() == ']') {
            min_max.pop_back();
        }
        auto bar_pos = min_max.find('|');
        if (bar_pos != std::string::npos) {
            signal.min_value = std::strtod(min_max.substr(0, bar_pos).c_str(), nullptr);
            signal.max_value = std::strtod(min_max.substr(bar_pos + 1).c_str(), nullptr);
        }
    }

    // Parse "unit"
    std::string unit;
    iss >> unit;
    if (unit.size() >= 2 && unit.front() == '"') {
        unit = unit.substr(1);
        if (unit.back() == '"') {
            unit.pop_back();
        }
        signal.unit = unit;
    }

    msg.signals.push_back(std::move(signal));
    return true;
}

auto c_dbc_parser::parse_comment(const std::string& line, c_database& db) -> void {
    // CM_ BO_ <id> "comment";
    // CM_ SG_ <id> <signal_name> "comment";
    if (line.find("CM_ BO_ ") == 0) {
        std::istringstream iss(line.substr(8));
        std::uint32_t id = 0;
        iss >> id;
        id &= 0x1FFFFFFFU;

        // Extract comment between quotes
        auto q1 = line.find('"');
        auto q2 = line.rfind('"');
        if (q1 != std::string::npos && q2 > q1) {
            auto comment = line.substr(q1 + 1, q2 - q1 - 1);
            for (auto& msg : db.messages) {
                if (msg.id == id) {
                    msg.comment = comment;
                    break;
                }
            }
        }
    } else if (line.find("CM_ SG_ ") == 0) {
        std::istringstream iss(line.substr(8));
        std::uint32_t id = 0;
        iss >> id;
        id &= 0x1FFFFFFFU;

        std::string sig_name;
        iss >> sig_name;

        auto q1 = line.find('"');
        auto q2 = line.rfind('"');
        if (q1 != std::string::npos && q2 > q1) {
            auto comment = line.substr(q1 + 1, q2 - q1 - 1);
            for (auto& msg : db.messages) {
                if (msg.id == id) {
                    for (auto& sig : msg.signals) {
                        if (sig.name == sig_name) {
                            sig.comment = comment;
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
}

auto c_dbc_parser::parse_value_descriptions(
    [[maybe_unused]] const std::string& line,
    [[maybe_unused]] c_database& db
) -> void {
    // VAL_ <id> <signal_name> <value> "<description>" ... ;
    // Value descriptions are stored as part of signal metadata.
    // For now we parse the basic structure but don't store the value table
    // (would need a map in c_signal_def).
}

} // namespace interface::can_db
