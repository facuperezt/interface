/// @file c_asc_reader.cpp
/// @brief ASC trace file parser implementation.

#include "interface/can_trace/c_asc_reader.hpp"

#include <charconv>
#include <cstdlib>
#include <sstream>

namespace interface::can_trace {

auto c_asc_reader::open(const std::filesystem::path& path) -> void_result_t {
    if (m_opened) {
        m_file.close();
    }

    m_path = path;
    m_file.open(path, std::ios::in);
    if (!m_file.is_open()) {
        return make_error("Failed to open ASC file: " + path.string(), e_error_category::io);
    }

    m_info = c_trace_info{
        .format = "ASC",
        .path = path,
    };
    m_opened = true;
    return {};
}

auto c_asc_reader::read_next() -> result_t<std::optional<can::c_can_frame>> {
    if (!m_opened) {
        return make_error("File not opened", e_error_category::io);
    }

    std::string line;
    while (std::getline(m_file, line)) {
        // Skip empty lines
        if (line.empty()) {
            continue;
        }
        // Skip comment lines (starting with ';' or containing keywords)
        if (line[0] == ';') {
            continue;
        }
        // Skip header lines like "date", "base", "internal events logged", "Begin Triggerblock", "End Triggerblock"
        if (line.find("date") == 0 || line.find("base") == 0 ||
            line.find("internal") == 0 || line.find("no internal") == 0 ||
            line.find("Begin") == 0 || line.find("End") == 0 ||
            line.find("Start") == 0) {
            continue;
        }

        auto frame = parse_frame_line(line);
        if (frame) {
            m_info.frame_count++;
            if (m_info.frame_count == 1) {
                m_info.start_time = frame->timestamp;
            }
            m_info.end_time = frame->timestamp;
            return *frame;
        }
    }

    return std::optional<can::c_can_frame>{std::nullopt};
}

auto c_asc_reader::read_all() -> result_t<std::vector<can::c_can_frame>> {
    if (!m_opened) {
        return make_error("File not opened", e_error_category::io);
    }

    // Reset to beginning
    auto reset_result = reset();
    if (!reset_result) {
        return std::unexpected(reset_result.error());
    }

    std::vector<can::c_can_frame> frames;
    while (true) {
        auto result = read_next();
        if (!result) {
            return std::unexpected(result.error());
        }
        if (!result->has_value()) {
            break;
        }
        frames.push_back(result->value());
    }
    return frames;
}

auto c_asc_reader::info() const -> c_trace_info {
    return m_info;
}

auto c_asc_reader::supported_extensions() const -> std::vector<std::string> {
    return {".asc"};
}

auto c_asc_reader::reset() -> void_result_t {
    if (!m_opened) {
        return make_error("File not opened", e_error_category::io);
    }

    m_file.clear();
    m_file.seekg(0, std::ios::beg);
    m_info.frame_count = 0;
    m_info.start_time = 0;
    m_info.end_time = 0;
    return {};
}

auto c_asc_reader::parse_frame_line(const std::string& line) -> std::optional<can::c_can_frame> {
    // Format: <timestamp> <channel> <id> <dir> d <dlc> <data bytes>
    // Example: "0.123456 1 100 Rx d 8 01 02 03 04 05 06 07 08"
    // Also: "   0.123456 1  100             Rx   d 8 01 02 03 04 05 06 07 08"
    std::istringstream iss(line);

    std::string timestamp_str;
    iss >> timestamp_str;
    if (timestamp_str.empty()) {
        return std::nullopt;
    }

    // Parse timestamp (seconds as floating point)
    char* end_ptr = nullptr;
    double timestamp_sec = std::strtod(timestamp_str.c_str(), &end_ptr);
    if (end_ptr == timestamp_str.c_str()) {
        return std::nullopt; // Not a number — probably a header line
    }

    std::string channel_str;
    iss >> channel_str;
    if (channel_str.empty()) {
        return std::nullopt;
    }

    std::string id_str;
    iss >> id_str;
    if (id_str.empty()) {
        return std::nullopt;
    }

    // Parse CAN ID (hex, possibly with 'x' or 'X' suffix for extended)
    bool extended = false;
    if (!id_str.empty() && (id_str.back() == 'x' || id_str.back() == 'X')) {
        extended = true;
        id_str.pop_back();
    }

    std::uint32_t can_id = 0;
    auto [ptr, ec] = std::from_chars(id_str.data(), id_str.data() + id_str.size(), can_id, 16);
    if (ec != std::errc{}) {
        return std::nullopt;
    }

    std::string dir_str;
    iss >> dir_str;
    // dir_str is "Rx" or "Tx" — we don't use it but need to consume it

    std::string type_str;
    iss >> type_str;
    if (type_str != "d" && type_str != "r") {
        return std::nullopt; // Must be data frame ('d') or remote ('r')
    }

    std::string dlc_str;
    iss >> dlc_str;
    std::uint8_t dlc = 0;
    {
        auto [p, e] = std::from_chars(dlc_str.data(), dlc_str.data() + dlc_str.size(), dlc);
        if (e != std::errc{}) {
            return std::nullopt;
        }
    }

    can::c_can_frame frame{};
    frame.id = can_id;
    frame.dlc = dlc;
    frame.timestamp = static_cast<timestamp_us_t>(timestamp_sec * 1'000'000.0);
    frame.flags.extended = extended;
    frame.flags.remote = (type_str == "r");

    // Parse data bytes
    auto byte_count = frame.data_length();
    for (std::size_t i = 0; i < byte_count; ++i) {
        std::string byte_str;
        iss >> byte_str;
        if (byte_str.empty()) {
            break;
        }
        std::uint8_t byte_val = 0;
        auto [p, e] = std::from_chars(byte_str.data(), byte_str.data() + byte_str.size(), byte_val, 16);
        if (e != std::errc{}) {
            break;
        }
        frame.data[i] = byte_val;
    }

    return frame;
}

} // namespace interface::can_trace
