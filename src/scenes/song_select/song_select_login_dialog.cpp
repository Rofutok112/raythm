#include "song_select/song_select_login_dialog.h"

#include <array>
#include <cstddef>

#include "song_select/song_select_layout.h"
#include "theme.h"
#include "tween.h"
#include "ui_draw.h"
#include "ui_layout.h"
#include "ui_scroll.h"
#include "ui_hit.h"

namespace {

constexpr float kDialogWidth = 540.0f;
constexpr float kLoginDialogHeight = 360.0f;
constexpr float kSignupDialogHeight = 510.0f;
constexpr float kVerifyDialogHeight = 396.0f;
constexpr float kAccountDialogHeight = 432.0f;
constexpr float kDialogOffsetY = 27.0f;
constexpr float kDialogPaddingX = 27.0f;
constexpr float kTitleHeight = 39.0f;
constexpr float kSubtitleHeight = 27.0f;
constexpr float kHeaderTop = 24.0f;
constexpr float kHeaderGap = 6.0f;
constexpr float kTabTop = 96.0f;
constexpr float kTabHeight = 42.0f;
constexpr float kBodyTop = 150.0f;
constexpr float kRowHeight = 54.0f;
constexpr float kRowGap = 12.0f;
constexpr float kButtonHeight = 54.0f;
constexpr float kButtonGap = 12.0f;
constexpr float kPrimaryButtonWidth = 192.0f;
constexpr float kScreenEdgeMargin = 18.0f;
constexpr float kOpenAnimOffsetY = 27.0f;
constexpr float kSignedInLineHeight = 33.0f;
constexpr float kDisplayNameOffsetY = 42.0f;
constexpr float kDisplayNameHeight = 30.0f;
constexpr float kEmailOffsetY = 78.0f;
constexpr float kEmailLineHeight = 24.0f;
constexpr float kVerifyOffsetY = 111.0f;
constexpr float kVerifyLineHeight = 24.0f;
constexpr float kProfileButtonOffsetY = 147.0f;
constexpr float kAccountButtonWidth = 138.0f;
constexpr float kTextInputLabelWidth = 135.0f;
constexpr float kVerifyButtonWidth = 168.0f;

struct login_dialog_frame_layout {
    Rectangle title;
    Rectangle subtitle;
    Rectangle body;
};

struct account_mode_layout {
    Rectangle signed_in;
    Rectangle display_name;
    Rectangle email;
    Rectangle verification;
    Rectangle profile;
    Rectangle refresh;
    Rectangle logout;
};

struct verify_mode_layout {
    Rectangle title;
    Rectangle email;
    Rectangle code;
    Rectangle resend;
    Rectangle verify;
};

struct auth_mode_layout {
    std::array<Rectangle, 2> tabs{};
};

struct auth_tab_descriptor {
    song_select::login_dialog_mode mode = song_select::login_dialog_mode::login;
    song_select::login_dialog_state_action state_action = song_select::login_dialog_state_action::none;
    const char* label = "";
};

struct account_action_button {
    song_select::login_dialog_command command = song_select::login_dialog_command::none;
    const char* label = "";
    int font_size = 14;
    Rectangle rect{};
};

struct auth_tab_button {
    auth_tab_descriptor tab{};
    Rectangle rect{};
    bool selected = false;
};

struct verify_action_button {
    song_select::login_dialog_command command = song_select::login_dialog_command::none;
    const char* label = "";
    Rectangle rect{};
};

struct primary_form_action_button {
    song_select::login_dialog_command command = song_select::login_dialog_command::none;
    const char* label = "";
    Rectangle rect{};
};

constexpr std::array<auth_tab_descriptor, 2> kAuthTabs = {{
    {song_select::login_dialog_mode::login, song_select::login_dialog_state_action::show_login, "LOGIN"},
    {song_select::login_dialog_mode::signup, song_select::login_dialog_state_action::show_signup, "SIGN UP"},
}};

login_dialog_frame_layout make_frame_layout(Rectangle dialog_rect) {
    const Rectangle body = {
        dialog_rect.x + kDialogPaddingX,
        dialog_rect.y + kBodyTop,
        dialog_rect.width - kDialogPaddingX * 2.0f,
        dialog_rect.height - kBodyTop,
    };
    return {
        {body.x, dialog_rect.y + kHeaderTop, body.width, kTitleHeight},
        {body.x, dialog_rect.y + kHeaderTop + kTitleHeight + kHeaderGap, body.width, kSubtitleHeight},
        body,
    };
}

float centered_footer_button_y(const Rectangle& dialog_rect, float content_bottom) {
    return content_bottom + (dialog_rect.y + dialog_rect.height - content_bottom - kButtonHeight) * 0.5f;
}

Rectangle make_row(const Rectangle& dialog_rect, int index) {
    const login_dialog_frame_layout frame = make_frame_layout(dialog_rect);
    return ui::vertical_list_row_rect(frame.body, index, kRowHeight, kRowGap, 0.0f);
}

account_mode_layout make_account_mode_layout(Rectangle dialog_rect) {
    const login_dialog_frame_layout frame = make_frame_layout(dialog_rect);
    const Rectangle profile = {
        frame.body.x,
        dialog_rect.y + kBodyTop + kProfileButtonOffsetY,
        frame.body.width,
        kButtonHeight,
    };
    const Rectangle button_row = {
        frame.body.x,
        centered_footer_button_y(dialog_rect, profile.y + profile.height),
        frame.body.width,
        kButtonHeight,
    };
    const Rectangle logout = {
        button_row.x + button_row.width - kAccountButtonWidth,
        button_row.y,
        kAccountButtonWidth,
        kButtonHeight,
    };
    return {
        {frame.body.x, dialog_rect.y + kBodyTop, frame.body.width, kSignedInLineHeight},
        {frame.body.x, dialog_rect.y + kBodyTop + kDisplayNameOffsetY, frame.body.width, kDisplayNameHeight},
        {frame.body.x, dialog_rect.y + kBodyTop + kEmailOffsetY, frame.body.width, kEmailLineHeight},
        {frame.body.x, dialog_rect.y + kBodyTop + kVerifyOffsetY, frame.body.width, kVerifyLineHeight},
        profile,
        {logout.x - kAccountButtonWidth - kButtonGap, button_row.y, kAccountButtonWidth, kButtonHeight},
        logout,
    };
}

std::array<account_action_button, 3> account_action_buttons_for(const account_mode_layout& layout) {
    return {{
        {song_select::login_dialog_command::request_profile, "PROFILE", 16, layout.profile},
        {song_select::login_dialog_command::request_restore, "REFRESH", 14, layout.refresh},
        {song_select::login_dialog_command::request_logout, "LOGOUT", 14, layout.logout},
    }};
}

std::array<auth_tab_button, kAuthTabs.size()> auth_tab_buttons_for(const auth_mode_layout& layout,
                                                                   song_select::login_dialog_mode mode) {
    std::array<auth_tab_button, kAuthTabs.size()> buttons{};
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        buttons[i] = {
            .tab = kAuthTabs[i],
            .rect = layout.tabs[i],
            .selected = mode == kAuthTabs[i].mode,
        };
    }
    return buttons;
}

