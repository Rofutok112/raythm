#include "editor_flow_controller.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "app_paths.h"
#include "chart_serializer.h"
#include "path_utils.h"

namespace {

std::string trim_copy(std::string value) {
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) == 0;
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) == 0;
    }).base(), value.end());
    return value;
}

std::string slugify(std::string text) {
    std::string slug;
    slug.reserve(text.size());
    bool previous_dash = false;

    for (unsigned char ch : text) {
        if (std::isalnum(ch) != 0) {
            slug.push_back(static_cast<char>(std::tolower(ch)));
            previous_dash = false;
        } else if (!previous_dash && !slug.empty()) {
            slug.push_back('-');
            previous_dash = true;
        }
    }

    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }

    return slug;
}

std::string suggested_chart_file_name(const chart_data& data) {
    std::string stem = slugify(data.meta.difficulty);
    if (stem.empty()) {
        stem = slugify(data.meta.chart_id);
    }
    if (stem.empty()) {
        stem = "new-chart";
    }
    return stem + ".rchart";
}

bool is_appdata_song(const song_data& song) {
    return song.directory.find(path_utils::to_utf8(app_paths::app_data_root())) != std::string::npos;
}

void open_save_dialog(save_dialog_state& save_dialog, editor_pending_action action_after_save, const chart_data& data) {
    save_dialog.open = true;
    save_dialog.submit_requested = false;
    save_dialog.action_after_save = action_after_save;
    save_dialog.error.clear();
    if (save_dialog.file_name_input.value.empty()) {
        save_dialog.file_name_input.value = suggested_chart_file_name(data);
    }
    save_dialog.file_name_input.active = false;
}

void close_save_dialog(save_dialog_state& save_dialog) {
    save_dialog.open = false;
    save_dialog.submit_requested = false;
    save_dialog.action_after_save = editor_pending_action::none;
    save_dialog.file_name_input.active = false;
}

editor_navigation_request navigation_for_action(editor_pending_action action) {
    switch (action) {
        case editor_pending_action::exit_to_song_select:
            return {editor_navigation_target::song_select, 0};
        case editor_pending_action::none:
            return {};
    }
    return {};
}

bool save_to_path(const editor_flow_context& context, const std::string& file_path, editor_flow_result& result) {
    if (context.state == nullptr || context.chart_for_save == nullptr || context.save_dialog == nullptr) {
        return false;
    }

    const std::filesystem::path path = path_utils::from_utf8(file_path);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        context.save_dialog->error = "Failed to prepare the charts directory.";
        return false;
    }

    if (!chart_serializer::serialize(*context.chart_for_save, path_utils::to_utf8(path))) {
        context.save_dialog->error = "Failed to save the chart file.";
        return false;
    }

    const std::string saved_path = path_utils::to_utf8(path);
    context.state->mark_saved(saved_path);
    context.save_dialog->error.clear();
    result.saved_chart_path = saved_path;
    return true;
}

bool save_chart_from_dialog(const editor_flow_context& context, editor_flow_result& result) {
    if (context.song == nullptr || context.state == nullptr || context.save_dialog == nullptr) {
        return false;
    }

    if (is_appdata_song(*context.song)) {
        const std::filesystem::path dest = app_paths::chart_path(context.state->data().meta.chart_id);
        app_paths::ensure_directories();
        if (!save_to_path(context, path_utils::to_utf8(dest), result)) {
            return false;
        }

        const editor_pending_action action = context.save_dialog->action_after_save;
        close_save_dialog(*context.save_dialog);
        result.navigation = navigation_for_action(action);
        return true;
    }

    std::string name = trim_copy(context.save_dialog->file_name_input.value);
    if (name.empty()) {
        context.save_dialog->error = "Enter a file name.";
        context.save_dialog->file_name_input.active = true;
        return false;
    }

    if (name.find_first_of("\\/:*?\"<>|") != std::string::npos) {
        context.save_dialog->error = "File name contains invalid characters.";
        context.save_dialog->file_name_input.active = true;
        return false;
    }

    const std::filesystem::path file_name(name);
    if (file_name.has_parent_path()) {
        context.save_dialog->error = "File name must not contain directories.";
        context.save_dialog->file_name_input.active = true;
        return false;
    }

    std::filesystem::path chart_path = path_utils::from_utf8(context.song->directory) / "charts" / file_name;
    if (chart_path.extension() != ".rchart") {
        chart_path += ".rchart";
    }

    if (!save_to_path(context, path_utils::to_utf8(chart_path), result)) {
        context.save_dialog->file_name_input.active = true;
        return false;
    }

    const editor_pending_action action = context.save_dialog->action_after_save;
    close_save_dialog(*context.save_dialog);
    result.navigation = navigation_for_action(action);
    return true;
}

