#include "editor/service/editor_metadata_service.h"

editor_metadata_apply_result editor_metadata_service::apply_changes(editor_metadata_apply_context context) {
    editor_metadata_apply_result result;
    chart_meta updated = context.state.data().meta;
    updated.difficulty = context.metadata_panel.difficulty_input.value;
    updated.chart_author = context.metadata_panel.chart_author_input.value;
    updated.key_count = context.metadata_panel.key_count;
    if (context.state.file_path().empty()) {
        updated.chart_id = context.generated_chart_id;
    }

    result.key_count_changed = updated.key_count != context.state.data().meta.key_count;
    if (result.key_count_changed && !context.clear_notes_for_key_count_change && !context.state.data().notes.empty()) {
        context.metadata_panel.pending_key_count = updated.key_count;
        context.metadata_panel.key_count_confirm_open = true;
        context.metadata_panel.error = "Changing mode will clear all notes.";
        result.confirmation_required = true;
        return result;
    }

    if (!context.state.modify_metadata(updated, context.clear_notes_for_key_count_change)) {
        context.metadata_panel.error = "Failed to update chart metadata.";
        context.metadata_panel.key_count = context.state.data().meta.key_count;
        return result;
    }

    result.success = true;
    return result;
}

bool editor_metadata_service::apply_chart_offset(editor_state& state, int offset_ms) {
    chart_meta updated = state.data().meta;
    updated.offset = offset_ms;
    return state.modify_metadata(updated);
}