std::array<verify_action_button, 2> verify_action_buttons_for(const verify_mode_layout& layout) {
    return {{
        {song_select::login_dialog_command::request_resend_code, "RESEND", layout.resend},
        {song_select::login_dialog_command::request_verify, "VERIFY", layout.verify},
    }};
}

verify_mode_layout make_verify_mode_layout(Rectangle dialog_rect) {
    const login_dialog_frame_layout frame = make_frame_layout(dialog_rect);
    const Rectangle title = {frame.body.x, dialog_rect.y + kBodyTop, frame.body.width, 30.0f};
    const Rectangle email = {frame.body.x, title.y + 34.0f, frame.body.width, 24.0f};
    const Rectangle code = {frame.body.x, email.y + 42.0f, frame.body.width, kRowHeight};
    const Rectangle button_row = {
        frame.body.x,
        centered_footer_button_y(dialog_rect, code.y + code.height),
        frame.body.width,
        kButtonHeight,
    };
    const Rectangle verify = {
        button_row.x + button_row.width - kVerifyButtonWidth,
        button_row.y,
        kVerifyButtonWidth,
        kButtonHeight,
    };
    return {
        title,
        email,
        code,
        {verify.x - kVerifyButtonWidth - kButtonGap, button_row.y, kVerifyButtonWidth, kButtonHeight},
        verify,
    };
}

