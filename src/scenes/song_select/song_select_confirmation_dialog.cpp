#include "song_select/song_select_confirmation_dialog.h"

#include "song_select/song_select_layout.h"
#include "theme.h"
#include "ui_draw.h"

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
    Rectangle confirm_button;
    Rectangle cancel_button;
};

confirmation_dialog_layout make_confirmation_dialog_layout(Rectangle panel) {
    const float buttons_left =
        panel.x + (panel.width - (kDialogButtonWidth * 2.0f + kDialogButtonGap)) * 0.5f;
    const float buttons_y = panel.y + panel.height - kDialogButtonHeight - kDialogButtonBottomPadding;
    return {
        panel,
        {panel.x + 20.0f, panel.y + 22.0f, panel.width - 40.0f, 30.0f},
        {panel.x + 28.0f, panel.y + 76.0f, panel.width - 56.0f, 40.0f},
        {panel.x + 28.0f, panel.y + 104.0f, panel.width - 56.0f, 22.0f},
        {buttons_left, buttons_y, kDialogButtonWidth, kDialogButtonHeight},
        {buttons_left + kDialogButtonWidth + kDialogButtonGap, buttons_y,
         kDialogButtonWidth, kDialogButtonHeight},
    };
}

ui::button_state enqueue_confirmation_button(Rectangle rect, const std::string& label, Color tone) {
    return ui::queued_toned_action_button(rect, label.c_str(), tone, {
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

    const bool deleting_song = state.confirmation_dialog.action == pending_confirmation_action::delete_song;
    const bool deleting_chart = state.confirmation_dialog.action == pending_confirmation_action::delete_chart;
    const bool deleting_mv = state.confirmation_dialog.action == pending_confirmation_action::delete_mv;
    const bool danger_action = deleting_song || deleting_chart || deleting_mv;
    const std::string title = state.confirmation_dialog.title.empty()
        ? (deleting_song ? "Delete Song" : deleting_chart ? "Delete Chart" : "Confirm Action")
        : state.confirmation_dialog.title;
    const std::string message = state.confirmation_dialog.message.empty()
        ? (deleting_song
            ? "This will remove the song and linked local charts."
            : "This will remove the selected local chart file.")
        : state.confirmation_dialog.message;
    const std::string hint = state.confirmation_dialog.hint.empty()
        ? (danger_action ? "This action cannot be undone." : "")
        : state.confirmation_dialog.hint;
    const std::string confirm_label = state.confirmation_dialog.confirm_label.empty()
        ? (danger_action ? "DELETE" : "CONFIRM")
        : state.confirmation_dialog.confirm_label;
    const confirmation_dialog_layout dialog_layout = make_confirmation_dialog_layout(layout::kConfirmDialogRect);

    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    enqueue_confirmation_panel(dialog_layout.panel, danger_action);
    ui::enqueue_text_in_rect(title.c_str(), 28,
                             dialog_layout.title,
                             g_theme->text, ui::text_align::center, layout::kModalLayer);
    ui::enqueue_text_in_rect(message.c_str(), 18,
                             dialog_layout.message,
                             g_theme->text_secondary, ui::text_align::center, layout::kModalLayer);
    if (!hint.empty()) {
        ui::enqueue_text_in_rect(hint.c_str(), 16,
                                 dialog_layout.hint,
                                 danger_action ? g_theme->text_secondary : g_theme->text_hint,
                                 ui::text_align::center, layout::kModalLayer);
    }
    const ui::button_state confirm =
        enqueue_confirmation_button(dialog_layout.confirm_button, confirm_label, danger_action ? g_theme->error : g_theme->accent);
    const ui::button_state cancel =
        enqueue_confirmation_button(dialog_layout.cancel_button, "CANCEL", g_theme->text_muted);

    if (confirm.clicked) {
        return confirmation_command::confirm;
    }
    if (cancel.clicked) {
        return confirmation_command::cancel;
    }
    return confirmation_command::none;
}

}  // namespace song_select
