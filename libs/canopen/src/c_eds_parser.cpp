/// @file c_eds_parser.cpp
/// @brief EDS parser implementation.

#include "interface/canopen/c_eds_parser.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace interface::canopen {

auto c_eds_parser::parse(const std::filesystem::path& path)
    -> result_t<c_object_dictionary>
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return make_error("Failed to open EDS file: " + path.string(), e_error_category::io);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return parse_string(content);
}

auto c_eds_parser::parse_string(const std::string& content)
    -> result_t<c_object_dictionary>
{
    auto sections = parse_ini(content);
    parse_file_info(sections);
    parse_device_info(sections);

    c_object_dictionary od;

    // Parse object lists from all three categories
    for (const auto& list_name : {"MandatoryObjects", "OptionalObjects", "ManufacturerSpecificObjects"}) {
        auto indices = parse_object_list(sections, list_name);
        for (auto index : indices) {
            auto entry = parse_object(sections, index);
            if (entry) {
                od.add_entry(std::move(*entry));
            }
        }
    }

    return od;
}

auto c_eds_parser::file_info() const -> const c_eds_file_info& {
    return m_file_info;
}

auto c_eds_parser::device_info() const -> const c_eds_device_info& {
    return m_device_info;
}

auto c_eds_parser::parse_ini(const std::string& content) -> section_map_t {
    section_map_t sections;
    std::istringstream stream(content);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        // Trim
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line[start] == ';' || line[start] == '#') {
            continue; // Empty or comment
        }
        line = line.substr(start);

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

        auto eq_pos = line.find('=');
        if (eq_pos != std::string::npos && !current_section.empty()) {
            auto key = line.substr(0, eq_pos);
            auto value = line.substr(eq_pos + 1);
            // Trim key and value
            while (!key.empty() && key.back() == ' ') key.pop_back();
            while (!value.empty() && value.front() == ' ') value = value.substr(1);
            sections[current_section][key] = value;
        }
    }

    return sections;
}

auto c_eds_parser::parse_file_info(const section_map_t& sections) -> void {
    auto it = sections.find("FileInfo");
    if (it == sections.end()) return;

    const auto& s = it->second;
    if (auto f = s.find("FileName"); f != s.end()) m_file_info.file_name = f->second;
    if (auto f = s.find("FileVersion"); f != s.end()) m_file_info.file_version = f->second;
    if (auto f = s.find("FileRevision"); f != s.end()) m_file_info.file_revision = f->second;
    if (auto f = s.find("Description"); f != s.end()) m_file_info.description = f->second;
    if (auto f = s.find("CreationDate"); f != s.end()) m_file_info.creation_date = f->second;
    if (auto f = s.find("CreatedBy"); f != s.end()) m_file_info.created_by = f->second;
}

auto c_eds_parser::parse_device_info(const section_map_t& sections) -> void {
    auto it = sections.find("DeviceInfo");
    if (it == sections.end()) return;

    const auto& s = it->second;
    if (auto f = s.find("VendorName"); f != s.end()) m_device_info.vendor_name = f->second;
    if (auto f = s.find("ProductName"); f != s.end()) m_device_info.product_name = f->second;
    if (auto f = s.find("VendorNumber"); f != s.end()) {
        m_device_info.vendor_number = static_cast<std::uint32_t>(std::strtoul(f->second.c_str(), nullptr, 0));
    }
    if (auto f = s.find("ProductNumber"); f != s.end()) {
        m_device_info.product_number = static_cast<std::uint32_t>(std::strtoul(f->second.c_str(), nullptr, 0));
    }
    if (auto f = s.find("RevisionNumber"); f != s.end()) {
        m_device_info.revision_number = static_cast<std::uint32_t>(std::strtoul(f->second.c_str(), nullptr, 0));
    }
}

auto c_eds_parser::parse_object_list(
    const section_map_t& sections,
    const std::string& list_section
) -> std::vector<std::uint16_t> {
    std::vector<std::uint16_t> indices;

    auto it = sections.find(list_section);
    if (it == sections.end()) return indices;

    const auto& s = it->second;
    // SupportedObjects=N, then 1=0x1000, 2=0x1001, etc.
    auto count_it = s.find("SupportedObjects");
    if (count_it == s.end()) return indices;

    auto count = static_cast<int>(std::strtol(count_it->second.c_str(), nullptr, 0));
    for (int i = 1; i <= count; ++i) {
        auto idx_it = s.find(std::to_string(i));
        if (idx_it != s.end()) {
            auto idx = static_cast<std::uint16_t>(std::strtoul(idx_it->second.c_str(), nullptr, 0));
            indices.push_back(idx);
        }
    }

    return indices;
}