auth_mode_layout make_auth_mode_layout(Rectangle dialog_rect) {
    const login_dialog_frame_layout frame = make_frame_layout(dialog_rect);
    const Rectangle tab_row = {frame.body.x, dialog_rect.y + kTabTop, frame.body.width, kTabHeight};
    const float tab_width = (tab_row.width - kButtonGap) * 0.5f;
    auth_mode_layout layout{};
    ui::hstack(tab_row, tab_width, kButtonGap, layout.tabs);
    return layout;
}

Rectangle make_primary_form_button_rect(Rectangle dialog_rect, Rectangle last_input_rect) {
    const login_dialog_frame_layout frame = make_frame_layout(dialog_rect);
    const Rectangle button_row = {
        frame.body.x,
        centered_footer_button_y(dialog_rect, last_input_rect.y + last_input_rect.height),
        frame.body.width,
        kButtonHeight,
    };
    return ui::place(button_row, kPrimaryButtonWidth, kButtonHeight,
                     ui::anchor::center, ui::anchor::center);
}

primary_form_action_button primary_form_action_button_for(Rectangle dialog_rect,
                                                          Rectangle last_input_rect,
                                                          bool signup) {
    return {
        signup ? song_select::login_dialog_command::request_register
               : song_select::login_dialog_command::request_login,
        signup ? "SIGN UP" : "LOGIN",
        make_primary_form_button_rect(dialog_rect, last_input_rect),
    };
}

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

bool printable_filter(int codepoint, const std::string&) {
    return codepoint >= 32 && codepoint != 127;
}

ui::text_input_options login_text_input_options(ui::draw_layer layer, std::size_t max_length, bool obscure_value = false) {
    return {
        .layer = layer,
        .font_size = 15,
        .max_length = max_length,
        .filter = printable_filter,
        .label_width = kTextInputLabelWidth,
        .obscure_value = obscure_value,
    };
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
    return ui::queued_tab_button(rect, label, {
        .layer = layer,
        .font_size = 15,
        .selected = selected,
        .style = ui::tab_button_style::raised,
        .border_width = 1.5f,
        .selected_border_width = 1.5f,
    });
}

ui::button_state draw_tab(const auth_tab_button& button, ui::draw_layer layer) {
    return draw_tab(button.rect, button.tab.label, button.selected, layer);
}

ui::button_state draw_dialog_button(Rectangle rect, const char* label, int font_size,
                                    ui::draw_layer layer, bool enabled) {
    return ui::queued_action_button(rect, label, {
        .layer = layer,
        .font_size = font_size,
        .border_width = 1.5f,
        .enabled = enabled,
    });
}

ui::button_state draw_dialog_button(const account_action_button& button, ui::draw_layer layer, bool enabled) {
    return draw_dialog_button(button.rect, button.label, button.font_size, layer, enabled);
}

ui::button_state draw_dialog_button(const verify_action_button& button, ui::draw_layer layer, bool enabled) {
    return draw_dialog_button(button.rect, button.label, 15, layer, enabled);
}

ui::button_state draw_dialog_button(const primary_form_action_button& button,
                                    ui::draw_layer layer,
                                    bool enabled) {
    return draw_dialog_button(button.rect, button.label, 16, layer, enabled);
}

struct login_dialog_interaction {
    song_select::login_dialog_command command = song_select::login_dialog_command::none;
    song_select::login_dialog_state_action state_action = song_select::login_dialog_state_action::none;
};

struct auth_form_fields_result {
    bool submitted = false;
    Rectangle last_input_rect{};
};

