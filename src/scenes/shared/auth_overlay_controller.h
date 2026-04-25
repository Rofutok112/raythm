#pragma once

#include <future>

#include "network/auth_client.h"
#include "song_select/song_select_login_dialog.h"
#include "song_select/song_select_state.h"

namespace auth_overlay {

struct controller {
    std::future<auth::operation_result> restore_future;
    std::future<auth::operation_result> request_future;
    bool restore_active = false;
    bool request_active = false;
};

void refresh_auth_state(song_select::auth_state& auth_state);
void start_restore(controller& controller_state, song_select::login_dialog_state& dialog_state);
void start_request(controller& controller_state,
                   song_select::login_dialog_state& dialog_state,
                   song_select::login_dialog_command command);
void poll_restore(controller& controller_state,
                  song_select::auth_state& auth_state,
                  song_select::login_dialog_state& dialog_state);
void poll_request(controller& controller_state,
                  song_select::auth_state& auth_state,
                  song_select::login_dialog_state& dialog_state);

}  // namespace auth_overlay
