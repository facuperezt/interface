#pragma once

/// @file object_dictionary.hpp
/// @brief CANopen Object Dictionary types and in-memory representation.

#include "interface/core/types.hpp"
#include "interface/core/error.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace interface::canopen {

/// CANopen data types (CiA 301 §7.4.7).
enum class e_od_data_type : std::uint16_t {
    boolean         = 0x0001,
    integer8        = 0x0002,
    integer16       = 0x0003,
    integer32       = 0x0004,
    unsigned8       = 0x0005,
    unsigned16      = 0x0006,
    unsigned32      = 0x0007,
    real32          = 0x0008,
    visible_string  = 0x0009,
    octet_string    = 0x000A,
    unicode_string  = 0x000B,
    domain          = 0x000F,
    integer64       = 0x0015,
    unsigned64      = 0x0018,
    real64          = 0x0011,
};

/// Access type for an OD entry.
enum class e_od_access {
    read_only,
    write_only,
    read_write,
    constant,
};

/// Object type in the OD.
enum class e_od_object_type {
    variable,       ///< VAR (0x07)
    array,          ///< ARRAY (0x08)
    record,         ///< RECORD (0x09)
};

/// Value stored in an OD entry.
using od_value_t = std::variant<
    bool,
    std::int8_t, std::int16_t, std::int32_t, std::int64_t,
    std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
    float, double,
    std::string,
    byte_buffer_t
>;

/// A single sub-entry in the Object Dictionary.
struct c_od_sub_entry {
    std::uint8_t sub_index{0};
    std::string name;
    e_od_data_type data_type{e_od_data_type::unsigned8};
    e_od_access access{e_od_access::read_only};
    std::optional<od_value_t> default_value;
    std::optional<od_value_t> min_value;
    std::optional<od_value_t> max_value;
    std::optional<od_value_t> current_value; ///< Runtime value (for simulation)
};

/// An entry (object) in the Object Dictionary at a given index.
struct c_od_entry {
    std::uint16_t index{0};
    std::string name;
    e_od_object_type object_type{e_od_object_type::variable};
    std::vector<c_od_sub_entry> sub_entries;

    /// Get a sub-entry by sub-index.
    [[nodiscard]] auto find_sub(std::uint8_t sub_index) const
        -> std::optional<std::reference_wrapper<const c_od_sub_entry>>;

    /// Get a mutable sub-entry.
    [[nodiscard]] auto find_sub_mut(std::uint8_t sub_index)
        -> std::optional<std::reference_wrapper<c_od_sub_entry>>;
};

/// In-memory Object Dictionary.
class c_object_dictionary {
public:
    c_object_dictionary() = default;

    /// Add an entry to the dictionary.
    auto add_entry(c_od_entry entry) -> void;

    /// Find an entry by index.
    [[nodiscard]] auto find(std::uint16_t index) const
        -> std::optional<std::reference_wrapper<const c_od_entry>>;

    /// Find a mutable entry.
    [[nodiscard]] auto find_mut(std::uint16_t index)
        -> std::optional<std::reference_wrapper<c_od_entry>>;

    /// Get all entries (sorted by index).
    [[nodiscard]] auto entries() const -> const std::map<std::uint16_t, c_od_entry>&;

    /// Number of entries.
    [[nodiscard]] auto size() const noexcept -> std::size_t;

    /// Clear all entries.
    auto clear() -> void;

private:
    std::map<std::uint16_t, c_od_entry> m_entries;
};

} // namespace interface::canopen
