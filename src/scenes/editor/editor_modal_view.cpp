#include "editor_modal_view.h"

#include "editor_layout.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_text_input.h"

namespace {
namespace layout = editor::layout;

const char* key_count_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

bool accepts_chart_file_character(int codepoint, const std::string&) {
    return (codepoint >= 'a' && codepoint <= 'z') ||
           (codepoint >= 'A' && codepoint <= 'Z') ||
           (codepoint >= '0' && codepoint <= '9') ||
           codepoint == '-' ||
           codepoint == '_' ||
           codepoint == '.';
}
}

void editor_modal_view::draw_unsaved_changes_dialog() {
    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(layout::kUnsavedChangesRect, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("Unsaved Changes", 28,
                             {layout::kUnsavedChangesRect.x + 20.0f, layout::kUnsavedChangesRect.y + 20.0f,
                              layout::kUnsavedChangesRect.width - 40.0f, 30.0f},
                             g_theme->text, ui::text_align::center, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("There are unsaved changes.", 18,
                             {layout::kUnsavedChangesRect.x + 28.0f, layout::kUnsavedChangesRect.y + 78.0f,
                              layout::kUnsavedChangesRect.width - 56.0f, 24.0f},
                             g_theme->text_secondary, ui::text_align::center, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("Save before leaving the editor?", 18,
                             {layout::kUnsavedChangesRect.x + 28.0f, layout::kUnsavedChangesRect.y + 104.0f,
                              layout::kUnsavedChangesRect.width - 56.0f, 24.0f},
                             g_theme->text, ui::text_align::center, ui::draw_layer::modal);

    ui::enqueue_button(layout::unsaved_save_button_rect(), "SAVE", 16, ui::draw_layer::modal, 1.5f);
    ui::enqueue_button(layout::unsaved_discard_button_rect(), "DISCARD", 16, ui::draw_layer::modal, 1.5f);
    ui::enqueue_button(layout::unsaved_cancel_button_rect(), "CANCEL", 16, ui::draw_layer::modal, 1.5f);
}

editor_modal_view_result editor_modal_view::draw_save_dialog(save_dialog_state& state) {
    editor_modal_view_result result;
    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(layout::kSaveDialogRect, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("Save Chart", 28,
                             {layout::kSaveDialogRect.x + 20.0f, layout::kSaveDialogRect.y + 18.0f,
                              layout::kSaveDialogRect.width - 40.0f, 30.0f},
                             g_theme->text, ui::text_align::center, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("Save into this song's charts directory.", 18,
                             {layout::kSaveDialogRect.x + 24.0f, layout::kSaveDialogRect.y + 52.0f,
                              layout::kSaveDialogRect.width - 48.0f, 22.0f},
                             g_theme->text_secondary, ui::text_align::center, ui::draw_layer::modal);

    const ui::text_input_result file_name_result = ui::draw_text_input(
        {layout::kSaveDialogRect.x + 20.0f, layout::kSaveDialogRect.y + 88.0f, layout::kSaveDialogRect.width - 40.0f, 38.0f},
        state.file_name_input, "File", "normal.chart", "new-chart.chart",
        ui::draw_layer::modal, 16, 48, accepts_chart_file_character, 64.0f);
    result.save_dialog_submit_requested = file_name_result.submitted;

    if (!state.error.empty()) {
        ui::draw_text_in_rect(state.error.c_str(), 16,
                              {layout::kSaveDialogRect.x + 24.0f, layout::kSaveDialogRect.y + 136.0f,
                               layout::kSaveDialogRect.width - 48.0f, 22.0f},
                              g_theme->error, ui::text_align::left);
    }

    ui::enqueue_button(layout::save_submit_button_rect(), "SAVE", 16, ui::draw_layer::modal, 1.5f);
    ui::enqueue_button(layout::save_cancel_button_rect(), "CANCEL", 16, ui::draw_layer::modal, 1.5f);
    return result;
}

void editor_modal_view::draw_key_count_confirmation(int pending_key_count) {
    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(layout::kMetadataConfirmRect, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("Change Key Mode", 28,
                             {layout::kMetadataConfirmRect.x + 20.0f, layout::kMetadataConfirmRect.y + 18.0f,
                              layout::kMetadataConfirmRect.width - 40.0f, 30.0f},
                             g_theme->text, ui::text_align::center, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("All placed notes will be cleared.", 18,
                             {layout::kMetadataConfirmRect.x + 28.0f, layout::kMetadataConfirmRect.y + 70.0f,
                              layout::kMetadataConfirmRect.width - 56.0f, 24.0f},
                             g_theme->text_secondary, ui::text_align::center, ui::draw_layer::modal);
    ui::enqueue_text_in_rect(TextFormat("Switch to %s?", key_count_label(pending_key_count)), 18,
                             {layout::kMetadataConfirmRect.x + 28.0f, layout::kMetadataConfirmRect.y + 98.0f,
                              layout::kMetadataConfirmRect.width - 56.0f, 24.0f},
                             g_theme->text, ui::text_align::center, ui::draw_layer::modal);

    ui::enqueue_button(layout::key_count_confirm_button_rect(), "CONFIRM", 16, ui::draw_layer::modal, 1.5f);
    ui::enqueue_button(layout::key_count_cancel_button_rect(), "CANCEL", 16, ui::draw_layer::modal, 1.5f);
}
