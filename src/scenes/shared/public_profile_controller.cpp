#include "shared/public_profile_controller.h"

#include <utility>

#include "tween.h"

namespace public_profile {

void controller::reset() {
    state_ = {};
    service_.reset();
    pending_effects_ = {};
}

void controller::open(std::string user_id) {
    if (user_id.empty()) {
        return;
    }
    if (state_.open && state_.requested_user_id == user_id && !state_.closing) {
        return;
    }
    state_.open = true;
    state_.closing = false;
    state_.suppress_background_close_until_release = true;
    state_.requested_user_id = std::move(user_id);
    state_.result = {};
    state_.loaded_once = false;
    request_load();
}

void controller::close() {
    if (state_.open) {
        state_.closing = true;
    }
}

void controller::tick(float dt) {
    if (state_.open && state_.closing) {
        state_.open_anim = tween::retreat(state_.open_anim, dt, 8.0f);
        if (state_.open_anim <= 0.0f) {
            state_.open = false;
            state_.closing = false;
        }
    } else if (state_.open) {
        state_.open_anim = tween::advance(state_.open_anim, dt, 8.0f);
    } else {
        state_.open_anim = 0.0f;
    }
}

void controller::poll() {
    for (const service::event& event : service_.poll()) {
        apply_service_event(event);
    }
}

bool controller::handle_input() {
    if (!state_.open) {
        return false;
    }
    if (state_.suppress_background_close_until_release) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            return true;
        }
        state_.suppress_background_close_until_release = false;
    }
    if ((IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) && !state_.loading) {
        close();
        return true;
    }
    const public_profile_view::command command = public_profile_view::handle_input({
        .loading = state_.loading,
        .loaded_once = state_.loaded_once,
        .relationship_operation_active = state_.relationship_operation_active,
        .open_anim = state_.open_anim,
        .avatar_base_url = auth::load_session_summary().server_url,
        .result = state_.result,
    });
    if (command.type != public_profile_view::command_type::none) {
        apply_view_command(command);
        return true;
    }
    return true;
}

void controller::draw(ui::draw_layer layer) {
    if (!state_.open) {
        return;
    }
    public_profile_view::draw({
        .loading = state_.loading,
        .loaded_once = state_.loaded_once,
        .relationship_operation_active = state_.relationship_operation_active,
        .open_anim = state_.open_anim,
        .avatar_base_url = auth::load_session_summary().server_url,
        .result = state_.result,
    }, layer);
}

bool controller::is_open() const {
    return state_.open;
}

Rectangle controller::bounds() const {
    return public_profile_view::bounds(state_.open_anim);
}

effects controller::consume_effects() {
    effects result = std::move(pending_effects_);
    pending_effects_ = {};
    return result;
}

void controller::request_load() {
    if (state_.loading) {
        return;
    }
    if (service_.request_load(state_.requested_user_id)) {
        state_.loading = true;
    }
}

void controller::start_relationship_action() {
    if (state_.relationship_operation_active ||
        !state_.result.success ||
        !state_.result.profile.has_value()) {
        return;
    }
    state_.relationship_operation_active = service_.start_relationship_action(*state_.result.profile);
}

void controller::apply_view_command(const public_profile_view::command& command) {
    switch (command.type) {
    case public_profile_view::command_type::none:
        break;
    case public_profile_view::command_type::close:
        close();
        break;
    case public_profile_view::command_type::start_relationship_action:
        start_relationship_action();
        break;
    }
}

void controller::apply_service_event(const service::event& event) {
    switch (event.type) {
    case service::event_type::profile_loaded:
        state_.result = event.profile;
        state_.loading = false;
        state_.loaded_once = true;
        if (!state_.result.success) {
            emit_notice({
                .message = state_.result.message,
                .tone = ui::notice_tone::error,
                .seconds = 2.8f,
            });
        }
        break;
    case service::event_type::relationship_completed:
        state_.relationship_result = event.relationship;
        state_.relationship_operation_active = false;
        if (!state_.relationship_result.success) {
            emit_notice({
                .message = state_.relationship_result.message.empty()
                    ? "Failed to update relationship."
                    : state_.relationship_result.message,
                .tone = ui::notice_tone::error,
                .seconds = 2.8f,
            });
        } else {
            emit_notice({
                .message = "Relationship updated.",
                .tone = ui::notice_tone::success,
                .seconds = 1.8f,
            });
            pending_effects_.reload_friends = true;
            state_.loading = false;
            request_load();
        }
        break;
    case service::event_type::service_error:
        state_.loading = false;
        state_.relationship_operation_active = false;
        emit_notice({
            .message = event.message.empty() ? "Public profile operation failed." : event.message,
            .tone = ui::notice_tone::error,
            .seconds = 2.8f,
        });
        break;
    }
}

void controller::emit_notice(notice_request notice) {
    pending_effects_.notices.push_back(std::move(notice));
}

}  // namespace public_profile
