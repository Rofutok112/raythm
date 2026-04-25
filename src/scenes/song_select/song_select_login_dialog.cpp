#include "song_select/song_select_login_dialog.h"

#include <array>

#include "song_select/song_select_layout.h"
#include "theme.h"
#include "tween.h"
#include "ui_draw.h"

namespace {

constexpr float kDialogWidth = 540.0f;
constexpr float kLoginDialogHeight = 462.0f;
constexpr float kSignupDialogHeight = 594.0f;
constexpr float kVerifyDialogHeight = 462.0f;
constexpr float kAccountDialogHeight = 387.0f;
constexpr float kDialogOffsetY = 27.0f;
constexpr float kDialogPaddingX = 27.0f;
constexpr float kTitleHeight = 39.0f;
constexpr float kSubtitleHeight = 27.0f;
constexpr float kHeaderTop = 27.0f;
constexpr float kHeaderGap = 9.0f;
constexpr float kTabTop = 105.0f;
constexpr float kTabHeight = 42.0f;
constexpr float kBodyTop = 168.0f;
constexpr float kRowHeight = 54.0f;
constexpr float kRowGap = 12.0f;
constexpr float kButtonHeight = 54.0f;
constexpr float kButtonGap = 12.0f;
constexpr float kPrimaryButtonWidth = 192.0f;
constexpr float kScreenEdgeMargin = 18.0f;
constexpr float kOpenAnimOffsetY = 27.0f;
constexpr float kFooterMarginBottom = 27.0f;
constexpr float kSignedInLineHeight = 33.0f;
constexpr float kDisplayNameOffsetY = 42.0f;
constexpr float kDisplayNameHeight = 30.0f;
constexpr float kEmailOffsetY = 78.0f;
constexpr float kEmailLineHeight = 24.0f;
constexpr float kVerifyOffsetY = 111.0f;
constexpr float kVerifyLineHeight = 24.0f;
constexpr float kAccountButtonWidth = 138.0f;
constexpr float kStatusOffsetAboveFooter = 42.0f;
constexpr float kStatusLineHeight = 30.0f;
constexpr float kTextInputLabelWidth = 135.0f;
constexpr float kFormMessageGap = 18.0f;
constexpr float kMessageHeight = 27.0f;
constexpr float kLoginButtonOffsetY = 42.0f;
constexpr float kVerifyButtonWidth = 168.0f;

Rectangle dialog_rect_for(const song_select::state& state) {
    const float dialog_height = state.auth.logged_in ? kAccountDialogHeight
        : (state.login_dialog.mode == song_select::login_dialog_mode::signup ? kSignupDialogHeight
           : (state.login_dialog.mode == song_select::login_dialog_mode::verify ? kVerifyDialogHeight
                                                                                : kLoginDialogHeight));
    Rectangle rect = {
        song_select::layout::kLoginButtonRect.x + song_select::layout::kLoginButtonRect.width - kDialogWidth,
        song_select::layout::kLoginButtonRect.y + song_select::layout::kLoginButtonRect.height + kDialogOffsetY,
        kDialogWidth,
        dialog_height
    };
    rect.x = std::clamp(rect.x, kScreenEdgeMargin,
                        song_select::layout::kScreenRect.width - rect.width - kScreenEdgeMargin);
    rect.y = std::clamp(rect.y, kScreenEdgeMargin,
                        song_select::layout::kScreenRect.height - rect.height - kScreenEdgeMargin);
    const float anim_t = tween::ease_out_cubic(state.login_dialog.open_anim);
    rect.y -= (1.0f - anim_t) * kOpenAnimOffsetY;
    return rect;
}

Rectangle dialog_rect_for(const song_select::auth_state& auth_state,
                          const song_select::login_dialog_state& dialog_state,
                          Rectangle anchor_rect,
                          Rectangle screen_rect) {
    const float dialog_height = auth_state.logged_in ? kAccountDialogHeight
        : (dialog_state.mode == song_select::login_dialog_mode::signup ? kSignupDialogHeight
           : (dialog_state.mode == song_select::login_dialog_mode::verify ? kVerifyDialogHeight
                                                                          : kLoginDialogHeight));
    Rectangle rect = {
        anchor_rect.x + anchor_rect.width - kDialogWidth,
        anchor_rect.y + anchor_rect.height + kDialogOffsetY,
        kDialogWidth,
        dialog_height
    };
    rect.x = std::clamp(rect.x, kScreenEdgeMargin, screen_rect.width - rect.width - kScreenEdgeMargin);
    rect.y = std::clamp(rect.y, kScreenEdgeMargin, screen_rect.height - rect.height - kScreenEdgeMargin);
    const float anim_t = tween::ease_out_cubic(dialog_state.open_anim);
    rect.y -= (1.0f - anim_t) * kOpenAnimOffsetY;
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

void deactivate_input(ui::text_input_state& input) {
    input.active = false;
    input.has_selection = false;
    input.mouse_selecting = false;
}

void activate_input(ui::text_input_state& input) {
    input.active = true;
    input.cursor = ui::utf8_codepoint_count(input.value);
    input.selection_anchor = input.cursor;
    input.has_selection = false;
    input.mouse_selecting = false;
}

void deactivate_form_inputs(song_select::login_dialog_state& dialog_state) {
    deactivate_input(dialog_state.display_name_input);
    deactivate_input(dialog_state.email_input);
    deactivate_input(dialog_state.password_input);
    deactivate_input(dialog_state.password_confirmation_input);
    deactivate_input(dialog_state.verification_code_input);
}

void focus_input(song_select::login_dialog_state& dialog_state, ui::text_input_state& input) {
    deactivate_form_inputs(dialog_state);
    activate_input(input);
}

void focus_relative_input(song_select::login_dialog_state& dialog_state, bool signup, int direction) {
    std::array<ui::text_input_state*, 4> inputs = {
        signup ? &dialog_state.display_name_input : &dialog_state.email_input,
        signup ? &dialog_state.email_input : &dialog_state.password_input,
        signup ? &dialog_state.password_input : nullptr,
        signup ? &dialog_state.password_confirmation_input : nullptr,
    };
    const int count = signup ? 4 : 2;
    int active_index = -1;
    for (int i = 0; i < count; ++i) {
        if (inputs[static_cast<size_t>(i)]->active) {
            active_index = i;
            break;
        }
    }

    const int next_index = active_index < 0
        ? 0
        : (active_index + direction + count) % count;
    focus_input(dialog_state, *inputs[static_cast<size_t>(next_index)]);
}

const char* verification_title(auth::verification_purpose purpose) {
    return purpose == auth::verification_purpose::login_verification
        ? "Confirm login"
        : "Verify email";
}

ui::button_state draw_tab(Rectangle rect, const char* label, bool selected, ui::draw_layer layer) {
    const auto& theme = *g_theme;
    const ui::row_state row = ui::enqueue_row(rect,
                                              selected ? theme.row_selected : theme.row,
                                              selected ? theme.row_active : theme.row_hover,
                                              selected ? theme.border_active : theme.border,
                                              layer, 1.5f);
    ui::enqueue_text_in_rect(label, 15, rect, selected ? theme.text : theme.text_secondary,
                             ui::text_align::center, layer);
    return {row.hovered, row.pressed, row.clicked};
}

ui::button_state draw_dialog_button(Rectangle rect, const char* label, int font_size,
                                    ui::draw_layer layer, bool enabled) {
    if (enabled) {
        return ui::enqueue_button(rect, label, font_size, layer, 1.5f);
    }

    const auto& theme = *g_theme;
    ui::enqueue_row(rect, theme.section, theme.section, theme.border_light, layer, 1.5f);
    ui::enqueue_text_in_rect(label, font_size, rect, theme.text_muted, ui::text_align::center, layer);
    return {};
}

}  // namespace

namespace song_select {

void open_login_dialog(login_dialog_state& dialog_state, const auth::session_summary& summary) {
    dialog_state.open = true;
    if (!summary.logged_in) {
        dialog_state.mode = login_dialog_mode::login;
    }
    dialog_state.open_anim = 0.0f;
    dialog_state.status_message.clear();
    dialog_state.status_message_is_error = false;
    dialog_state.email_input.value = summary.email;
    dialog_state.display_name_input.value.clear();
    dialog_state.password_input.value.clear();
    dialog_state.password_confirmation_input.value.clear();
    dialog_state.verification_code_input.value.clear();
    deactivate_form_inputs(dialog_state);
}

Rectangle login_dialog_rect(const auth_state& auth_state, const login_dialog_state& dialog_state,
                            Rectangle anchor_rect, Rectangle screen_rect) {
    return dialog_rect_for(auth_state, dialog_state, anchor_rect, screen_rect);
}

login_dialog_command draw_login_dialog(const auth_state& auth_state, login_dialog_state& dialog_state,
                                       Rectangle anchor_rect, Rectangle screen_rect,
                                       bool request_active, ui::draw_layer layer) {
    if (!dialog_state.open) {
        return login_dialog_command::none;
    }

    const auto& theme = *g_theme;
    const Rectangle dialog_rect = dialog_rect_for(auth_state, dialog_state, anchor_rect, screen_rect);
    const float form_x = dialog_rect.x + kDialogPaddingX;
    const float form_width = dialog_rect.width - kDialogPaddingX * 2.0f;
    const float footer_y = dialog_rect.y + dialog_rect.height - kFooterMarginBottom - kButtonHeight;

    ui::draw_panel(dialog_rect);
    ui::draw_text_in_rect("Account", 24,
                          {dialog_rect.x + kDialogPaddingX, dialog_rect.y + kHeaderTop,
                           dialog_rect.width - kDialogPaddingX * 2.0f, kTitleHeight},
                          theme.text, ui::text_align::left);
    ui::draw_text_in_rect("Connect to raythm-Server", 14,
                          {dialog_rect.x + kDialogPaddingX, dialog_rect.y + kHeaderTop + kTitleHeight + kHeaderGap,
                           dialog_rect.width - kDialogPaddingX * 2.0f, kSubtitleHeight},
                          theme.text_secondary, ui::text_align::left);

    if (auth_state.logged_in) {
        const Rectangle signed_in_rect = {form_x, dialog_rect.y + kBodyTop, form_width, kSignedInLineHeight};
        const Rectangle display_name_rect = {
            form_x, dialog_rect.y + kBodyTop + kDisplayNameOffsetY, form_width, kDisplayNameHeight
        };
        const Rectangle email_rect = {
            form_x, dialog_rect.y + kBodyTop + kEmailOffsetY, form_width, kEmailLineHeight
        };
        const Rectangle verify_rect = {
            form_x, dialog_rect.y + kBodyTop + kVerifyOffsetY, form_width, kVerifyLineHeight
        };
        const Rectangle button_row = {form_x, footer_y, form_width, kButtonHeight};
        const Rectangle logout_rect = {
            button_row.x + button_row.width - kAccountButtonWidth, button_row.y, kAccountButtonWidth, kButtonHeight
        };
        const Rectangle refresh_rect = {
            logout_rect.x - kAccountButtonWidth - kButtonGap, button_row.y, kAccountButtonWidth, kButtonHeight
        };

        ui::draw_text_in_rect("Signed in", 20, signed_in_rect, theme.success, ui::text_align::left);
        ui::draw_text_in_rect(auth_state.display_name.empty() ? auth_state.email.c_str() : auth_state.display_name.c_str(),
                              18,
                              display_name_rect,
                              theme.text_secondary,
                              ui::text_align::left);
        ui::draw_text_in_rect(auth_state.email.c_str(),
                              14,
                              email_rect,
                              theme.text_muted,
                              ui::text_align::left);
        ui::draw_text_in_rect(auth_state.email_verified
                                  ? "Email verified"
                                  : "Verify on the Web to submit online scores.",
                              13,
                              verify_rect,
                              auth_state.email_verified ? theme.success : theme.error,
                              ui::text_align::left);

        if (!dialog_state.status_message.empty()) {
            ui::draw_text_in_rect(dialog_state.status_message.c_str(), 13,
                                  {form_x, footer_y - kStatusOffsetAboveFooter, form_width, kStatusLineHeight},
                                  dialog_state.status_message_is_error ? theme.error : theme.success,
                                  ui::text_align::left);
        }

        if (draw_dialog_button(refresh_rect, "REFRESH", 14, layer, !request_active).clicked) {
            return login_dialog_command::request_restore;
        }
        if (draw_dialog_button(logout_rect, "LOGOUT", 14, layer, !request_active).clicked) {
            return login_dialog_command::request_logout;
        }
        return login_dialog_command::none;
    }

    if (dialog_state.mode == login_dialog_mode::verify) {
        const Rectangle title_rect = {form_x, dialog_rect.y + kBodyTop, form_width, 30.0f};
        const Rectangle email_rect = {form_x, title_rect.y + 34.0f, form_width, 24.0f};
        const Rectangle code_rect = {form_x, email_rect.y + 42.0f, form_width, kRowHeight};
        const Rectangle message_rect = {form_x, code_rect.y + code_rect.height + kFormMessageGap,
                                        form_width, kMessageHeight};
        const Rectangle button_row = {form_x, message_rect.y + kLoginButtonOffsetY, form_width, kButtonHeight};
        const Rectangle verify_rect = {
            button_row.x + button_row.width - kVerifyButtonWidth,
            button_row.y,
            kVerifyButtonWidth,
            kButtonHeight
        };
        const Rectangle resend_rect = {
            verify_rect.x - kVerifyButtonWidth - kButtonGap,
            button_row.y,
            kVerifyButtonWidth,
            kButtonHeight
        };

        ui::draw_text_in_rect(verification_title(dialog_state.verification), 20,
                              title_rect, theme.text, ui::text_align::left);
        ui::draw_text_in_rect(dialog_state.verification_email.c_str(), 14,
                              email_rect, theme.text_muted, ui::text_align::left);
        const ui::text_input_result code_result = ui::draw_text_input(
            code_rect, dialog_state.verification_code_input, "Code", "6 digit code",
            nullptr, layer, 15, 12, printable_filter, kTextInputLabelWidth);

        if (!dialog_state.status_message.empty()) {
            ui::draw_text_in_rect(dialog_state.status_message.c_str(), 16, message_rect,
                                  dialog_state.status_message_is_error ? theme.error : theme.success,
                                  ui::text_align::left);
        }

        if (draw_dialog_button(resend_rect, "RESEND", 15, layer, !request_active).clicked) {
            return login_dialog_command::request_resend_code;
        }
        if ((draw_dialog_button(verify_rect, "VERIFY", 15, layer, !request_active).clicked ||
             code_result.submitted) &&
            !request_active) {
            return login_dialog_command::request_verify;
        }
        return login_dialog_command::none;
    }

    const bool signup = dialog_state.mode == login_dialog_mode::signup;
    const Rectangle tab_row = {form_x, dialog_rect.y + kTabTop, form_width, kTabHeight};
    const float tab_width = (tab_row.width - kButtonGap) * 0.5f;
    const Rectangle login_tab = {tab_row.x, tab_row.y, tab_width, tab_row.height};
    const Rectangle signup_tab = {tab_row.x + tab_width + kButtonGap, tab_row.y, tab_width, tab_row.height};

    if (draw_tab(login_tab, "LOGIN", !signup, layer).clicked && !request_active) {
        dialog_state.mode = login_dialog_mode::login;
        dialog_state.status_message.clear();
        deactivate_form_inputs(dialog_state);
    }
    if (draw_tab(signup_tab, "SIGN UP", signup, layer).clicked && !request_active) {
        dialog_state.mode = login_dialog_mode::signup;
        dialog_state.status_message.clear();
        deactivate_form_inputs(dialog_state);
    }

    int row = 0;
    bool submitted = false;
    if (signup) {
        const ui::text_input_result display_name_result = ui::draw_text_input(
            make_row(dialog_rect, row++), dialog_state.display_name_input, "Name", "Display name",
            nullptr, layer, 15, 32, printable_filter, kTextInputLabelWidth);
        submitted = submitted || display_name_result.submitted;
    }

    const ui::text_input_result email_result = ui::draw_text_input(
        make_row(dialog_rect, row++), dialog_state.email_input, "Email", "name@example.com",
        nullptr, layer, 15, 64, printable_filter, kTextInputLabelWidth);
    submitted = submitted || email_result.submitted;

    const ui::text_input_result password_result = ui::draw_text_input(
        make_row(dialog_rect, row++), dialog_state.password_input, "Pass", "Password",
        nullptr, layer, 15, 64, printable_filter, kTextInputLabelWidth, true);
    submitted = submitted || password_result.submitted;

    if (signup) {
        const ui::text_input_result confirm_result = ui::draw_text_input(
            make_row(dialog_rect, row++), dialog_state.password_confirmation_input, "Confirm", "Repeat password",
            nullptr, layer, 15, 64, printable_filter, kTextInputLabelWidth, true);
        submitted = submitted || confirm_result.submitted;
    }

    if (IsKeyPressed(KEY_TAB) && !request_active) {
        focus_relative_input(dialog_state, signup,
                             IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? -1 : 1);
    }

    const float action_top =
        dialog_rect.y + kBodyTop + static_cast<float>(row) * (kRowHeight + kRowGap) + kFormMessageGap;
    const Rectangle message_rect = {form_x, action_top, form_width, kMessageHeight};
    const Rectangle login_button_row = {form_x, action_top + kLoginButtonOffsetY, form_width, kButtonHeight};
    const Rectangle primary_rect = ui::place(login_button_row, kPrimaryButtonWidth, kButtonHeight,
                                             ui::anchor::center, ui::anchor::center);

    if (!dialog_state.status_message.empty()) {
        ui::draw_text_in_rect(dialog_state.status_message.c_str(), 16, message_rect,
                              dialog_state.status_message_is_error ? theme.error : theme.success,
                              ui::text_align::left);
    }

    const char* primary_label = signup ? "SIGN UP" : "LOGIN";
    if ((draw_dialog_button(primary_rect, primary_label, 16, layer, !request_active).clicked || submitted) &&
        !request_active) {
        return signup ? login_dialog_command::request_register : login_dialog_command::request_login;
    }

    return login_dialog_command::none;
}

void open_login_dialog(state& state, const auth::session_summary& summary) {
    open_login_dialog(state.login_dialog, summary);
}

Rectangle login_dialog_rect(const state& state) {
    return login_dialog_rect(state.auth, state.login_dialog,
                             song_select::layout::kLoginButtonRect,
                             song_select::layout::kScreenRect);
}

login_dialog_command draw_login_dialog(state& state, bool request_active) {
    return draw_login_dialog(state.auth, state.login_dialog,
                             song_select::layout::kLoginButtonRect,
                             song_select::layout::kScreenRect,
                             request_active,
                             song_select::layout::kModalLayer);
}

}  // namespace song_select
