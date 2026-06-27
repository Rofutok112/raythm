#include "title/title_friends_controller.h"

#include <string>
#include <utility>

#include "network/auth_client.h"
#include "title/title_friends_effects.h"
#include "title/title_friends_reducer.h"
#include "tween.h"

void title_friends_controller::reset() {
    state_ = {};
    service_.reset();
}

void title_friends_controller::open() {
    if (state_.open && !state_.closing) {
        return;
    }
    state_.open = true;
    state_.closing = false;
    state_.avatar_base_url = auth::load_session_summary().server_url;
    state_.suppress_background_close_until_release = true;
    request_reload();
}

void title_friends_controller::close() {
    if (state_.open) {
        state_.closing = true;
    }
}

void title_friends_controller::tick(float dt) {
    service_.tick(dt);

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

void title_friends_controller::poll() {
    for (const title_friends_service::event& event : service_.poll()) {
        apply_service_event(event);
    }
}

bool title_friends_controller::handle_input() {
    if (!state_.open) {
        return false;
    }
    if (state_.suppress_background_close_until_release) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            return true;
        }
        state_.suppress_background_close_until_release = false;
    }
    if ((IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) && !state_.operation_active) {
        close();
        return true;
    }
    const title_friends_view::model view_model{
        .open_anim = state_.open_anim,
        .loading = state_.loading,
        .loaded_once = state_.loaded_once,
        .operation_active = state_.operation_active,
        .avatar_base_url = state_.avatar_base_url,
        .selected_tab = state_.selected_tab,
        .social = state_.social,
    };
    const title_friends_view::command command = title_friends_view::handle_input(view_model, ui::draw_layer::modal);
    if (command.type != title_friends_view::command_type::none) {
        apply_view_command(command);
        return true;
    }
    return true;
}

void title_friends_controller::draw(ui::draw_layer layer, bool draw_backdrop) {
    if (!state_.open) {
        return;
    }
    title_friends_view::draw({
        .open_anim = state_.open_anim,
        .loading = state_.loading,
        .loaded_once = state_.loaded_once,
        .operation_active = state_.operation_active,
        .avatar_base_url = state_.avatar_base_url,
        .selected_tab = state_.selected_tab,
        .social = state_.social,
    }, layer, draw_backdrop);
}

bool title_friends_controller::is_open() const {
    return state_.open;
}

int title_friends_controller::unread_badge_count() const {
    return title_friends_state::unread_badge_count(state_.social);
}

title_friends_effects::feature_effects title_friends_controller::consume_effects() {
    title_friends_effects::feature_effects effects = std::move(state_.pending_effects);
    state_.pending_effects = {};
    return effects;
}

void title_friends_controller::request_reload() {
    if (state_.loading) {
        return;
    }
    if (service_.request_reload()) {
        state_.loading = true;
    }
}

void title_friends_controller::start_accept_request(std::string request_id) {
    if (state_.operation_active) {
        return;
    }
    state_.operation_active = service_.accept_request(std::move(request_id));
}

void title_friends_controller::start_decline_request(std::string request_id) {
    if (state_.operation_active) {
        return;
    }
    state_.operation_active = service_.decline_request(std::move(request_id));
}

void title_friends_controller::start_remove_friend(std::string user_id) {
    if (state_.operation_active || user_id.empty()) {
        return;
    }
    state_.operation_active = service_.remove_friend(std::move(user_id));
}

void title_friends_controller::start_block_user(std::string user_id) {
    if (state_.operation_active || user_id.empty()) {
        return;
    }
    state_.operation_active = service_.block_user(std::move(user_id));
}

void title_friends_controller::start_accept_invite(std::string invite_id) {
    if (state_.operation_active) {
        return;
    }
    state_.operation_active = service_.accept_invite(std::move(invite_id));
}

void title_friends_controller::start_read_invite(std::string invite_id) {
    if (state_.operation_active) {
        return;
    }
    state_.operation_active = service_.read_invite(std::move(invite_id));
}

void title_friends_controller::start_decline_invite(std::string invite_id) {
    if (state_.operation_active) {
        return;
    }
    state_.operation_active = service_.decline_invite(std::move(invite_id));
}

