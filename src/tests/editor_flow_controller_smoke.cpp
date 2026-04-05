#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

#include "editor/editor_flow_controller.h"

namespace {

chart_data make_chart() {
    chart_data data;
    data.meta.chart_id = "editor-flow-smoke";
    data.meta.song_id = "song";
    data.meta.key_count = 4;
    data.meta.difficulty = "Normal";
    data.meta.level = 5;
    data.meta.chart_author = "Codex";
    data.meta.format_version = 1;
    data.meta.resolution = 480;
    data.meta.offset = 0;
    data.timing_events = {
        {timing_event_type::bpm, 0, 120.0f, 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    data.notes = {
        {note_type::tap, 0, 0, 0},
    };
    return data;
}

song_data make_song(const std::filesystem::path& directory) {
    song_data song;
    song.directory = directory.string();
    song.meta.song_id = "song";
    song.meta.title = "Song";
    song.meta.audio_file = "audio.mp3";
    return song;
}

editor_flow_context make_context(const song_data& song, const chart_data& chart,
                                 const std::shared_ptr<editor_state>& state,
                                 metadata_panel_state& metadata_panel,
                                 save_dialog_state& save_dialog,
                                 unsaved_changes_dialog_state& unsaved_changes_dialog) {
    editor_flow_context context;
    context.song = &song;
    context.chart_for_save = &chart;
    context.state = state;
    context.metadata_panel = &metadata_panel;
    context.save_dialog = &save_dialog;
    context.unsaved_changes_dialog = &unsaved_changes_dialog;
    return context;
}

}  // namespace

int main() {
    const std::filesystem::path temp_root =
        std::filesystem::temp_directory_path() / "raythm-editor-flow-smoke";
    std::filesystem::create_directories(temp_root / "charts");

    const chart_data chart = make_chart();
    const song_data song = make_song(temp_root);

    {
        auto state = std::make_shared<editor_state>(chart, "");
        state->add_note({note_type::tap, 240, 1, 240});
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.escape_pressed = true;

        const editor_flow_result result = editor_flow_controller::update(context);
        if (!result.consume_update || !unsaved_changes_dialog.open ||
            unsaved_changes_dialog.pending != editor_pending_action::exit_to_song_select) {
            std::cerr << "escape should open unsaved changes dialog for dirty state\n";
            return EXIT_FAILURE;
        }
    }

    {
        auto state = std::make_shared<editor_state>(chart, temp_root.string() + "\\charts\\saved.rchart");
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;
        unsaved_changes_dialog.open = true;
        unsaved_changes_dialog.pending = editor_pending_action::exit_to_song_select;

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.unsaved_discard_clicked = true;

        const editor_flow_result result = editor_flow_controller::update(context);
        if (!result.consume_update || result.navigation.target != editor_navigation_target::song_select ||
            unsaved_changes_dialog.open) {
            std::cerr << "discard should close dialog and navigate away\n";
            return EXIT_FAILURE;
        }
    }

    {
        auto state = std::make_shared<editor_state>(chart, "");
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;
        unsaved_changes_dialog.open = true;
        unsaved_changes_dialog.pending = editor_pending_action::exit_to_song_select;

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.unsaved_cancel_clicked = true;

        const editor_flow_result result = editor_flow_controller::update(context);
        if (!result.consume_update || unsaved_changes_dialog.open || result.navigation.has_value()) {
            std::cerr << "cancel should only close the unsaved changes dialog\n";
            return EXIT_FAILURE;
        }
    }

    {
        auto state = std::make_shared<editor_state>(chart, "");
        metadata_panel_state metadata_panel;
        metadata_panel.key_count_confirm_open = true;
        metadata_panel.pending_key_count = 6;
        metadata_panel.key_count = 4;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.key_count_confirm_clicked = true;

        const editor_flow_result result = editor_flow_controller::update(context);
        if (!result.consume_update || !result.request_apply_confirmed_key_count ||
            !result.clear_notes_for_key_count_change || metadata_panel.key_count != 6) {
            std::cerr << "key count confirm should request metadata apply with note clear\n";
            return EXIT_FAILURE;
        }
    }

    {
        auto state = std::make_shared<editor_state>(chart, "");
        metadata_panel_state metadata_panel;
        metadata_panel.key_count_confirm_open = true;
        metadata_panel.pending_key_count = 6;
        metadata_panel.key_count = 6;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.key_count_cancel_clicked = true;

        const editor_flow_result result = editor_flow_controller::update(context);
        if (!result.consume_update || metadata_panel.key_count_confirm_open || metadata_panel.key_count != 4) {
            std::cerr << "key count cancel should revert the pending change\n";
            return EXIT_FAILURE;
        }
    }

    {
        auto state = std::make_shared<editor_state>(chart, "");
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.f5_pressed = true;
        context.has_active_metadata_input = true;
        context.playback_tick = 480;

        const editor_flow_result blocked = editor_flow_controller::update(context);
        if (blocked.navigation.has_value()) {
            std::cerr << "blocked F5 should not start playtest\n";
            return EXIT_FAILURE;
        }

        context.has_active_metadata_input = false;
        const editor_flow_result normal = editor_flow_controller::update(context);
        if (normal.navigation.target != editor_navigation_target::playtest ||
            normal.navigation.playtest_start_tick != 480) {
            std::cerr << "F5 should request playtest from current playback tick\n";
            return EXIT_FAILURE;
        }

        context.shift_pressed = true;
        const editor_flow_result from_start = editor_flow_controller::update(context);
        if (from_start.navigation.playtest_start_tick != 0) {
            std::cerr << "Shift+F5 should request playtest from tick 0\n";
            return EXIT_FAILURE;
        }
    }

    {
        auto state = std::make_shared<editor_state>(chart, "");
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.ctrl_s_pressed = true;

        const editor_flow_result open_result = editor_flow_controller::update(context);
        if (!open_result.consume_update || !save_dialog.open) {
            std::cerr << "Ctrl+S should open save dialog for unsaved chart\n";
            return EXIT_FAILURE;
        }

        save_dialog.file_name_input.value = "flow-test.rchart";
        context.ctrl_s_pressed = false;
        context.save_dialog_submit = true;
        const editor_flow_result save_result = editor_flow_controller::update(context);
        if (!save_result.saved_chart_path.has_value() ||
            !std::filesystem::exists(*save_result.saved_chart_path) ||
            state->file_path().empty() || save_dialog.open) {
            std::cerr << "save dialog submit should write the chart and close the dialog\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "editor_flow_controller smoke test passed\n";
    return EXIT_SUCCESS;
}