auto c_eds_parser::parse_object(
    const section_map_t& sections,
    std::uint16_t index
) -> std::optional<c_od_entry> {
    // Section name is the hex index, e.g., "1000" or "1018"
    char section_name[8];
    std::snprintf(section_name, sizeof(section_name), "%04X", index);

    auto it = sections.find(section_name);
    if (it == sections.end()) return std::nullopt;

    const auto& s = it->second;

    c_od_entry entry{};
    entry.index = index;

    if (auto f = s.find("ParameterName"); f != s.end()) entry.name = f->second;

    // ObjectType: 7=VAR, 8=ARRAY, 9=RECORD
    if (auto f = s.find("ObjectType"); f != s.end()) {
        auto ot = static_cast<int>(std::strtol(f->second.c_str(), nullptr, 0));
        switch (ot) {
            case 0x07: entry.object_type = e_od_object_type::variable; break;
            case 0x08: entry.object_type = e_od_object_type::array; break;
            case 0x09: entry.object_type = e_od_object_type::record; break;
            default:   entry.object_type = e_od_object_type::variable; break;
        }
    }

    if (entry.object_type == e_od_object_type::variable) {
        // Simple variable — create a single sub-entry at sub-index 0
        c_od_sub_entry sub{};
        sub.sub_index = 0;
        sub.name = entry.name;

        if (auto f = s.find("DataType"); f != s.end()) {
            auto dt = static_cast<std::uint16_t>(std::strtoul(f->second.c_str(), nullptr, 0));
            sub.data_type = parse_data_type(dt);
        }
        if (auto f = s.find("AccessType"); f != s.end()) {
            sub.access = parse_access_type(f->second);
        }
        if (auto f = s.find("DefaultValue"); f != s.end()) {
            sub.default_value = f->second;
        }

        entry.sub_entries.push_back(std::move(sub));
    } else {
        // ARRAY or RECORD — parse SubNumber and sub-entries
        int sub_count = 0;
        if (auto f = s.find("SubNumber"); f != s.end()) {
            sub_count = static_cast<int>(std::strtol(f->second.c_str(), nullptr, 0));
        }

        for (int i = 0; i < sub_count; ++i) {
            auto sub_entry = parse_sub_object(sections, index, static_cast<std::uint8_t>(i));
            if (sub_entry) {
                entry.sub_entries.push_back(std::move(*sub_entry));
            }
        }
    }

    return entry;
}

auto c_eds_parser::parse_sub_object(
    const section_map_t& sections,
    std::uint16_t index,
    std::uint8_t sub_index
) -> std::optional<c_od_sub_entry> {
    // Section: "1018sub0", "1018sub1", etc.
    char section_name[16];
    std::snprintf(section_name, sizeof(section_name), "%04Xsub%X",
                  static_cast<unsigned>(index), static_cast<unsigned>(sub_index));

    auto it = sections.find(section_name);
    if (it == sections.end()) return std::nullopt;

    const auto& s = it->second;

    c_od_sub_entry sub{};
    sub.sub_index = sub_index;

    if (auto f = s.find("ParameterName"); f != s.end()) sub.name = f->second;
    if (auto f = s.find("DataType"); f != s.end()) {
        auto dt = static_cast<std::uint16_t>(std::strtoul(f->second.c_str(), nullptr, 0));
        sub.data_type = parse_data_type(dt);
    }
    if (auto f = s.find("AccessType"); f != s.end()) {
        sub.access = parse_access_type(f->second);
    }
    if (auto f = s.find("DefaultValue"); f != s.end()) {
        sub.default_value = f->second;
    }

    return sub;
}

auto c_eds_parser::parse_data_type(std::uint16_t type_code) -> e_od_data_type {
    // Map CiA 301 data type codes
    switch (type_code) {
        case 0x0001: return e_od_data_type::boolean;
        case 0x0002: return e_od_data_type::integer8;
        case 0x0003: return e_od_data_type::integer16;
        case 0x0004: return e_od_data_type::integer32;
        case 0x0005: return e_od_data_type::unsigned8;
        case 0x0006: return e_od_data_type::unsigned16;
        case 0x0007: return e_od_data_type::unsigned32;
        case 0x0008: return e_od_data_type::real32;
        case 0x0009: return e_od_data_type::visible_string;
        case 0x000A: return e_od_data_type::octet_string;
        case 0x000B: return e_od_data_type::unicode_string;
        case 0x000F: return e_od_data_type::domain;
        case 0x0011: return e_od_data_type::real64;
        case 0x0015: return e_od_data_type::integer64;
        case 0x0018: return e_od_data_type::unsigned64;
        default:     return e_od_data_type::unsigned8;
    }
}

auto c_eds_parser::parse_access_type(const std::string& access) -> e_od_access {
    if (access == "ro" || access == "RO") return e_od_access::read_only;
    if (access == "wo" || access == "WO") return e_od_access::write_only;
    if (access == "rw" || access == "RW") return e_od_access::read_write;
    if (access == "const" || access == "CONST") return e_od_access::constant;
    return e_od_access::read_only;
}

} // namespace interface::canopen
