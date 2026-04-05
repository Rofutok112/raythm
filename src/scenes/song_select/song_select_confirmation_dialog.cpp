#include "song_select/song_select_confirmation_dialog.h"

#include "song_select/song_select_layout.h"
#include "theme.h"
#include "ui_draw.h"

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
    const std::string title = state.confirmation_dialog.title.empty()
        ? (deleting_song ? "Delete Song" : "Delete Chart")
        : state.confirmation_dialog.title;
    const std::string message = state.confirmation_dialog.message.empty()
        ? (deleting_song
            ? "This will remove the song and linked AppData charts."
            : "This will remove the selected AppData chart file.")
        : state.confirmation_dialog.message;
    const std::string hint = state.confirmation_dialog.hint.empty()
        ? ((deleting_song || deleting_chart) ? "Official content cannot be deleted." : "")
        : state.confirmation_dialog.hint;
    const std::string confirm_label = state.confirmation_dialog.confirm_label.empty()
        ? (deleting_song || deleting_chart ? "DELETE" : "CONFIRM")
        : state.confirmation_dialog.confirm_label;
    constexpr Rectangle confirm_button = {layout::kConfirmDialogRect.x + 76.0f, layout::kConfirmDialogRect.y + 148.0f, 132.0f, 34.0f};
    constexpr Rectangle cancel_button = {layout::kConfirmDialogRect.x + 272.0f, layout::kConfirmDialogRect.y + 148.0f, 132.0f, 34.0f};

    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(layout::kConfirmDialogRect, layout::kModalLayer);
    ui::enqueue_text_in_rect(title.c_str(), 28,
                             {layout::kConfirmDialogRect.x + 20.0f, layout::kConfirmDialogRect.y + 22.0f,
                              layout::kConfirmDialogRect.width - 40.0f, 30.0f},
                             g_theme->text, ui::text_align::center, layout::kModalLayer);
    ui::enqueue_text_in_rect(message.c_str(), 18,
                             {layout::kConfirmDialogRect.x + 28.0f, layout::kConfirmDialogRect.y + 76.0f,
                              layout::kConfirmDialogRect.width - 56.0f, 24.0f},
                             g_theme->text_secondary, ui::text_align::center, layout::kModalLayer);
    if (!hint.empty()) {
        ui::enqueue_text_in_rect(hint.c_str(), 16,
                                 {layout::kConfirmDialogRect.x + 28.0f, layout::kConfirmDialogRect.y + 104.0f,
                                  layout::kConfirmDialogRect.width - 56.0f, 22.0f},
                                 g_theme->text_hint, ui::text_align::center, layout::kModalLayer);
    }
    const ui::button_state confirm = ui::enqueue_button(confirm_button, confirm_label.c_str(), 16, layout::kModalLayer, 1.5f);
    const ui::button_state cancel = ui::enqueue_button(cancel_button, "CANCEL", 16, layout::kModalLayer, 1.5f);

    if (confirm.clicked) {
        return confirmation_command::confirm;
    }
    if (cancel.clicked) {
        return confirmation_command::cancel;
    }
    return confirmation_command::none;
}

}  // namespace song_select
