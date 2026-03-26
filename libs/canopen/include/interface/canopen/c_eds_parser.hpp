#pragma once

/// @file c_eds_parser.hpp
/// @brief EDS (Electronic Data Sheet) parser for CANopen Object Dictionary.

#include "interface/canopen/object_dictionary.hpp"
#include "interface/core/error.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace interface::canopen {

/// Parsed EDS file info section.
struct c_eds_file_info {
    std::string file_name;
    std::string file_version;
    std::string file_revision;
    std::string description;
    std::string creation_date;
    std::string created_by;
};

/// Parsed EDS device info section.
struct c_eds_device_info {
    std::string vendor_name;
    std::string product_name;
    std::uint32_t vendor_number{0};
    std::uint32_t product_number{0};
    std::uint32_t revision_number{0};
};

/// Parser for CANopen EDS (Electronic Data Sheet) files.
/// EDS files are INI-format describing a CANopen device's Object Dictionary.
class c_eds_parser {
public:
    c_eds_parser() = default;

    /// Parse an EDS file and populate an Object Dictionary.
    [[nodiscard]] auto parse(const std::filesystem::path& path)
        -> result_t<c_object_dictionary>;

    /// Parse EDS content from a string (for testing).
    [[nodiscard]] auto parse_string(const std::string& content)
        -> result_t<c_object_dictionary>;

    /// Get parsed file info (valid after parse).
    [[nodiscard]] auto file_info() const -> const c_eds_file_info&;

    /// Get parsed device info (valid after parse).
    [[nodiscard]] auto device_info() const -> const c_eds_device_info&;

private:
    using section_map_t = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;

    [[nodiscard]] auto parse_ini(const std::string& content) -> section_map_t;
    auto parse_file_info(const section_map_t& sections) -> void;
    auto parse_device_info(const section_map_t& sections) -> void;
    [[nodiscard]] auto parse_object_list(
        const section_map_t& sections,
        const std::string& list_section
    ) -> std::vector<std::uint16_t>;
    [[nodiscard]] auto parse_object(
        const section_map_t& sections,
        std::uint16_t index
    ) -> std::optional<c_od_entry>;
    [[nodiscard]] auto parse_sub_object(
        const section_map_t& sections,
        std::uint16_t index,
        std::uint8_t sub_index
    ) -> std::optional<c_od_sub_entry>;
    [[nodiscard]] static auto parse_data_type(std::uint16_t type_code) -> e_od_data_type;
    [[nodiscard]] static auto parse_access_type(const std::string& access) -> e_od_access;

    c_eds_file_info m_file_info;
    c_eds_device_info m_device_info;
};

} // namespace interface::canopen
