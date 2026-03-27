#pragma once

/// @file script_engine.hpp
/// @brief CAN script execution engine.

#include "interface/can_script/script.hpp"
#include "interface/can_hal/i_can_adapter.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace interface::can_script {

// =============================================================================
// Engine state
// =============================================================================

enum class e_engine_state {
    idle,
    running,
    paused,
    finished,
    error,
};

// =============================================================================
// Engine events
// =============================================================================

struct c_engine_event {
    enum class e_type {
        step_started,
        step_completed,
        script_finished,
        error,
        timeout,
    };

    e_type type;
    std::string step_label;
    std::string message;
    timestamp_us_t timestamp{0};
};

using engine_event_callback_t = std::function<void(const c_engine_event&)>;

// =============================================================================
// Script engine
// =============================================================================

class c_script_engine {
public:
    explicit c_script_engine(std::shared_ptr<can_hal::i_can_adapter> adapter);

    auto load_script(c_script script) -> void;
    auto start() -> result_t<void>;
    auto stop() -> void;
    auto pause() -> void;
    auto resume() -> void;
    auto process(timestamp_us_t now_us) -> void;
    auto on_frame_received(const can::c_can_frame& frame) -> void;

    [[nodiscard]] auto state() const -> e_engine_state;
    [[nodiscard]] auto current_step() const -> std::size_t;
    auto set_event_callback(engine_event_callback_t cb) -> void;
    [[nodiscard]] auto script() const -> const c_script&;
    [[nodiscard]] auto event_log() const -> const std::vector<c_engine_event>&;

private:
    std::shared_ptr<can_hal::i_can_adapter> m_adapter;
    c_script m_script;
    e_engine_state m_state{e_engine_state::idle};
    std::size_t m_current_step{0};
    timestamp_us_t m_step_start_time{0};
    std::uint32_t m_repeat_counter{0};
    bool m_step_triggered{false};

    engine_event_callback_t m_event_callback;
    std::vector<c_engine_event> m_event_log;

    auto execute_action(const c_script_action& action, timestamp_us_t now_us) -> void;
    auto advance_to_step(std::size_t index, timestamp_us_t now_us) -> void;
    auto advance_to_label(const std::string& label, timestamp_us_t now_us) -> bool;
    auto find_step_by_label(const std::string& label) const -> std::optional<std::size_t>;
    auto emit_event(const c_engine_event& event) -> void;
};

} // namespace interface::can_script
