#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

#include "app_paths.h"
#include "chart_fingerprint.h"
#include "chart_serializer.h"
#include "editor/editor_flow_controller.h"
#include "managed_content_storage.h"
#include "path_utils.h"
#include "updater/update_verify.h"

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
    song.directory = path_utils::to_utf8(directory);
    song.meta.song_id = "song";
    song.meta.title = "Song";
    song.meta.audio_file = "audio.mp3";
    return song;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void set_local_app_data_for_test(const std::filesystem::path& path) {
#ifdef _WIN32
    _putenv_s("LOCALAPPDATA", path.string().c_str());
#else
    setenv("LOCALAPPDATA", path.string().c_str(), 1);
#endif
}

std::string text_from_bytes(const std::vector<unsigned char>& bytes) {
    return {bytes.begin(), bytes.end()};
}

editor_flow_context make_context(const song_data& song, const chart_data& chart,
                                 const std::shared_ptr<editor_state>& state,
                                 metadata_panel_state& metadata_panel,
                                 save_dialog_state& save_dialog,
                                 unsaved_changes_dialog_state& unsaved_changes_dialog) {
    editor_flow_context context;
    context.song = &song;
    context.make_chart_data_for_save = [&chart]() { return chart; };
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
    std::filesystem::remove_all(temp_root);
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
        int make_chart_call_count = 0;

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.make_chart_data_for_save = [&]() {
            ++make_chart_call_count;
            return chart;
        };
        const editor_flow_result idle_result = editor_flow_controller::update(context);
        if (idle_result.consume_update || make_chart_call_count != 0) {
            std::cerr << "idle update should not build save chart data\n";
            return EXIT_FAILURE;
        }

        context.ctrl_s_pressed = true;

        const editor_flow_result open_result = editor_flow_controller::update(context);
        if (!open_result.consume_update || !save_dialog.open || make_chart_call_count != 1) {
            std::cerr << "Ctrl+S should open save dialog for unsaved chart\n";
            return EXIT_FAILURE;
        }

        save_dialog.file_name_input.value = "flow-test.rchart";
        context.ctrl_s_pressed = true;
        const editor_flow_result save_result = editor_flow_controller::update(context);
        if (!save_result.saved_chart_path.has_value() ||
            !std::filesystem::exists(*save_result.saved_chart_path) ||
            state->file_path().empty() || save_dialog.open) {
            std::cerr << "Ctrl+S in the save dialog should write the chart and close the dialog\n";
            return EXIT_FAILURE;
        }
    }

    {
        const std::filesystem::path existing_path = temp_root / "charts" / "existing.rchart";
        std::ofstream(existing_path, std::ios::binary) << "old chart contents\n";

        auto state = std::make_shared<editor_state>(chart, path_utils::to_utf8(existing_path));
        state->add_note({note_type::tap, 240, 1, 0});
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;

        chart_data chart_for_save = chart;
        chart_for_save.notes.push_back({note_type::decorative_hold, 960, 1, 1440});

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.make_chart_data_for_save = [&]() {
            return chart_for_save;
        };
        context.ctrl_s_pressed = true;

        const editor_flow_result save_result = editor_flow_controller::update(context);
        const std::string saved_text = read_text(existing_path);
        if (!save_result.saved_chart_path.has_value() ||
            *save_result.saved_chart_path != path_utils::to_utf8(existing_path) ||
            save_dialog.open ||
            saved_text.find("decorativeHold,960,1,1440") == std::string::npos ||
            saved_text.find("formatVersion=5") == std::string::npos) {
            std::cerr << "Ctrl+S should overwrite the existing chart path with current chart data\n";
            return EXIT_FAILURE;
        }
    }

    {
        const std::filesystem::path existing_path = temp_root / "charts" / "unsaved-exit-existing.rchart";
        std::ofstream(existing_path, std::ios::binary) << "old chart contents\n";

        auto state = std::make_shared<editor_state>(chart, path_utils::to_utf8(existing_path));
        state->add_note({note_type::tap, 480, 1, 0});
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;
        unsaved_changes_dialog.open = true;
        unsaved_changes_dialog.pending = editor_pending_action::exit_to_song_select;

        chart_data chart_for_save = chart;
        chart_for_save.notes.push_back({note_type::decorative_hold, 1920, 0, 2400});

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.make_chart_data_for_save = [&]() {
            return chart_for_save;
        };
        context.ctrl_s_pressed = true;

        const editor_flow_result save_result = editor_flow_controller::update(context);
        const std::string saved_text = read_text(existing_path);
        if (!save_result.saved_chart_path.has_value() ||
            save_result.navigation.target != editor_navigation_target::song_select ||
            unsaved_changes_dialog.open ||
            saved_text.find("decorativeHold,1920,0,2400") == std::string::npos) {
            std::cerr << "Ctrl+S in unsaved changes should write existing charts before navigating\n";
            return EXIT_FAILURE;
        }
    }

    {
        auto state = std::make_shared<editor_state>(chart, "");
        state->add_note({note_type::tap, 720, 2, 0});
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;
        unsaved_changes_dialog.open = true;
        unsaved_changes_dialog.pending = editor_pending_action::exit_to_song_select;

        chart_data chart_for_save = chart;
        chart_for_save.notes.push_back({note_type::decorative_hold, 2400, 3, 2880});

        editor_flow_context context = make_context(song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.make_chart_data_for_save = [&]() {
            return chart_for_save;
        };
        context.ctrl_s_pressed = true;

        const editor_flow_result open_result = editor_flow_controller::update(context);
        if (!open_result.consume_update || !save_dialog.open ||
            save_dialog.action_after_save != editor_pending_action::exit_to_song_select ||
            unsaved_changes_dialog.open) {
            std::cerr << "Ctrl+S in unsaved changes should open save dialog for new charts\n";
            return EXIT_FAILURE;
        }

        save_dialog.file_name_input.value = "unsaved-exit-new.rchart";
        context.ctrl_s_pressed = true;

        const editor_flow_result save_result = editor_flow_controller::update(context);
        const std::filesystem::path saved_path = temp_root / "charts" / "unsaved-exit-new.rchart";
        const std::string saved_text = read_text(saved_path);
        if (!save_result.saved_chart_path.has_value() ||
            save_result.navigation.target != editor_navigation_target::song_select ||
            save_dialog.open ||
            saved_text.find("decorativeHold,2400,3,2880") == std::string::npos) {
            std::cerr << "save dialog opened from unsaved changes should save and then navigate\n";
            return EXIT_FAILURE;
        }
    }

    {
        const std::filesystem::path local_app_data = temp_root / "local-app-data";
        set_local_app_data_for_test(local_app_data);

        managed_content_storage::song_identity song_identity;
        song_identity.source = online_content::source::community;
        song_identity.server_url = "https://raythm.test";
        song_identity.remote_song_id = "remote-song";
        song_identity.song_version = 1;
        song_identity.revision_id = "song-revision";

        managed_content_storage::chart_identity chart_identity;
        chart_identity.source = song_identity.source;
        chart_identity.server_url = song_identity.server_url;
        chart_identity.remote_song_id = song_identity.remote_song_id;
        chart_identity.remote_chart_id = "remote-chart";
        chart_identity.song_version = song_identity.song_version;
        chart_identity.chart_version = 1;
        chart_identity.revision_id = "chart-revision";

        const std::filesystem::path managed_song_dir = managed_content_storage::song_directory(song_identity);
        const std::string managed_chart_id = managed_content_storage::local_chart_id(chart_identity);
        const std::filesystem::path managed_chart_path =
            managed_content_storage::chart_file_path(managed_song_dir, managed_chart_id);

        managed_content_storage::package_manifest manifest;
        manifest.song = song_identity;
        manifest.local_song_id = managed_content_storage::local_song_id(song_identity);
        managed_content_storage::upsert_chart(manifest, chart_identity);
        managed_content_storage::chart_manifest_entry* manifest_chart = nullptr;
        for (managed_content_storage::chart_manifest_entry& entry : manifest.charts) {
            if (entry.local_chart_id == managed_chart_id) {
                manifest_chart = &entry;
                break;
            }
        }
        if (manifest_chart == nullptr) {
            std::cerr << "managed chart manifest setup failed\n";
            return EXIT_FAILURE;
        }

        chart_data encrypted_chart = chart;
        encrypted_chart.meta.chart_id = managed_chart_id;
        const std::string initial_chart_text = chart_serializer::serialize_to_string(encrypted_chart);
        std::string error_message;
        if (!managed_content_storage::write_encrypted_asset(
                manifest,
                managed_song_dir,
                path_utils::to_utf8(std::filesystem::path("charts") / (managed_chart_id + ".rchart")),
                initial_chart_text,
                manifest_chart->encrypted_chart,
                error_message) ||
            !managed_content_storage::write_manifest(manifest, error_message)) {
            std::cerr << "managed chart setup failed: " << error_message << "\n";
            return EXIT_FAILURE;
        }

        song_data managed_song = make_song(managed_song_dir);
        auto state = std::make_shared<editor_state>(encrypted_chart, path_utils::to_utf8(managed_chart_path));
        state->add_note({note_type::decorative_hold, 4320, 1, 4800});
        chart_meta renamed_meta = state->data().meta;
        renamed_meta.chart_id = "renamed-managed-chart";
        state->modify_metadata(renamed_meta);
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;

        editor_flow_context context =
            make_context(managed_song, encrypted_chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.make_chart_data_for_save = [&]() {
            return state->data();
        };
        context.ctrl_s_pressed = true;

        const editor_flow_result save_result = editor_flow_controller::update(context);
        const managed_content_storage::managed_file_read_result read_result =
            managed_content_storage::read_managed_file(managed_chart_path);
        const std::string saved_text = text_from_bytes(read_result.bytes);
        const std::optional<managed_content_storage::package_manifest> saved_manifest =
            managed_content_storage::read_manifest(managed_song_dir);
        const managed_content_storage::chart_manifest_entry* saved_manifest_chart = nullptr;
        if (saved_manifest.has_value()) {
            for (const managed_content_storage::chart_manifest_entry& entry : saved_manifest->charts) {
                if (entry.local_chart_id == managed_chart_id) {
                    saved_manifest_chart = &entry;
                    break;
                }
            }
        }
        const std::string expected_chart_hash = updater::compute_sha256_hex(std::string_view(saved_text));
        const std::string expected_chart_fingerprint =
            updater::compute_sha256_hex(std::string_view(chart_fingerprint::build(saved_text)));
        if (!save_result.saved_chart_path.has_value() ||
            !read_result.managed ||
            !read_result.success ||
            saved_text.find("decorativeHold,4320,1,4800") == std::string::npos ||
            saved_text.find("chartId=" + managed_chart_id) == std::string::npos ||
            saved_text.find("chartId=renamed-managed-chart") != std::string::npos ||
            saved_manifest_chart == nullptr ||
            saved_manifest_chart->chart_hash != expected_chart_hash ||
            saved_manifest_chart->chart_fingerprint != expected_chart_fingerprint ||
            std::filesystem::exists(managed_chart_path)) {
            std::cerr << "Ctrl+S should update encrypted managed chart assets\n";
            return EXIT_FAILURE;
        }
    }

    {
        const std::filesystem::path local_app_data = temp_root / "local-app-data";
        set_local_app_data_for_test(local_app_data);

        song_data appdata_song = make_song(app_paths::song_dir("song"));
        chart_data state_chart = chart;
        state_chart.meta.chart_id = "immediate-chart-id";
        auto state = std::make_shared<editor_state>(state_chart, "");
        state->add_note({note_type::decorative_hold, 3360, 0, 3840});
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        unsaved_changes_dialog_state unsaved_changes_dialog;

        editor_flow_context context =
            make_context(appdata_song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.make_chart_data_for_save = [&]() {
            return state->data();
        };
        context.ctrl_s_pressed = true;

        const editor_flow_result save_result = editor_flow_controller::update(context);
        const std::filesystem::path expected_path = app_paths::song_chart_path("song", "immediate-chart-id");
        const std::string saved_text = read_text(expected_path);
        if (!save_result.saved_chart_path.has_value() ||
            *save_result.saved_chart_path != path_utils::to_utf8(expected_path) ||
            save_dialog.open ||
            state->file_path() != path_utils::to_utf8(expected_path) ||
            saved_text.find("decorativeHold,3360,0,3840") == std::string::npos) {
            std::cerr << "Ctrl+S should immediately save new AppData charts without a save dialog\n";
            return EXIT_FAILURE;
        }
    }

    {
        const std::filesystem::path local_app_data = temp_root / "local-app-data";
        set_local_app_data_for_test(local_app_data);

        song_data appdata_song = make_song(app_paths::song_dir("song"));
        chart_data state_chart = chart;
        state_chart.meta.chart_id = "stale-chart-id";
        auto state = std::make_shared<editor_state>(state_chart, "");
        metadata_panel_state metadata_panel;
        save_dialog_state save_dialog;
        save_dialog.open = true;
        unsaved_changes_dialog_state unsaved_changes_dialog;

        chart_data chart_for_save = chart;
        chart_for_save.meta.chart_id = "fresh-chart-id";
        chart_for_save.notes.push_back({note_type::decorative_hold, 1440, 2, 1920});

        editor_flow_context context =
            make_context(appdata_song, chart, state, metadata_panel, save_dialog, unsaved_changes_dialog);
        context.make_chart_data_for_save = [&]() {
            return chart_for_save;
        };
        context.save_dialog_submit = true;

        const editor_flow_result save_result = editor_flow_controller::update(context);
        const std::filesystem::path expected_path = app_paths::song_chart_path("song", "fresh-chart-id");
        const std::filesystem::path stale_path = app_paths::song_chart_path("song", "stale-chart-id");
        const std::string saved_text = read_text(expected_path);
        if (!save_result.saved_chart_path.has_value() ||
            *save_result.saved_chart_path != path_utils::to_utf8(expected_path) ||
            !std::filesystem::exists(expected_path) ||
            std::filesystem::exists(stale_path) ||
            save_dialog.open ||
            state->data().meta.chart_id != "fresh-chart-id" ||
            saved_text.find("chartId=fresh-chart-id") == std::string::npos ||
            saved_text.find("decorativeHold,1440,2,1920") == std::string::npos) {
            std::cerr << "AppData save should use the chart id from the chart data being saved\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "editor_flow_controller smoke test passed\n";
    return EXIT_SUCCESS;
}
