#include "interface/can_script/script_engine.hpp"

namespace interface::can_script {

c_script_engine::c_script_engine(std::shared_ptr<can_hal::i_can_adapter> adapter)
    : m_adapter(std::move(adapter)) {}

auto c_script_engine::load_script(c_script script) -> void {
    m_script = std::move(script);
    m_state = e_engine_state::idle;
    m_current_step = 0;
    m_step_start_time = 0;
    m_repeat_counter = 0;
    m_step_triggered = false;
    m_event_log.clear();
}

auto c_script_engine::start() -> result_t<void> {
    if (m_script.steps.empty()) {
        m_state = e_engine_state::error;
        return make_error("Cannot start script with no steps", e_error_category::config);
    }

    m_state = e_engine_state::running;
    m_current_step = 0;
    m_repeat_counter = 0;
    m_step_triggered = false;
    m_step_start_time = 0;

    return {};
}

auto c_script_engine::stop() -> void {
    m_state = e_engine_state::idle;
}

auto c_script_engine::pause() -> void {
    if (m_state == e_engine_state::running) {
        m_state = e_engine_state::paused;
    }
}

auto c_script_engine::resume() -> void {
    if (m_state == e_engine_state::paused) {
        m_state = e_engine_state::running;
    }
}

auto c_script_engine::process(timestamp_us_t now_us) -> void {
    if (m_state != e_engine_state::running) {
        return;
    }

    if (m_current_step >= m_script.steps.size()) {
        if (m_script.loop) {
            m_current_step = 0;
            m_step_start_time = now_us;
            m_step_triggered = false;
            m_repeat_counter = 0;
        } else {
            m_state = e_engine_state::finished;
            emit_event(c_engine_event{
                .type = c_engine_event::e_type::script_finished,
                .step_label = {},
                .message = "Script finished",
                .timestamp = now_us,
            });
            return;
        }
    }

    const auto& step = m_script.steps[m_current_step];

    // Initialize step start time on first process call for this step
    if (m_step_start_time == 0) {
        m_step_start_time = now_us;
        emit_event(c_engine_event{
            .type = c_engine_event::e_type::step_started,
            .step_label = step.label,
            .message = "Step started: " + step.label,
            .timestamp = now_us,
        });
    }

    switch (step.trigger_type) {
        case e_trigger_type::immediate: {
            execute_action(step.action, now_us);
            emit_event(c_engine_event{
                .type = c_engine_event::e_type::step_completed,
                .step_label = step.label,
                .message = "Step completed: " + step.label,
                .timestamp = now_us,
            });

            if (step.repeat) {
                ++m_repeat_counter;
                if (step.repeat_count > 0 && m_repeat_counter >= step.repeat_count) {
                    advance_to_step(m_current_step + 1, now_us);
                } else {
                    m_step_start_time = now_us;
                    m_step_triggered = false;
                }
            } else {
                if (!step.on_match_goto.empty()) {
                    advance_to_label(step.on_match_goto, now_us);
                } else {
                    advance_to_step(m_current_step + 1, now_us);
                }
            }
            break;
        }

        case e_trigger_type::delay: {
            auto elapsed = now_us - m_step_start_time;
            if (elapsed >= step.delay.count()) {
                execute_action(step.action, now_us);
                emit_event(c_engine_event{
                    .type = c_engine_event::e_type::step_completed,
                    .step_label = step.label,
                    .message = "Step completed: " + step.label,
                    .timestamp = now_us,
                });

                if (step.repeat) {
                    ++m_repeat_counter;
                    if (step.repeat_count > 0 && m_repeat_counter >= step.repeat_count) {
                        advance_to_step(m_current_step + 1, now_us);
                    } else {
                        m_step_start_time = now_us;
                        m_step_triggered = false;
                    }
                } else {
                    if (!step.on_match_goto.empty()) {
                        advance_to_label(step.on_match_goto, now_us);
                    } else {
                        advance_to_step(m_current_step + 1, now_us);
                    }
                }
            }
            break;
        }

        case e_trigger_type::on_receive: {
            if (m_step_triggered) {
                execute_action(step.action, now_us);
                emit_event(c_engine_event{
                    .type = c_engine_event::e_type::step_completed,
                    .step_label = step.label,
                    .message = "Step completed: " + step.label,
                    .timestamp = now_us,
                });

                if (step.repeat) {
                    ++m_repeat_counter;
                    if (step.repeat_count > 0 && m_repeat_counter >= step.repeat_count) {
                        advance_to_step(m_current_step + 1, now_us);
                    } else {
                        m_step_start_time = now_us;
                        m_step_triggered = false;
                    }
                } else {
                    if (!step.on_match_goto.empty()) {
                        advance_to_label(step.on_match_goto, now_us);
                    } else {
                        advance_to_step(m_current_step + 1, now_us);
                    }
                }
            }
            break;
        }

        case e_trigger_type::on_receive_or_timeout: {
            if (m_step_triggered) {
                execute_action(step.action, now_us);
                emit_event(c_engine_event{
                    .type = c_engine_event::e_type::step_completed,
                    .step_label = step.label,
                    .message = "Step completed: " + step.label,
                    .timestamp = now_us,
                });

                if (step.repeat) {
                    ++m_repeat_counter;
                    if (step.repeat_count > 0 && m_repeat_counter >= step.repeat_count) {
                        advance_to_step(m_current_step + 1, now_us);
                    } else {
                        m_step_start_time = now_us;
                        m_step_triggered = false;
                    }
                } else {
                    if (!step.on_match_goto.empty()) {
                        advance_to_label(step.on_match_goto, now_us);
                    } else {
                        advance_to_step(m_current_step + 1, now_us);
                    }
                }
            } else if (step.timeout.count() > 0) {
                auto elapsed = now_us - m_step_start_time;
                if (elapsed >= step.timeout.count()) {
                    emit_event(c_engine_event{
                        .type = c_engine_event::e_type::timeout,
                        .step_label = step.label,
                        .message = "Step timed out: " + step.label,
                        .timestamp = now_us,
                    });

                    if (!step.on_timeout_goto.empty()) {
                        advance_to_label(step.on_timeout_goto, now_us);
                    } else {
                        advance_to_step(m_current_step + 1, now_us);
                    }
                }
            }
            break;
        }
    }
}

auto c_script_engine::on_frame_received(const can::c_can_frame& frame) -> void {
    if (m_state != e_engine_state::running) {
        return;
    }

    if (m_current_step >= m_script.steps.size()) {
        return;
    }

    const auto& step = m_script.steps[m_current_step];

    if (step.trigger_type == e_trigger_type::on_receive ||
        step.trigger_type == e_trigger_type::on_receive_or_timeout) {
        if (step.match.matches(frame)) {
            m_step_triggered = true;
        }
    }
}

auto c_script_engine::state() const -> e_engine_state {
    return m_state;
}

auto c_script_engine::current_step() const -> std::size_t {
    return m_current_step;
}

auto c_script_engine::set_event_callback(engine_event_callback_t cb) -> void {
    m_event_callback = std::move(cb);
}

auto c_script_engine::script() const -> const c_script& {
    return m_script;
}

auto c_script_engine::event_log() const -> const std::vector<c_engine_event>& {
    return m_event_log;
}

auto c_script_engine::execute_action(const c_script_action& action, timestamp_us_t now_us) -> void {
    switch (action.type) {
        case e_action_type::send_frame: {
            if (m_adapter) {
                auto frame = action.frame.to_frame(now_us);
                (void)m_adapter->send(frame);
            }
            break;
        }
        case e_action_type::send_sequence: {
            if (m_adapter) {
                for (const auto& entry : action.sequence) {
                    // Note: In a real implementation, delays would be handled asynchronously.
                    // For testing, we send all frames immediately (delay is recorded but not waited).
                    auto frame = entry.frame.to_frame(now_us);
                    (void)m_adapter->send(frame);
                }
            }
            break;
        }
        case e_action_type::log_message:
            // Logged via event system
            break;
        case e_action_type::set_variable:
            // Variable storage not yet implemented
            break;
        case e_action_type::no_op:
            break;
    }
}

auto c_script_engine::advance_to_step(std::size_t index, timestamp_us_t now_us) -> void {
    m_current_step = index;
    m_step_start_time = now_us;
    m_step_triggered = false;
    m_repeat_counter = 0;

    // Emit step_started for the new step if it exists
    if (m_current_step < m_script.steps.size()) {
        emit_event(c_engine_event{
            .type = c_engine_event::e_type::step_started,
            .step_label = m_script.steps[m_current_step].label,
            .message = "Step started: " + m_script.steps[m_current_step].label,
            .timestamp = now_us,
        });
    }
}

auto c_script_engine::advance_to_label(const std::string& label, timestamp_us_t now_us) -> bool {
    auto index = find_step_by_label(label);
    if (index) {
        advance_to_step(*index, now_us);
        return true;
    }
    return false;
}

auto c_script_engine::find_step_by_label(const std::string& label) const -> std::optional<std::size_t> {
    for (std::size_t i = 0; i < m_script.steps.size(); ++i) {
        if (m_script.steps[i].label == label) {
            return i;
        }
    }
    return std::nullopt;
}

auto c_script_engine::emit_event(const c_engine_event& event) -> void {
    m_event_log.push_back(event);
    if (m_event_callback) {
        m_event_callback(event);
    }
}

} // namespace interface::can_script
