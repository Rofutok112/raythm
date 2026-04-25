#pragma once

#include "network/auth_client.h"
#include "song_select/song_select_state.h"

namespace song_select {

enum class login_dialog_command {
    none,
    close,
    request_restore,
    request_login,
    request_register,
    request_logout,
};

void open_login_dialog(login_dialog_state& dialog_state, const auth::session_summary& summary);
Rectangle login_dialog_rect(const auth_state& auth_state, const login_dialog_state& dialog_state,
                            Rectangle anchor_rect, Rectangle screen_rect);
login_dialog_command draw_login_dialog(const auth_state& auth_state, login_dialog_state& dialog_state,
                                       Rectangle anchor_rect, Rectangle screen_rect,
                                       bool request_active,
                                       ui::draw_layer layer = ui::draw_layer::modal);

void open_login_dialog(state& state, const auth::session_summary& summary);
Rectangle login_dialog_rect(const state& state);
login_dialog_command draw_login_dialog(state& state, bool request_active);

}  // namespace song_select
