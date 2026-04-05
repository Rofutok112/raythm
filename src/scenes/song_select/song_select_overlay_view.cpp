#include "song_select/song_select_overlay_view.h"

#include <vector>

#include "song_select/song_select_layout.h"
#include "theme.h"
#include "ui_draw.h"

namespace song_select {

context_menu_command draw_context_menu(const state& state) {
    if (!state.context_menu.open) {
        return context_menu_command::none;
    }

    std::vector<ui::context_menu_item> items;
    if (state.context_menu.target == context_menu_target::song) {
        const bool valid_song = state.context_menu.song_index >= 0 &&
                                state.context_menu.song_index < static_cast<int>(state.songs.size());
        const bool can_add_chart_to_song = valid_song &&
                                           state.songs[static_cast<size_t>(state.context_menu.song_index)].song.source != content_source::official;
        const bool can_edit_song = valid_song &&
                                   state.songs[static_cast<size_t>(state.context_menu.song_index)].song.can_edit;
        const bool can_delete_song = valid_song &&
                                     state.songs[static_cast<size_t>(state.context_menu.song_index)].song.can_delete;
        items = {
            {"EDIT META", can_edit_song},
            {"NEW CHART", can_add_chart_to_song},
            {"DELETE SONG", can_delete_song},
        };
    } else {
        bool can_edit_chart = false;
        bool can_delete_chart = false;
        if (state.context_menu.song_index >= 0 &&
            state.context_menu.song_index < static_cast<int>(state.songs.size())) {
            const auto& charts = state.songs[static_cast<size_t>(state.context_menu.song_index)].charts;
            if (state.context_menu.chart_index >= 0 &&
                state.context_menu.chart_index < static_cast<int>(charts.size())) {
                can_edit_chart = charts[static_cast<size_t>(state.context_menu.chart_index)].source != content_source::official;
                can_delete_chart = charts[static_cast<size_t>(state.context_menu.chart_index)].can_delete;
            }
        }
        items = {
            {"EDIT CHART", can_edit_chart},
            {"DELETE CHART", can_delete_chart},
        };
    }

    const ui::context_menu_state menu = ui::enqueue_context_menu(state.context_menu.rect, items,
                                                                 layout::kContextMenuLayer, 16,
                                                                 layout::kContextMenuItemHeight,
                                                                 layout::kContextMenuItemSpacing);
    if (menu.clicked_index == 0) {
        return state.context_menu.target == context_menu_target::song
            ? context_menu_command::edit_song
            : context_menu_command::edit_chart;
    }
    if (state.context_menu.target == context_menu_target::song && menu.clicked_index == 1) {
        return context_menu_command::new_chart;
    }
    if ((state.context_menu.target == context_menu_target::song && menu.clicked_index == 2) ||
        (state.context_menu.target == context_menu_target::chart && menu.clicked_index == 1)) {
        return state.context_menu.target == context_menu_target::song
            ? context_menu_command::request_delete_song
            : context_menu_command::request_delete_chart;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        !ui::is_hovered(state.context_menu.rect, layout::kContextMenuLayer)) {
        return context_menu_command::close_menu;
    }
    return context_menu_command::none;
}

confirmation_command draw_confirmation_dialog(const state& state) {
    if (!state.confirmation_dialog.open) {
        return confirmation_command::none;
    }

    const bool deleting_song = state.confirmation_dialog.action == pending_confirmation_action::delete_song;
    const char* title = deleting_song ? "Delete Song" : "Delete Chart";
    const char* message = deleting_song
        ? "This will remove the song and linked AppData charts."
        : "This will remove the selected AppData chart file.";
    const Rectangle confirm_button = {layout::kConfirmDialogRect.x + 76.0f, layout::kConfirmDialogRect.y + 148.0f, 132.0f, 34.0f};
    const Rectangle cancel_button = {layout::kConfirmDialogRect.x + 272.0f, layout::kConfirmDialogRect.y + 148.0f, 132.0f, 34.0f};

    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(layout::kConfirmDialogRect, layout::kModalLayer);
    ui::enqueue_text_in_rect(title, 28,
                             {layout::kConfirmDialogRect.x + 20.0f, layout::kConfirmDialogRect.y + 22.0f,
                              layout::kConfirmDialogRect.width - 40.0f, 30.0f},
                             g_theme->text, ui::text_align::center, layout::kModalLayer);
    ui::enqueue_text_in_rect(message, 18,
                             {layout::kConfirmDialogRect.x + 28.0f, layout::kConfirmDialogRect.y + 76.0f,
                              layout::kConfirmDialogRect.width - 56.0f, 24.0f},
                             g_theme->text_secondary, ui::text_align::center, layout::kModalLayer);
    ui::enqueue_text_in_rect("Official content cannot be deleted.", 16,
                             {layout::kConfirmDialogRect.x + 28.0f, layout::kConfirmDialogRect.y + 104.0f,
                              layout::kConfirmDialogRect.width - 56.0f, 22.0f},
                             g_theme->text_hint, ui::text_align::center, layout::kModalLayer);
    const ui::button_state confirm = ui::enqueue_button(confirm_button, "DELETE", 16, layout::kModalLayer, 1.5f);
    const ui::button_state cancel = ui::enqueue_button(cancel_button, "CANCEL", 16, layout::kModalLayer, 1.5f);

    if (confirm.clicked) {
        return confirmation_command::confirm;
    }
    if (cancel.clicked || (!state.confirmation_dialog.suppress_initial_pointer_cancel &&
                           IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                           !ui::is_hovered(layout::kConfirmDialogRect, layout::kModalLayer))) {
        return confirmation_command::cancel;
    }
    return confirmation_command::none;
}

}  // namespace song_select
