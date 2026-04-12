#include "song_select/song_select_login_dialog.h"

#include "song_select/song_select_layout.h"
#include "theme.h"
#include "ui_draw.h"

namespace {

constexpr ui::draw_layer kModalLayer = song_select::layout::kModalLayer;
constexpr float kDialogWidth = 360.0f;
constexpr float kLoginDialogHeight = 308.0f;
constexpr float kAccountDialogHeight = 258.0f;
constexpr float kDialogOffsetY = 18.0f;
constexpr float kDialogPaddingX = 18.0f;
constexpr float kTitleHeight = 26.0f;
constexpr float kSubtitleHeight = 18.0f;
constexpr float kHeaderTop = 18.0f;
constexpr float kHeaderGap = 6.0f;
constexpr float kBodyTop = 86.0f;
constexpr float kRowHeight = 36.0f;
constexpr float kRowGap = 8.0f;
constexpr float kButtonHeight = 36.0f;
constexpr float kButtonGap = 8.0f;
constexpr float kPrimaryButtonWidth = 128.0f;
constexpr float kSecondaryButtonWidth = 164.0f;
constexpr float kHelperTextHeight = 18.0f;

float ease_out_cubic(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv * inv;
}

Rectangle dialog_rect_for(const song_select::state& state) {
    const float dialog_height = state.auth.logged_in
        ? kAccountDialogHeight
        : kLoginDialogHeight;
    Rectangle rect = {
        song_select::layout::kLoginButtonRect.x + song_select::layout::kLoginButtonRect.width - kDialogWidth,
        song_select::layout::kLoginButtonRect.y + song_select::layout::kLoginButtonRect.height + kDialogOffsetY,
        kDialogWidth,
        dialog_height
    };
    rect.x = std::clamp(rect.x, 12.0f, song_select::layout::kScreenRect.width - rect.width - 12.0f);
    rect.y = std::clamp(rect.y, 12.0f, song_select::layout::kScreenRect.height - rect.height - 12.0f);
    const float anim_t = ease_out_cubic(state.login_dialog.open_anim);
    rect.y -= (1.0f - anim_t) * 18.0f;
    return rect;
}

Rectangle make_row(const Rectangle& dialog_rect, int index) {
    return {
        dialog_rect.x + kDialogPaddingX,
        dialog_rect.y + kBodyTop + static_cast<float>(index) * (kRowHeight + kRowGap),
        dialog_rect.width - kDialogPaddingX * 2.0f,
        kRowHeight
    };
}

bool printable_filter(int codepoint, const std::string&) {
    return codepoint >= 32 && codepoint != 127;
}

}  // namespace