song_select::login_dialog_command draw_account_actions(const account_mode_layout& layout,
                                                       ui::draw_layer layer,
                                                       bool request_active) {
    for (const account_action_button& button : account_action_buttons_for(layout)) {
        if (draw_dialog_button(button, layer, !request_active).clicked && !request_active) {
            return button.command;
        }
    }
    return song_select::login_dialog_command::none;
}

song_select::login_dialog_command draw_verify_actions(const verify_mode_layout& layout,
                                                      const ui::text_input_result& code_result,
                                                      ui::draw_layer layer,
                                                      bool request_active) {
    for (const verify_action_button& button : verify_action_buttons_for(layout)) {
        const bool submitted_verify =
            button.command == song_select::login_dialog_command::request_verify && code_result.submitted;
        if ((draw_dialog_button(button, layer, !request_active).clicked || submitted_verify) && !request_active) {
            return button.command;
        }
    }
    return song_select::login_dialog_command::none;
}

song_select::login_dialog_state_action draw_auth_tabs(const auth_mode_layout& layout,
                                                      song_select::login_dialog_mode mode,
                                                      ui::draw_layer layer,
                                                      bool request_active) {
    for (const auth_tab_button& button : auth_tab_buttons_for(layout, mode)) {
        if (draw_tab(button, layer).clicked && !request_active) {
            return button.tab.state_action;
        }
    }
    return song_select::login_dialog_state_action::none;
}

auth_form_fields_result draw_auth_form_fields(Rectangle dialog_rect,
                                              song_select::login_dialog_state& dialog_state,
                                              bool signup,
                                              ui::draw_layer layer) {
    int row = 0;
    bool submitted = false;
    if (signup) {
        const ui::text_input_result display_name_result = ui::text_input(
            make_row(dialog_rect, row++), dialog_state.display_name_input, "Name", "Display name",
            login_text_input_options(layer, 32));
        submitted = submitted || display_name_result.submitted;
    }

    const ui::text_input_result email_result = ui::text_input(
        make_row(dialog_rect, row++), dialog_state.email_input, "Email", "name@example.com",
        login_text_input_options(layer, 64));
    submitted = submitted || email_result.submitted;

    const ui::text_input_result password_result = ui::text_input(
        make_row(dialog_rect, row++), dialog_state.password_input, "Pass", "Password",
        login_text_input_options(layer, 64, true));
    submitted = submitted || password_result.submitted;

    if (signup) {
        const ui::text_input_result confirm_result = ui::text_input(
            make_row(dialog_rect, row++), dialog_state.password_confirmation_input, "Confirm", "Repeat password",
            login_text_input_options(layer, 64, true));
        submitted = submitted || confirm_result.submitted;
    }

    return {
        .submitted = submitted,
        .last_input_rect = make_row(dialog_rect, row - 1),
    };
}

song_select::login_dialog_command draw_primary_form_action(Rectangle dialog_rect,
                                                           const auth_form_fields_result& form_fields,
                                                           bool signup,
                                                           ui::draw_layer layer,
                                                           bool request_active) {
    const primary_form_action_button primary_button =
        primary_form_action_button_for(dialog_rect, form_fields.last_input_rect, signup);
    if ((draw_dialog_button(primary_button, layer, !request_active).clicked || form_fields.submitted) &&
        !request_active) {
        return primary_button.command;
    }
    return song_select::login_dialog_command::none;
}

}  // namespace

