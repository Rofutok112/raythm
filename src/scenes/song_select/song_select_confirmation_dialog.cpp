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
    constexpr float kDialogButtonWidth = 168.0f;
    constexpr float kDialogButtonHeight = 40.0f;
    constexpr float kDialogButtonGap = 24.0f;
    const float buttons_left =
        layout::kConfirmDialogRect.x +
        (layout::kConfirmDialogRect.width - (kDialogButtonWidth * 2.0f + kDialogButtonGap)) * 0.5f;
    const float buttons_y =
        layout::kConfirmDialogRect.y + layout::kConfirmDialogRect.height - kDialogButtonHeight - 34.0f;
    const Rectangle confirm_button = {buttons_left, buttons_y, kDialogButtonWidth, kDialogButtonHeight};
    const Rectangle cancel_button = {
        buttons_left + kDialogButtonWidth + kDialogButtonGap,
        buttons_y,
        kDialogButtonWidth,
        kDialogButtonHeight
    };

    auto enqueue_dialog_button = [&](Rectangle rect, const std::string& label, Color tone) {
        const bool hovered = ui::is_hovered(rect, layout::kModalLayer);
        const bool pressed = ui::is_pressed(rect, layout::kModalLayer);
        const bool clicked = ui::is_clicked(rect, layout::kModalLayer);
        const Rectangle visual = pressed ? ui::inset(rect, 1.5f) : rect;
        const Color bg = hovered ? g_theme->row_hover : g_theme->row;
        const Color fill = with_alpha(lerp_color(bg, tone, hovered ? 0.16f : 0.08f), 228);
        const Color border = with_alpha(lerp_color(g_theme->border, tone, 0.35f), 220);
        ui::enqueue_draw_command(layout::kModalLayer,
                                 [visual, fill, border, label_copy = label]() {
            ui::draw_rect_f(visual, fill);
            ui::draw_rect_lines(visual, 1.5f, border);
            ui::draw_text_in_rect(label_copy.c_str(), 16, visual, g_theme->text);
        });
        return ui::button_state{hovered, pressed, clicked};
    };

    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    if (danger_action) {
        ui::enqueue_draw_command(layout::kModalLayer, []() {
            ui::draw_rect_f(layout::kConfirmDialogRect, with_alpha(g_theme->panel, 248));
            ui::draw_rect_lines(layout::kConfirmDialogRect, 2.0f, with_alpha(g_theme->error, 220));
        });
    } else {
        ui::enqueue_panel(layout::kConfirmDialogRect, layout::kModalLayer);
    }
    ui::enqueue_text_in_rect(title.c_str(), 28,
                             {layout::kConfirmDialogRect.x + 20.0f, layout::kConfirmDialogRect.y + 22.0f,
                              layout::kConfirmDialogRect.width - 40.0f, 30.0f},
                             g_theme->text, ui::text_align::center, layout::kModalLayer);
    ui::enqueue_text_in_rect(message.c_str(), 18,
                             {layout::kConfirmDialogRect.x + 28.0f, layout::kConfirmDialogRect.y + 76.0f,
                              layout::kConfirmDialogRect.width - 56.0f, 40.0f},
                             g_theme->text_secondary, ui::text_align::center, layout::kModalLayer);
    if (!hint.empty()) {
        ui::enqueue_text_in_rect(hint.c_str(), 16,
                                 {layout::kConfirmDialogRect.x + 28.0f, layout::kConfirmDialogRect.y + 104.0f,
                                  layout::kConfirmDialogRect.width - 56.0f, 22.0f},
                                 danger_action ? g_theme->text_secondary : g_theme->text_hint,
                                 ui::text_align::center, layout::kModalLayer);
    }
    const ui::button_state confirm =
        enqueue_dialog_button(confirm_button, confirm_label, danger_action ? g_theme->error : g_theme->accent);
    const ui::button_state cancel =
        enqueue_dialog_button(cancel_button, "CANCEL", g_theme->text_muted);

    if (confirm.clicked) {
        return confirmation_command::confirm;
    }
    if (cancel.clicked) {
        return confirmation_command::cancel;
    }
    return confirmation_command::none;
}

}  // namespace song_select