namespace song_select {

void open_login_dialog(state& state, const auth::session_summary& summary) {
    state.login_dialog.open = true;
    state.login_dialog.open_anim = 0.0f;
    state.login_dialog.status_message.clear();
    state.login_dialog.status_message_is_error = false;
    state.login_dialog.email_input.value = summary.email;
    state.login_dialog.password_input.value.clear();
}

Rectangle login_dialog_rect(const state& state) {
    return dialog_rect_for(state);
}

login_dialog_command draw_login_dialog(state& state, bool request_active) {
    if (!state.login_dialog.open) {
        return login_dialog_command::none;
    }

    const auto& theme = *g_theme;
    const Rectangle dialog_rect = dialog_rect_for(state);
    const float form_x = dialog_rect.x + kDialogPaddingX;
    const float form_width = dialog_rect.width - kDialogPaddingX * 2.0f;
    const float footer_y = dialog_rect.y + dialog_rect.height - 18.0f - kButtonHeight;

    ui::draw_panel(dialog_rect);
    ui::draw_text_in_rect("Account", 24,
                          {dialog_rect.x + kDialogPaddingX, dialog_rect.y + kHeaderTop,
                           dialog_rect.width - kDialogPaddingX * 2.0f, kTitleHeight},
                          theme.text, ui::text_align::left);
    ui::draw_text_in_rect("Connect to raythm-Server", 14,
                          {dialog_rect.x + kDialogPaddingX, dialog_rect.y + kHeaderTop + kTitleHeight + kHeaderGap,
                           dialog_rect.width - kDialogPaddingX * 2.0f, kSubtitleHeight},
                          theme.text_secondary, ui::text_align::left);

    if (state.auth.logged_in) {
        const Rectangle signed_in_rect = {form_x, dialog_rect.y + kBodyTop, form_width, 22.0f};
        const Rectangle display_name_rect = {form_x, dialog_rect.y + kBodyTop + 28.0f, form_width, 20.0f};
        const Rectangle email_rect = {form_x, dialog_rect.y + kBodyTop + 52.0f, form_width, 16.0f};
        const Rectangle verify_rect = {form_x, dialog_rect.y + kBodyTop + 74.0f, form_width, 16.0f};
        const Rectangle button_row = {form_x, footer_y, form_width, kButtonHeight};
        constexpr float kButtonWidth = 92.0f;
        const Rectangle logout_rect = {button_row.x + button_row.width - kButtonWidth, button_row.y, kButtonWidth, kButtonHeight};
        const Rectangle refresh_rect = {logout_rect.x - kButtonWidth - kButtonGap, button_row.y, kButtonWidth, kButtonHeight};

        ui::draw_text_in_rect("Signed in", 20, signed_in_rect, theme.success, ui::text_align::left);
        ui::draw_text_in_rect(state.auth.display_name.empty() ? state.auth.email.c_str() : state.auth.display_name.c_str(),
                              18,
                              display_name_rect,
                              theme.text_secondary,
                              ui::text_align::left);
        ui::draw_text_in_rect(state.auth.email.c_str(),
                              14,
                              email_rect,
                              theme.text_muted,
                              ui::text_align::left);
        ui::draw_text_in_rect(state.auth.email_verified
                                  ? "Email verified"
                                  : "Verify on the Web to submit online scores.",
                              13,
                              verify_rect,
                              state.auth.email_verified ? theme.success : theme.error,
                              ui::text_align::left);

        if (!state.login_dialog.status_message.empty()) {
            ui::draw_text_in_rect(state.login_dialog.status_message.c_str(), 13,
                                  {form_x, footer_y - 28.0f, form_width, 20.0f},
                                  state.login_dialog.status_message_is_error ? theme.error : theme.success,
                                  ui::text_align::left);
        }

        if (ui::enqueue_button(refresh_rect, "REFRESH", 14, kModalLayer, 1.5f).clicked && !request_active) {
            return login_dialog_command::request_restore;
        }
        if (ui::enqueue_button(logout_rect, "LOGOUT", 14, kModalLayer, 1.5f).clicked && !request_active) {
            return login_dialog_command::request_logout;
        }
        return login_dialog_command::none;
    }

    int row = 0;
    const ui::text_input_result email_result = ui::draw_text_input(
        make_row(dialog_rect, row++), state.login_dialog.email_input, "Email", "name@example.com",
        nullptr, kModalLayer, 15, 64, printable_filter, 90.0f);

    const ui::text_input_result password_result = ui::draw_text_input(
        make_row(dialog_rect, row++), state.login_dialog.password_input, "Pass", "At least 8 characters",
        nullptr, kModalLayer, 15, 64, printable_filter, 90.0f);

    const float action_top = dialog_rect.y + 166.0f;
    const Rectangle message_rect = {form_x, action_top, form_width, 18.0f};
    const Rectangle login_button_row = {form_x, action_top + 28.0f, form_width, kButtonHeight};
    const Rectangle helper_rect = {form_x, action_top + 72.0f, form_width, kHelperTextHeight};
    const Rectangle web_button_row = {form_x, action_top + 94.0f, form_width, kButtonHeight};
    const Rectangle primary_rect = ui::place(login_button_row, kPrimaryButtonWidth, kButtonHeight,
                                             ui::anchor::center, ui::anchor::center);
    const Rectangle web_button_rect = ui::place(web_button_row, kSecondaryButtonWidth, kButtonHeight,
                                                ui::anchor::center, ui::anchor::center);

    const bool submitted = email_result.submitted || password_result.submitted;

    if (!state.login_dialog.status_message.empty()) {
        ui::draw_text_in_rect(state.login_dialog.status_message.c_str(), 16, message_rect,
                              state.login_dialog.status_message_is_error ? theme.error : theme.success,
                              ui::text_align::left);
    }

    ui::enqueue_text_in_rect("New to raythm? Register on the Web.", 14, helper_rect,
                             theme.text_muted, ui::text_align::center, kModalLayer);
    if (ui::enqueue_button(web_button_rect, "Create", 15, kModalLayer, 1.5f).clicked && !request_active) {
        return login_dialog_command::open_register_web;
    }
    if ((ui::enqueue_button(primary_rect, "LOGIN", 16, kModalLayer, 1.5f).clicked || submitted) && !request_active) {
        return login_dialog_command::request_login;
    }

    return login_dialog_command::none;
}

}  // namespace song_select