namespace song_select {

void open_login_dialog(login_dialog_state& dialog_state, const auth::session_summary& summary) {
    dialog_state.open = true;
    if (!summary.logged_in) {
        dialog_state.mode = login_dialog_mode::login;
    }
    dialog_state.open_anim = 0.0f;
    dialog_state.email_input.value = summary.email;
    dialog_state.display_name_input.value.clear();
    dialog_state.password_input.value.clear();
    dialog_state.password_confirmation_input.value.clear();
    dialog_state.verification_code_input.value.clear();
    deactivate_form_inputs(dialog_state);
}

Rectangle login_dialog_rect(const auth_state& auth_state, const login_dialog_state& dialog_state,
                            Rectangle anchor_rect, Rectangle screen_rect) {
    return make_login_dialog_layout(auth_state, dialog_state, anchor_rect, screen_rect).dialog_rect;
}

login_dialog_layout make_login_dialog_layout(const auth_state& auth_state,
                                             const login_dialog_state& dialog_state,
                                             Rectangle anchor_rect,
                                             Rectangle screen_rect,
                                             ui::draw_layer layer) {
    return {
        .anchor_rect = anchor_rect,
        .screen_rect = screen_rect,
        .dialog_rect = dialog_rect_for(auth_state, dialog_state, anchor_rect, screen_rect),
        .layer = layer,
    };
}

namespace {

void draw_login_dialog_frame(Rectangle dialog_rect) {
    const auto& theme = *g_theme;
    const login_dialog_frame_layout frame = make_frame_layout(dialog_rect);

    ui::panel(dialog_rect);
    ui::draw_text_in_rect("Account", 24,
                          frame.title,
                          theme.text, ui::text_align::left);
    ui::draw_text_in_rect("Connect to raythm-Server", 14,
                          frame.subtitle,
                          theme.text_secondary, ui::text_align::left);
}

login_dialog_interaction draw_account_mode(const song_select::auth_state& auth_state,
                                           Rectangle dialog_rect,
                                           ui::draw_layer layer,
                                           bool request_active) {
    const auto& theme = *g_theme;
    const account_mode_layout account_layout = make_account_mode_layout(dialog_rect);

    ui::draw_text_in_rect("Signed in", 20, account_layout.signed_in, theme.success, ui::text_align::left);
    ui::draw_text_in_rect(auth_state.display_name.empty() ? auth_state.email.c_str() : auth_state.display_name.c_str(),
                          18,
                          account_layout.display_name,
                          theme.text_secondary,
                          ui::text_align::left);
    ui::draw_text_in_rect(auth_state.email.c_str(),
                          14,
                          account_layout.email,
                          theme.text_muted,
                          ui::text_align::left);
    ui::draw_text_in_rect(auth_state.email_verified
                              ? "Email verified"
                              : "Verify on the Web to submit online scores.",
                          13,
                          account_layout.verification,
                          auth_state.email_verified ? theme.success : theme.error,
                          ui::text_align::left);
    if (const song_select::login_dialog_command command =
            draw_account_actions(account_layout, layer, request_active);
        command != song_select::login_dialog_command::none) {
        return {.command = command};
    }
    return {};
}

login_dialog_interaction draw_verify_mode(song_select::login_dialog_state& dialog_state,
                                          Rectangle dialog_rect,
                                          ui::draw_layer layer,
                                          bool request_active) {
    const auto& theme = *g_theme;
    const verify_mode_layout verify_layout = make_verify_mode_layout(dialog_rect);

    ui::draw_text_in_rect(verification_title(dialog_state.verification), 20,
                          verify_layout.title, theme.text, ui::text_align::left);
    ui::draw_text_in_rect(dialog_state.verification_email.c_str(), 14,
                          verify_layout.email, theme.text_muted, ui::text_align::left);
    const ui::text_input_result code_result = ui::text_input(
        verify_layout.code, dialog_state.verification_code_input, "Code", "6 digit code",
        login_text_input_options(layer, 12));

    if (const song_select::login_dialog_command command =
            draw_verify_actions(verify_layout, code_result, layer, request_active);
        command != song_select::login_dialog_command::none) {
        return {.command = command};
    }
    return {};
}

login_dialog_interaction draw_auth_form_mode(song_select::login_dialog_state& dialog_state,
                                             Rectangle dialog_rect,
                                             ui::draw_layer layer,
                                             bool request_active) {
    login_dialog_interaction interaction;
    const auth_mode_layout auth_layout = make_auth_mode_layout(dialog_rect);

    interaction.state_action = draw_auth_tabs(auth_layout, dialog_state.mode, layer, request_active);

    const bool signup = dialog_state.mode == song_select::login_dialog_mode::signup;
    const auth_form_fields_result form_fields =
        draw_auth_form_fields(dialog_rect, dialog_state, signup, layer);

    if (ui::is_tab_pressed() && !request_active) {
        interaction.state_action = ui::is_shift_down()
            ? song_select::login_dialog_state_action::focus_previous
            : song_select::login_dialog_state_action::focus_next;
    }

    if (const song_select::login_dialog_command command =
            draw_primary_form_action(dialog_rect, form_fields, signup, layer, request_active);
        command != song_select::login_dialog_command::none) {
        interaction.command = command;
    }

    return interaction;
}

login_dialog_interaction draw_login_dialog_view(const song_select::auth_state& auth_state,
                                                song_select::login_dialog_state& dialog_state,
                                                const song_select::login_dialog_layout& layout,
                                                bool request_active) {
    if (!dialog_state.open) {
        return {};
    }

    const Rectangle dialog_rect = layout.dialog_rect;
    const ui::draw_layer layer = layout.layer;
    draw_login_dialog_frame(dialog_rect);

    if (auth_state.logged_in) {
        return draw_account_mode(auth_state, dialog_rect, layer, request_active);
    }

    if (dialog_state.mode == song_select::login_dialog_mode::verify) {
        return draw_verify_mode(dialog_state, dialog_rect, layer, request_active);
    }

    return draw_auth_form_mode(dialog_state, dialog_rect, layer, request_active);
}

}  // namespace

login_dialog_result draw_login_dialog_result(const auth_state& auth_state,
                                             login_dialog_state& dialog_state,
                                             const login_dialog_layout& layout,
                                             bool request_active) {
    const login_dialog_interaction interaction =
        draw_login_dialog_view(auth_state, dialog_state, layout, request_active);
    return {
        .command = interaction.command,
        .state_action = interaction.state_action,
    };
}

void apply_login_dialog_result(login_dialog_state& dialog_state, const login_dialog_result& result) {
    switch (result.state_action) {
        case login_dialog_state_action::none:
            break;
        case login_dialog_state_action::show_login:
            dialog_state.mode = login_dialog_mode::login;
            deactivate_form_inputs(dialog_state);
            break;
        case login_dialog_state_action::show_signup:
            dialog_state.mode = login_dialog_mode::signup;
            deactivate_form_inputs(dialog_state);
            break;
        case login_dialog_state_action::focus_previous:
            focus_relative_input(dialog_state, dialog_state.mode == login_dialog_mode::signup, -1);
            break;
        case login_dialog_state_action::focus_next:
            focus_relative_input(dialog_state, dialog_state.mode == login_dialog_mode::signup, 1);
            break;
    }
}

login_dialog_command draw_login_dialog(const auth_state& auth_state,
                                       login_dialog_state& dialog_state,
                                       const login_dialog_layout& layout,
                                       bool request_active) {
    const login_dialog_result result =
        draw_login_dialog_result(auth_state, dialog_state, layout, request_active);
    apply_login_dialog_result(dialog_state, result);
    return result.command;
}

login_dialog_command draw_login_dialog(const auth_state& auth_state, login_dialog_state& dialog_state,
                                       Rectangle anchor_rect, Rectangle screen_rect,
                                       bool request_active, ui::draw_layer layer) {
    const login_dialog_layout layout =
        make_login_dialog_layout(auth_state, dialog_state, anchor_rect, screen_rect, layer);
    return draw_login_dialog(auth_state, dialog_state, layout, request_active);
}

void open_login_dialog(state& state, const auth::session_summary& summary) {
    open_login_dialog(state.login_dialog, summary);
}

Rectangle login_dialog_rect(const state& state) {
    return make_login_dialog_layout(state.auth,
                                    state.login_dialog,
                                    song_select::layout::kLoginButtonRect,
                                    song_select::layout::kScreenRect,
                                    song_select::layout::kModalLayer).dialog_rect;
}

login_dialog_command draw_login_dialog(state& state, bool request_active) {
    const login_dialog_layout layout =
        make_login_dialog_layout(state.auth,
                                 state.login_dialog,
                                 song_select::layout::kLoginButtonRect,
                                 song_select::layout::kScreenRect,
                                 song_select::layout::kModalLayer);
    return draw_login_dialog(state.auth, state.login_dialog, layout, request_active);
}

}  // namespace song_select
