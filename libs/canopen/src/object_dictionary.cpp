/// @file object_dictionary.cpp
/// @brief Object Dictionary implementation.

#include "interface/canopen/object_dictionary.hpp"

#include <algorithm>

namespace interface::canopen {

auto c_od_entry::find_sub(std::uint8_t sub_index) const
    -> std::optional<std::reference_wrapper<const c_od_sub_entry>>
{
    auto it = std::ranges::find_if(sub_entries, [sub_index](const c_od_sub_entry& sub) {
        return sub.sub_index == sub_index;
    });
    if (it != sub_entries.end()) {
        return std::cref(*it);
    }
    return std::nullopt;
}

auto c_od_entry::find_sub_mut(std::uint8_t sub_index)
    -> std::optional<std::reference_wrapper<c_od_sub_entry>>
{
    auto it = std::ranges::find_if(sub_entries, [sub_index](const c_od_sub_entry& sub) {
        return sub.sub_index == sub_index;
    });
    if (it != sub_entries.end()) {
        return std::ref(*it);
    }
    return std::nullopt;
}

auto c_object_dictionary::add_entry(c_od_entry entry) -> void {
    auto index = entry.index;
    m_entries.insert_or_assign(index, std::move(entry));
}

auto c_object_dictionary::find(std::uint16_t index) const
    -> std::optional<std::reference_wrapper<const c_od_entry>>
{
    auto it = m_entries.find(index);
    if (it != m_entries.end()) {
        return std::cref(it->second);
    }
    return std::nullopt;
}

auto c_object_dictionary::find_mut(std::uint16_t index)
    -> std::optional<std::reference_wrapper<c_od_entry>>
{
    auto it = m_entries.find(index);
    if (it != m_entries.end()) {
        return std::ref(it->second);
    }
    return std::nullopt;
}

auto c_object_dictionary::entries() const -> const std::map<std::uint16_t, c_od_entry>& {
    return m_entries;
}

auto c_object_dictionary::size() const noexcept -> std::size_t {
    return m_entries.size();
}

auto c_object_dictionary::clear() -> void {
    m_entries.clear();
}

} // namespace interface::canopen
