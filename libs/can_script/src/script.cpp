#include "interface/can_script/script.hpp"

#include <fstream>
#include <sstream>

namespace interface::can_script {

// =============================================================================
// Helpers
// =============================================================================

namespace {

auto parse_hex_or_int(const nlohmann::json& j) -> result_t<std::uint32_t> {
    if (j.is_number_unsigned()) {
        return j.get<std::uint32_t>();
    }
    if (j.is_number_integer()) {
        return static_cast<std::uint32_t>(j.get<std::int64_t>());
    }
    if (j.is_string()) {
        auto s = j.get<std::string>();
        try {
            auto val = std::stoul(s, nullptr, 0);  // auto-detects 0x prefix
            return static_cast<std::uint32_t>(val);
        } catch (...) {
            return make_error("Invalid hex/int string: " + s, e_error_category::parse);
        }
    }
    return make_error("Expected number or hex string", e_error_category::parse);
}

auto parse_byte_value(const nlohmann::json& j) -> result_t<byte_t> {
    auto result = parse_hex_or_int(j);
    if (!result) {
        return std::unexpected(result.error());
    }
    return static_cast<byte_t>(*result);
}

auto parse_trigger_type(const std::string& s) -> result_t<e_trigger_type> {
    if (s == "immediate") return e_trigger_type::immediate;
    if (s == "delay") return e_trigger_type::delay;
    if (s == "on_receive") return e_trigger_type::on_receive;
    if (s == "on_receive_or_timeout") return e_trigger_type::on_receive_or_timeout;
    return make_error("Unknown trigger type: " + s, e_error_category::parse);
}

auto trigger_type_to_string(e_trigger_type t) -> std::string {
    switch (t) {
        case e_trigger_type::immediate: return "immediate";
        case e_trigger_type::delay: return "delay";
        case e_trigger_type::on_receive: return "on_receive";
        case e_trigger_type::on_receive_or_timeout: return "on_receive_or_timeout";
    }
    return "unknown";
}

auto parse_action_type(const std::string& s) -> result_t<e_action_type> {
    if (s == "send_frame") return e_action_type::send_frame;
    if (s == "send_sequence") return e_action_type::send_sequence;
    if (s == "log_message") return e_action_type::log_message;
    if (s == "set_variable") return e_action_type::set_variable;
    if (s == "no_op") return e_action_type::no_op;
    return make_error("Unknown action type: " + s, e_error_category::parse);
}

auto action_type_to_string(e_action_type t) -> std::string {
    switch (t) {
        case e_action_type::send_frame: return "send_frame";
        case e_action_type::send_sequence: return "send_sequence";
        case e_action_type::log_message: return "log_message";
        case e_action_type::set_variable: return "set_variable";
        case e_action_type::no_op: return "no_op";
    }
    return "unknown";
}

auto parse_byte_matcher(const nlohmann::json& j) -> result_t<can::c_byte_matcher> {
    if (!j.contains("type")) {
        return make_error("Byte matcher missing 'type'", e_error_category::parse);
    }

    auto type_str = j["type"].get<std::string>();

    if (type_str == "any") {
        return can::c_byte_matcher::any();
    }
    if (type_str == "exact") {
        if (!j.contains("value")) {
            return make_error("Exact matcher missing 'value'", e_error_category::parse);
        }
        auto val = parse_byte_value(j["value"]);
        if (!val) return std::unexpected(val.error());
        return can::c_byte_matcher::exact(*val);
    }
    if (type_str == "masked") {
        if (!j.contains("value") || !j.contains("mask")) {
            return make_error("Masked matcher missing 'value' or 'mask'", e_error_category::parse);
        }
        auto val = parse_byte_value(j["value"]);
        if (!val) return std::unexpected(val.error());
        auto mask = parse_byte_value(j["mask"]);
        if (!mask) return std::unexpected(mask.error());
        return can::c_byte_matcher::masked(*val, *mask);
    }
    if (type_str == "range") {
        if (!j.contains("low") || !j.contains("high")) {
            return make_error("Range matcher missing 'low' or 'high'", e_error_category::parse);
        }
        auto low = parse_byte_value(j["low"]);
        if (!low) return std::unexpected(low.error());
        auto high = parse_byte_value(j["high"]);
        if (!high) return std::unexpected(high.error());
        return can::c_byte_matcher::range(*low, *high);
    }
    return make_error("Unknown byte matcher type: " + type_str, e_error_category::parse);
}

auto parse_frame_match(const nlohmann::json& j) -> result_t<c_frame_match> {
    c_frame_match match;

    if (j.contains("id")) {
        auto id = parse_hex_or_int(j["id"]);
        if (!id) return std::unexpected(id.error());
        match.id = *id;
    }

    if (j.contains("id_mask")) {
        auto mask = parse_hex_or_int(j["id_mask"]);
        if (!mask) return std::unexpected(mask.error());
        match.id_mask = *mask;
    }

    if (j.contains("payload")) {
        for (const auto& bm_json : j["payload"]) {
            auto bm = parse_byte_matcher(bm_json);
            if (!bm) return std::unexpected(bm.error());
            match.payload_matchers.push_back(*bm);
        }
    }

    return match;
}

auto parse_frame_template(const nlohmann::json& j) -> result_t<c_frame_template> {
    c_frame_template tmpl;

    if (j.contains("id")) {
        auto id = parse_hex_or_int(j["id"]);
        if (!id) return std::unexpected(id.error());
        tmpl.id = *id;
    }

    if (j.contains("dlc")) {
        tmpl.dlc = j["dlc"].get<std::uint8_t>();
    }

    if (j.contains("data")) {
        const auto& data_arr = j["data"];
        for (std::size_t i = 0; i < data_arr.size() && i < 8; ++i) {
            auto val = parse_byte_value(data_arr[i]);
            if (!val) return std::unexpected(val.error());
            tmpl.data[i] = *val;
        }
        if (!j.contains("dlc")) {
            tmpl.dlc = static_cast<std::uint8_t>(std::min(data_arr.size(), std::size_t{8}));
        }
    }

    if (j.contains("extended")) {
        tmpl.flags.extended = j["extended"].get<bool>();
    }

    return tmpl;
}

auto frame_template_to_json(const c_frame_template& tmpl) -> nlohmann::json {
    nlohmann::json j;
    j["id"] = std::format("0x{:03X}", tmpl.id);
    j["dlc"] = tmpl.dlc;

    nlohmann::json data_arr = nlohmann::json::array();
    for (std::size_t i = 0; i < tmpl.dlc && i < 8; ++i) {
        data_arr.push_back(std::format("0x{:02X}", tmpl.data[i]));
    }
    j["data"] = data_arr;

    if (tmpl.flags.extended) {
        j["extended"] = true;
    }

    return j;
}

auto parse_action(const nlohmann::json& j) -> result_t<c_script_action> {
    c_script_action action;

    if (!j.contains("type")) {
        return make_error("Action missing 'type'", e_error_category::parse);
    }

    auto type = parse_action_type(j["type"].get<std::string>());
    if (!type) return std::unexpected(type.error());
    action.type = *type;

    switch (action.type) {
        case e_action_type::send_frame: {
            if (!j.contains("frame")) {
                return make_error("send_frame action missing 'frame'", e_error_category::parse);
            }
            auto tmpl = parse_frame_template(j["frame"]);
            if (!tmpl) return std::unexpected(tmpl.error());
            action.frame = *tmpl;
            break;
        }
        case e_action_type::send_sequence: {
            if (!j.contains("sequence")) {
                return make_error("send_sequence action missing 'sequence'", e_error_category::parse);
            }
            for (const auto& entry_json : j["sequence"]) {
                c_script_action::sequence_entry entry;
                if (entry_json.contains("delay_ms")) {
                    auto ms = entry_json["delay_ms"].get<std::int64_t>();
                    entry.delay_before = std::chrono::microseconds(ms * 1000);
                }
                if (!entry_json.contains("frame")) {
                    return make_error("Sequence entry missing 'frame'", e_error_category::parse);
                }
                auto tmpl = parse_frame_template(entry_json["frame"]);
                if (!tmpl) return std::unexpected(tmpl.error());
                entry.frame = *tmpl;
                action.sequence.push_back(std::move(entry));
            }
            break;
        }
        case e_action_type::log_message: {
            if (j.contains("message")) {
                action.message = j["message"].get<std::string>();
            }
            break;
        }
        case e_action_type::set_variable: {
            if (j.contains("variable_name")) {
                action.variable_name = j["variable_name"].get<std::string>();
            }
            if (j.contains("variable_value")) {
                action.variable_value = j["variable_value"].get<std::string>();
            }
            break;
        }
        case e_action_type::no_op:
            break;
    }

    return action;
}

auto action_to_json(const c_script_action& action) -> nlohmann::json {
    nlohmann::json j;
    j["type"] = action_type_to_string(action.type);

    switch (action.type) {
        case e_action_type::send_frame:
            j["frame"] = frame_template_to_json(action.frame);
            break;
        case e_action_type::send_sequence: {
            nlohmann::json seq = nlohmann::json::array();
            for (const auto& entry : action.sequence) {
                nlohmann::json ej;
                ej["delay_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    entry.delay_before).count();
                ej["frame"] = frame_template_to_json(entry.frame);
                seq.push_back(std::move(ej));
            }
            j["sequence"] = seq;
            break;
        }
        case e_action_type::log_message:
            j["message"] = action.message;
            break;
        case e_action_type::set_variable:
            j["variable_name"] = action.variable_name;
            j["variable_value"] = action.variable_value;
            break;
        case e_action_type::no_op:
            break;
    }

    return j;
}

auto parse_step(const nlohmann::json& j) -> result_t<c_script_step> {
    c_script_step step;

    if (j.contains("label")) {
        step.label = j["label"].get<std::string>();
    }

    // Parse trigger
    if (j.contains("trigger")) {
        const auto& trigger = j["trigger"];

        if (!trigger.contains("type")) {
            return make_error("Trigger missing 'type'", e_error_category::parse);
        }

        auto ttype = parse_trigger_type(trigger["type"].get<std::string>());
        if (!ttype) return std::unexpected(ttype.error());
        step.trigger_type = *ttype;

        if (trigger.contains("delay_ms")) {
            auto ms = trigger["delay_ms"].get<std::int64_t>();
            step.delay = std::chrono::microseconds(ms * 1000);
        }

        if (trigger.contains("match")) {
            auto match = parse_frame_match(trigger["match"]);
            if (!match) return std::unexpected(match.error());
            step.match = *match;
        }

        if (trigger.contains("timeout_ms")) {
            auto ms = trigger["timeout_ms"].get<std::int64_t>();
            step.timeout = std::chrono::microseconds(ms * 1000);
        }
    }

    // Parse action
    if (j.contains("action")) {
        auto action = parse_action(j["action"]);
        if (!action) return std::unexpected(action.error());
        step.action = *action;
    }

    // Flow control
    if (j.contains("on_timeout_goto")) {
        step.on_timeout_goto = j["on_timeout_goto"].get<std::string>();
    }
    if (j.contains("on_match_goto")) {
        step.on_match_goto = j["on_match_goto"].get<std::string>();
    }
    if (j.contains("repeat")) {
        step.repeat = j["repeat"].get<bool>();
    }
    if (j.contains("repeat_count")) {
        step.repeat_count = j["repeat_count"].get<std::uint32_t>();
    }

    return step;
}

auto step_to_json(const c_script_step& step) -> nlohmann::json {
    nlohmann::json j;

    if (!step.label.empty()) {
        j["label"] = step.label;
    }

    // Trigger
    nlohmann::json trigger;
    trigger["type"] = trigger_type_to_string(step.trigger_type);

    if (step.trigger_type == e_trigger_type::delay) {
        trigger["delay_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            step.delay).count();
    }

    if (step.trigger_type == e_trigger_type::on_receive ||
        step.trigger_type == e_trigger_type::on_receive_or_timeout) {
        nlohmann::json match_j;
        match_j["id"] = std::format("0x{:03X}", step.match.id);
        if (step.match.id_mask != 0x1FFFFFFF) {
            match_j["id_mask"] = std::format("0x{:08X}", step.match.id_mask);
        }
        if (!step.match.payload_matchers.empty()) {
            nlohmann::json payload = nlohmann::json::array();
            for (std::size_t idx = 0; idx < step.match.payload_matchers.size(); ++idx) {
                nlohmann::json bm_j;
                bm_j["type"] = "any";
                payload.push_back(bm_j);
            }
            match_j["payload"] = payload;
        }
        trigger["match"] = match_j;
    }

    if (step.trigger_type == e_trigger_type::on_receive_or_timeout) {
        trigger["timeout_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            step.timeout).count();
    }

    j["trigger"] = trigger;

    // Action
    j["action"] = action_to_json(step.action);

    // Flow control
    if (!step.on_timeout_goto.empty()) {
        j["on_timeout_goto"] = step.on_timeout_goto;
    }
    if (!step.on_match_goto.empty()) {
        j["on_match_goto"] = step.on_match_goto;
    }
    if (step.repeat) {
        j["repeat"] = step.repeat;
        j["repeat_count"] = step.repeat_count;
    }

    return j;
}

} // anonymous namespace

// =============================================================================
// c_frame_match
// =============================================================================

auto c_frame_match::matches(const can::c_can_frame& frame) const -> bool {
    if ((frame.id & id_mask) != (id & id_mask)) {
        return false;
    }

    for (std::size_t i = 0; i < payload_matchers.size(); ++i) {
        if (i >= frame.data_length()) {
            return false;
        }
        if (!payload_matchers[i].matches(frame.data[i])) {
            return false;
        }
    }

    return true;
}

// =============================================================================
// c_frame_template
// =============================================================================

auto c_frame_template::to_frame(timestamp_us_t ts) const -> can::c_can_frame {
    can::c_can_frame f;
    f.id = id;
    f.dlc = dlc;
    f.data = data;
    f.timestamp = ts;
    f.flags = flags;
    return f;
}

// =============================================================================
// c_script
// =============================================================================

auto c_script::from_json(const nlohmann::json& j) -> result_t<c_script> {
    c_script script;

    if (!j.contains("name")) {
        return make_error("Script missing 'name'", e_error_category::parse);
    }
    script.name = j["name"].get<std::string>();

    if (j.contains("description")) {
        script.description = j["description"].get<std::string>();
    }

    if (j.contains("loop")) {
        script.loop = j["loop"].get<bool>();
    }

    if (!j.contains("steps") || !j["steps"].is_array()) {
        return make_error("Script missing 'steps' array", e_error_category::parse);
    }

    for (const auto& step_json : j["steps"]) {
        auto step = parse_step(step_json);
        if (!step) return std::unexpected(step.error());
        script.steps.push_back(std::move(*step));
    }

    return script;
}

auto c_script::to_json() const -> nlohmann::json {
    nlohmann::json j;
    j["name"] = name;
    j["description"] = description;
    j["loop"] = loop;

    nlohmann::json steps_arr = nlohmann::json::array();
    for (const auto& step : steps) {
        steps_arr.push_back(step_to_json(step));
    }
    j["steps"] = steps_arr;

    return j;
}

auto c_script::load_from_file(const std::filesystem::path& path) -> result_t<c_script> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return make_error("Cannot open file: " + path.string(), e_error_category::io);
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::parse_error& e) {
        return make_error("JSON parse error: " + std::string(e.what()), e_error_category::parse);
    }

    return from_json(j);
}

auto c_script::save_to_file(const std::filesystem::path& path) const -> result_t<void> {
    std::ofstream file(path);
    if (!file.is_open()) {
        return make_error("Cannot open file for writing: " + path.string(), e_error_category::io);
    }

    file << to_json().dump(4);
    if (!file.good()) {
        return make_error("Error writing to file: " + path.string(), e_error_category::io);
    }

    return {};
}

} // namespace interface::can_script
