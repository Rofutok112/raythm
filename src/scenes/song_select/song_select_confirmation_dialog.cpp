#include "song_select/song_select_confirmation_dialog.h"

#include <array>

#include "song_select/song_select_layout.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_layout.h"

namespace {

constexpr float kDialogButtonWidth = 168.0f;
constexpr float kDialogButtonHeight = 40.0f;
constexpr float kDialogButtonGap = 24.0f;
constexpr float kDialogButtonBottomPadding = 34.0f;

struct confirmation_dialog_layout {
    Rectangle panel;
    Rectangle title;
    Rectangle message;
    Rectangle hint;
    std::array<Rectangle, 2> buttons;
};

confirmation_dialog_layout make_confirmation_dialog_layout(Rectangle panel) {
    const float buttons_y = panel.y + panel.height - kDialogButtonHeight - kDialogButtonBottomPadding;
    const Rectangle button_row = {panel.x, buttons_y, panel.width, kDialogButtonHeight};
    std::array<Rectangle, 2> buttons{};
    ui::centered_hstack(button_row, kDialogButtonWidth, kDialogButtonHeight, kDialogButtonGap, buttons);
    return {
        panel,
        {panel.x + 20.0f, panel.y + 22.0f, panel.width - 40.0f, 30.0f},
        {panel.x + 28.0f, panel.y + 76.0f, panel.width - 56.0f, 40.0f},
        {panel.x + 28.0f, panel.y + 104.0f, panel.width - 56.0f, 22.0f},
        buttons,
    };
}

struct confirmation_button_descriptor {
    song_select::confirmation_command command = song_select::confirmation_command::none;
    bool confirm = false;
    const char* label = "";
};

struct confirmation_button {
    song_select::confirmation_command command = song_select::confirmation_command::none;
    std::string label;
    Color tone{};
    Rectangle rect{};
};

struct confirmation_dialog_content {
    std::string title;
    std::string message;
    std::string hint;
    std::string confirm_label;
    bool danger_action = false;
};

constexpr std::array<confirmation_button_descriptor, 2> kConfirmationButtons = {{
    {song_select::confirmation_command::confirm, true, ""},
    {song_select::confirmation_command::cancel, false, "CANCEL"},
}};

confirmation_dialog_content confirmation_dialog_content_for(
    const song_select::confirmation_dialog_state& dialog_state) {
    const bool deleting_song = dialog_state.action == song_select::pending_confirmation_action::delete_song;
    const bool deleting_chart = dialog_state.action == song_select::pending_confirmation_action::delete_chart;
    const bool deleting_mv = dialog_state.action == song_select::pending_confirmation_action::delete_mv;
    const bool danger_action = deleting_song || deleting_chart || deleting_mv;
    return {
        dialog_state.title.empty()
            ? (deleting_song ? "Delete Song" : deleting_chart ? "Delete Chart" : "Confirm Action")
            : dialog_state.title,
        dialog_state.message.empty()
            ? (deleting_song
                ? "This will remove the song and linked local charts."
                : "This will remove the selected local chart file.")
            : dialog_state.message,
        dialog_state.hint.empty()
            ? (danger_action ? "This action cannot be undone." : "")
            : dialog_state.hint,
        dialog_state.confirm_label.empty()
            ? (danger_action ? "DELETE" : "CONFIRM")
            : dialog_state.confirm_label,
        danger_action,
    };
}

confirmation_button confirmation_button_for(const confirmation_button_descriptor& descriptor,
                                            Rectangle rect,
                                            const std::string& confirm_label,
                                            bool danger_action) {
    return {
        .command = descriptor.command,
        .label = descriptor.confirm ? confirm_label : std::string(descriptor.label),
        .tone = descriptor.confirm
            ? (danger_action ? g_theme->error : g_theme->accent)
            : g_theme->text_muted,
        .rect = rect,
    };
}

ui::button_state enqueue_confirmation_button(const confirmation_button& button) {
    return ui::queued_toned_action_button(button.rect, button.label.c_str(), button.tone, {
        .layer = song_select::layout::kModalLayer,
        .font_size = 16,
        .border_width = 1.5f,
        .bg_alpha = 228,
        .bg_hover_alpha = 228,
    });
}

void enqueue_confirmation_panel(Rectangle rect, bool danger_action) {
    if (danger_action) {
        ui::queued_panel(rect, song_select::layout::kModalLayer, {
            .fill = with_alpha(g_theme->panel, 248),
            .border_color = with_alpha(g_theme->error, 220),
            .custom_colors = true,
        });
    } else {
        ui::queued_panel(rect, song_select::layout::kModalLayer);
    }
}

void enqueue_confirmation_text(const confirmation_dialog_layout& dialog_layout,
                               const confirmation_dialog_content& content) {
    ui::enqueue_text_in_rect(content.title.c_str(), 28,
                             dialog_layout.title,
                             g_theme->text, ui::text_align::center, song_select::layout::kModalLayer);
    ui::enqueue_text_in_rect(content.message.c_str(), 18,
                             dialog_layout.message,
                             g_theme->text_secondary, ui::text_align::center, song_select::layout::kModalLayer);
    if (!content.hint.empty()) {
        ui::enqueue_text_in_rect(content.hint.c_str(), 16,
                                 dialog_layout.hint,
                                 content.danger_action ? g_theme->text_secondary : g_theme->text_hint,
                                 ui::text_align::center, song_select::layout::kModalLayer);
    }
}

song_select::confirmation_command enqueue_confirmation_buttons(
    const confirmation_dialog_layout& dialog_layout,
    const confirmation_dialog_content& content) {
    for (std::size_t i = 0; i < kConfirmationButtons.size(); ++i) {
        const confirmation_button button =
            confirmation_button_for(
                kConfirmationButtons[i],
                dialog_layout.buttons[i],
                content.confirm_label,
                content.danger_action);
        if (enqueue_confirmation_button(button).clicked) {
            return button.command;
        }
    }
    return song_select::confirmation_command::none;
}

}  // namespace

namespace song_select {

void open_confirmation_dialog(state& state, pending_confirmation_action action,
                              std::string title, std::string message,
                              std::string hint, std::string confirm_label,
                              int song_index, int chart_index) {
    state.confirmation_dialog.open = true;
    state.confirmation_dialog.action = action;
    state.confirmation_dialog.song_index = song_index;
    state.confirmation_dialog.chart_index = chart_index;
    state.confirmation_dialog.suppress_initial_pointer_cancel = true;
    state.confirmation_dialog.title = std::move(title);
    state.confirmation_dialog.message = std::move(message);
    state.confirmation_dialog.hint = std::move(hint);
    state.confirmation_dialog.confirm_label = std::move(confirm_label);
}

confirmation_command draw_confirmation_dialog(const state& state) {
    if (!state.confirmation_dialog.open) {
        return confirmation_command::none;
    }

    const confirmation_dialog_content content =
        confirmation_dialog_content_for(state.confirmation_dialog);
    const confirmation_dialog_layout dialog_layout = make_confirmation_dialog_layout(layout::kConfirmDialogRect);

    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    enqueue_confirmation_panel(dialog_layout.panel, content.danger_action);
    enqueue_confirmation_text(dialog_layout, content);
    return enqueue_confirmation_buttons(dialog_layout, content);
}

}  // namespace song_select