void title_friends_controller::apply_view_command(const title_friends_view::command& command) {
    switch (command.type) {
    case title_friends_view::command_type::none:
        break;
    case title_friends_view::command_type::close:
        close();
        break;
    case title_friends_view::command_type::refresh:
        request_reload();
        break;
    case title_friends_view::command_type::select_tab:
        state_.selected_tab = command.selected_tab;
        break;
    case title_friends_view::command_type::open_profile:
        state_.pending_effects.profile_user_id = command.id;
        break;
    case title_friends_view::command_type::accept_request:
        start_accept_request(command.id);
        break;
    case title_friends_view::command_type::decline_request:
        start_decline_request(command.id);
        break;
    case title_friends_view::command_type::remove_friend:
        start_remove_friend(command.id);
        break;
    case title_friends_view::command_type::block_user:
        start_block_user(command.id);
        break;
    case title_friends_view::command_type::accept_invite:
        start_accept_invite(command.id);
        break;
    case title_friends_view::command_type::decline_invite:
        start_decline_invite(command.id);
        break;
    case title_friends_view::command_type::mark_invite_read:
        start_read_invite(command.id);
        break;
    }
}

void title_friends_controller::emit_notice(title_friends_effects::notice_request notice) {
    state_.pending_effects.notices.push_back(std::move(notice));
}

void title_friends_controller::apply_operation_result(const friend_client::operation_result& result) {
    if (!result.success) {
        emit_notice({
            .message = result.message.empty() ? "Friends operation failed." : result.message,
            .tone = ui::notice_tone::error,
            .seconds = 2.8f,
        });
        return;
    }
    emit_notice({
        .message = "Friends updated.",
        .tone = ui::notice_tone::success,
        .seconds = 1.8f,
    });
    state_.loading = false;
    request_reload();
}

void title_friends_controller::apply_service_event(const title_friends_service::event& event) {
    switch (event.type) {
    case title_friends_service::event_type::friend_listing_loaded: {
        const title_friends_reducer::apply_result apply_result =
            title_friends_reducer::apply_friend_listing(state_.social, event.friends);
        if (!apply_result.applied) {
            state_.message = apply_result.message;
            emit_notice({
                .message = state_.message,
                .tone = ui::notice_tone::error,
                .seconds = 2.8f,
            });
        }
        break;
    }
    case title_friends_service::event_type::request_listing_loaded: {
        const title_friends_reducer::apply_result apply_result =
            title_friends_reducer::apply_request_listing(state_.social, event.requests);
        if (!apply_result.applied) {
            state_.message = apply_result.message;
            emit_notice({
                .message = state_.message,
                .tone = ui::notice_tone::error,
                .seconds = 2.8f,
            });
        }
        break;
    }
    case title_friends_service::event_type::invite_listing_loaded: {
        const title_friends_reducer::apply_result apply_result =
            title_friends_reducer::apply_invite_listing(state_.social, event.invites);
        if (!apply_result.applied) {
            state_.message = apply_result.message;
            emit_notice({
                .message = state_.message,
                .tone = ui::notice_tone::error,
                .seconds = 2.8f,
            });
        }
        state_.loading = false;
        state_.loaded_once = true;
        break;
    }
    case title_friends_service::event_type::operation_completed:
        state_.operation_active = false;
        apply_operation_result(event.operation);
        break;
    case title_friends_service::event_type::invite_operation_completed:
        state_.operation_active = false;
        if (event.invite_operation.success &&
            !event.invite_operation.join_room_id.empty() &&
            !event.invite_operation.join_invite_id.empty()) {
            state_.pending_effects.room_join = title_friends_effects::room_join_request{
                .room_id = event.invite_operation.join_room_id,
                .invite_id = event.invite_operation.join_invite_id,
            };
            close();
        }
        apply_operation_result(event.invite_operation);
        break;
    case title_friends_service::event_type::social_event:
        apply_social_event(event.social);
        break;
    case title_friends_service::event_type::service_error:
        state_.loading = false;
        state_.operation_active = false;
        state_.message = event.message.empty() ? "Friends operation failed." : event.message;
        emit_notice({
            .message = state_.message,
            .tone = ui::notice_tone::error,
            .seconds = 2.8f,
        });
        break;
    }
}

void title_friends_controller::apply_social_event(const friend_client::social_realtime_event& event) {
    const title_friends_state::event_apply_result apply_result =
        title_friends_state::apply_social_event(state_.social, event);
    if (event.invite.has_value()) {
        const friend_client::room_invite& invite = *event.invite;
        if (apply_result.invite_inserted) {
            const std::string sender = invite.sender.display_name.empty() ? "Friend" : invite.sender.display_name;
            const std::string room = invite.room_name.empty() ? "a room" : invite.room_name;
            emit_notice({
                .message = sender + " invited you to " + room + ".",
                .tone = ui::notice_tone::info,
                .seconds = 3.2f,
            });
        }
    }

    if (event.type == "error" && !event.message.empty()) {
        emit_notice({
            .message = event.message,
            .tone = ui::notice_tone::error,
            .seconds = 2.8f,
        });
    }
}
