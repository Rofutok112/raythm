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
    request_verify,
    request_resend_code,
    request_profile,
    request_logout,
    request_delete_account,
};

enum class login_dialog_state_action {
    none,
    show_login,
    show_signup,
    focus_previous,
    focus_next,
};

struct login_dialog_result {
    login_dialog_command command = login_dialog_command::none;
    login_dialog_state_action state_action = login_dialog_state_action::none;
};

struct login_dialog_layout {
    Rectangle anchor_rect{};
    Rectangle screen_rect{};
    Rectangle dialog_rect{};
    ui::draw_layer layer = ui::draw_layer::modal;
};

void open_login_dialog(login_dialog_state& dialog_state, const auth::session_summary& summary);
login_dialog_layout make_login_dialog_layout(const auth_state& auth_state,
                                             const login_dialog_state& dialog_state,
                                             Rectangle anchor_rect,
                                             Rectangle screen_rect,
                                             ui::draw_layer layer = ui::draw_layer::modal);
Rectangle login_dialog_rect(const auth_state& auth_state, const login_dialog_state& dialog_state,
                            Rectangle anchor_rect, Rectangle screen_rect);
login_dialog_result draw_login_dialog_result(const auth_state& auth_state,
                                             login_dialog_state& dialog_state,
                                             const login_dialog_layout& layout,
                                             bool request_active);
login_dialog_command draw_login_dialog(const auth_state& auth_state,
                                       login_dialog_state& dialog_state,
                                       const login_dialog_layout& layout,
                                       bool request_active);
void apply_login_dialog_result(login_dialog_state& dialog_state, const login_dialog_result& result);
login_dialog_command draw_login_dialog(const auth_state& auth_state, login_dialog_state& dialog_state,
                                       Rectangle anchor_rect, Rectangle screen_rect,
                                       bool request_active,
                                       ui::draw_layer layer = ui::draw_layer::modal);

void open_login_dialog(state& state, const auth::session_summary& summary);
Rectangle login_dialog_rect(const state& state);
login_dialog_command draw_login_dialog(state& state, bool request_active);

}  // namespace song_select