bool save_chart(const editor_flow_context& context, editor_pending_action action_after_save, editor_flow_result& result) {
    if (context.state == nullptr || context.chart_for_save == nullptr || context.save_dialog == nullptr) {
        return false;
    }

    if (!context.state->file_path().empty()) {
        return save_to_path(context, context.state->file_path(), result);
    }

    open_save_dialog(*context.save_dialog, action_after_save, *context.chart_for_save);
    return false;
}

void request_action(const editor_flow_context& context, editor_pending_action action, editor_flow_result& result) {
    if (action == editor_pending_action::none || context.state == nullptr || context.unsaved_changes_dialog == nullptr) {
        return;
    }

    if (context.state->is_dirty()) {
        context.unsaved_changes_dialog->open = true;
        context.unsaved_changes_dialog->pending = action;
        return;
    }

    result.navigation = navigation_for_action(action);
}

}  // namespace

editor_flow_result editor_flow_controller::update(const editor_flow_context& context) {
    editor_flow_result result;
    if (context.save_dialog == nullptr || context.unsaved_changes_dialog == nullptr || context.metadata_panel == nullptr) {
        return result;
    }

    if (context.save_dialog_submit && context.save_dialog->open) {
        context.save_dialog->submit_requested = false;
        save_chart_from_dialog(context, result);
    }

    if (context.save_dialog->open && (context.escape_pressed || context.save_dialog_cancel)) {
        close_save_dialog(*context.save_dialog);
        result.consume_update = true;
        return result;
    }

    if (context.unsaved_changes_dialog->open && context.escape_pressed) {
        *context.unsaved_changes_dialog = {};
        result.consume_update = true;
        return result;
    }

    if (context.metadata_panel->key_count_confirm_open && context.escape_pressed) {
        context.metadata_panel->key_count_confirm_open = false;
        context.metadata_panel->pending_key_count = context.state
            ? context.state->data().meta.key_count
            : context.metadata_panel->key_count;
        context.metadata_panel->key_count = context.metadata_panel->pending_key_count;
        result.consume_update = true;
        return result;
    }

    if (context.save_dialog->open) {
        result.consume_update = true;
        return result;
    }

    if (context.unsaved_changes_dialog->open) {
        if (context.unsaved_save_clicked) {
            const editor_pending_action action = context.unsaved_changes_dialog->pending;
            *context.unsaved_changes_dialog = {};
            if (context.state != nullptr && context.state->file_path().empty()) {
                open_save_dialog(*context.save_dialog, action, *context.chart_for_save);
            } else if (save_chart(context, editor_pending_action::none, result)) {
                result.navigation = navigation_for_action(action);
            }
        } else if (context.unsaved_discard_clicked) {
            const editor_pending_action action = context.unsaved_changes_dialog->pending;
            *context.unsaved_changes_dialog = {};
            result.navigation = navigation_for_action(action);
        } else if (context.unsaved_cancel_clicked) {
            *context.unsaved_changes_dialog = {};
        }

        result.consume_update = true;
        return result;
    }

    if (context.metadata_panel->key_count_confirm_open) {
        if (context.key_count_confirm_clicked) {
            context.metadata_panel->key_count = context.metadata_panel->pending_key_count;
            result.request_apply_confirmed_key_count = true;
            result.clear_notes_for_key_count_change = true;
        } else if (context.key_count_cancel_clicked) {
            context.metadata_panel->error.clear();
            context.metadata_panel->key_count_confirm_open = false;
            context.metadata_panel->pending_key_count = context.state
                ? context.state->data().meta.key_count
                : context.metadata_panel->key_count;
            context.metadata_panel->key_count = context.metadata_panel->pending_key_count;
        }

        result.consume_update = true;
        return result;
    }

    if (context.escape_pressed || context.back_clicked) {
        request_action(context, editor_pending_action::exit_to_song_select, result);
        if (result.navigation.has_value() || context.unsaved_changes_dialog->open) {
            result.consume_update = true;
        }
        return result;
    }

    const bool editing_blocked = context.has_active_metadata_input ||
        context.has_active_timing_input ||
        context.timing_bar_pick_mode ||
        context.timeline_drag_active;

    if (!editing_blocked && context.f5_pressed) {
        result.navigation = {
            editor_navigation_target::playtest,
            context.shift_pressed ? 0 : context.playback_tick
        };
        result.consume_update = true;
        return result;
    }

    if (context.ctrl_s_pressed) {
        save_chart(context, editor_pending_action::none, result);
        if (context.save_dialog->open) {
            result.consume_update = true;
        }
    }

    return result;
}
