#include "editor_session_loader.h"

#include <algorithm>
#include <filesystem>

#include "app_paths.h"
#include "audio_manager.h"
#include "audio_waveform.h"
#include "chart_parser.h"
#include "editor/service/editor_chart_identity_service.h"
#include "editor_scene_sync.h"
#include "editor_transport_controller.h"
#include "game_settings.h"
#include "path_utils.h"

namespace {

chart_data make_new_chart_data(const editor_start_request& request) {
    chart_data data;
    if (request.initial_meta.has_value()) {
        data.meta = *request.initial_meta;
    } else {
        data.meta.difficulty = "New";
        data.meta.chart_id = editor_chart_identity_service::generated_chart_id(request.song, data.meta.difficulty);
        data.meta.key_count = request.new_chart_key_count;
        data.meta.level = 1.0f;
        data.meta.chart_author = "Unknown";
        data.meta.format_version = 1;
        data.meta.resolution = 480;
        data.meta.offset = 0;
    }
    data.meta.song_id = request.song.meta.song_id;
    if (!request.song.meta.timing_events.empty()) {
        data.timing_events = request.song.meta.timing_events;
        data.meta.resolution = 480;
    } else {
        data.timing_events = {
            {timing_event_type::bpm, 0, std::max(request.song.meta.base_bpm, 120.0f), 4, 4},
            {timing_event_type::meter, 0, 0.0f, 4, 4},
        };
    }
    if (request.song.meta.has_offset) {
        data.meta.offset = request.song.meta.offset;
    }
    return data;
}

void apply_song_timing_to_chart(const song_data& song, chart_data& chart) {
    if (!song.meta.timing_events.empty()) {
        chart.timing_events = song.meta.timing_events;
        chart.meta.resolution = 480;
    }
    if (song.meta.has_offset) {
        chart.meta.offset = song.meta.offset;
    }
}

void scroll_timing_list_to_bottom(editor_timing_panel_state& timing_panel, size_t count) {
    constexpr float kTimingRowHeight = 30.0f;
    constexpr float kTimingRowGap = 4.0f;
    constexpr float kTimingListViewportHeight = 174.0f;

    const float content_height = count == 0
        ? kTimingListViewportHeight
        : static_cast<float>(count) * kTimingRowHeight +
            static_cast<float>(std::max<int>(0, static_cast<int>(count) - 1)) * kTimingRowGap;
    timing_panel.list_scroll_offset = std::max(0.0f, content_height - kTimingListViewportHeight);
}

std::string first_existing_audio_asset(std::initializer_list<const char*> file_names) {
    for (const char* file_name : file_names) {
        const std::filesystem::path path = app_paths::audio_root() / file_name;
        if (std::filesystem::exists(path)) {
            return path_utils::to_utf8(path);
        }
    }
    return "";
}

std::string audio_asset_path(const char* file_name) {
    const std::filesystem::path path = app_paths::audio_root() / file_name;
    return std::filesystem::exists(path) ? path_utils::to_utf8(path) : "";
}

editor_hitsound_paths load_hitsound_paths() {
    editor_hitsound_paths hitsounds;
    hitsounds.tap = audio_asset_path("HitSound_Tap.mp3");
    hitsounds.ray_tap = audio_asset_path("HitSound_RayTap.mp3");
    hitsounds.release = audio_asset_path("HitSound_Release.mp3");
    hitsounds.ray_release = audio_asset_path("HitSound_RayRelease.mp3");
    hitsounds.stay = audio_asset_path("HitSound_Stay.mp3");
    hitsounds.ray_stay = audio_asset_path("HitSound_RayStay.mp3");
    if (hitsounds.tap.empty()) {
        hitsounds.tap = first_existing_audio_asset({"hitsound.mp3"});
    }
    return hitsounds;
}

void preload_hitsounds(audio_manager& audio, const editor_hitsound_paths& hitsounds) {
    const std::string* paths[] = {
        &hitsounds.tap,
        &hitsounds.ray_tap,
        &hitsounds.release,
        &hitsounds.ray_release,
        &hitsounds.stay,
        &hitsounds.ray_stay,
    };
    for (const std::string* path : paths) {
        if (path != nullptr && !path->empty()) {
            audio.preload_se(*path);
        }
    }
}

}  // namespace

namespace editor_session_loader {

editor_session_load_result load(const editor_start_request& request) {
    editor_session_load_result result;
    result.state = request.state ? request.state : std::make_shared<editor_state>();
    result.save_dialog = {};
    result.unsaved_changes_dialog = {};
    result.waveform_visible = request.resume_state.has_value() ? request.resume_state->waveform_visible : true;
    result.waveform_offset_ms = 0;
    result.ticks_per_pixel = request.resume_state.has_value() ? request.resume_state->ticks_per_pixel : 2.0f;
    result.snap_index = request.resume_state.has_value() ? request.resume_state->snap_index : 4;
    result.selected_note_index = request.resume_state.has_value() ? request.resume_state->selected_note_index : std::nullopt;

    if (request.resume_state.has_value()) {
        result.chart_path = result.state->file_path().empty()
            ? std::nullopt
            : std::optional<std::string>(result.state->file_path());
    } else if (request.chart_path.has_value()) {
        const chart_parse_result parse_result = chart_parser::parse(*request.chart_path);
        if (parse_result.success && parse_result.data.has_value()) {
            chart_data loaded_chart = *parse_result.data;
            loaded_chart.meta.song_id = request.song.meta.song_id;
            apply_song_timing_to_chart(request.song, loaded_chart);
            result.state->load(loaded_chart, *request.chart_path);
            result.chart_path = request.chart_path;
        } else {
            result.state->load(make_new_chart_data(request), "");
            result.load_errors = parse_result.errors;
        }
    } else {
        result.state->load(make_new_chart_data(request), "");
    }

    result.bottom_tick = request.resume_state.has_value() ? request.resume_state->bottom_tick : 0.0f;
    result.bottom_tick_target = request.resume_state.has_value() ? request.resume_state->bottom_tick_target : result.bottom_tick;

    result.timing_panel.selected_event_index = result.state->data().timing_events.empty()
        ? std::nullopt
        : std::optional<size_t>(0);
    result.timing_panel.active_input_field = editor_timing_input_field::none;
    result.timing_panel.input_error.clear();
    result.timing_panel.bar_pick_mode = false;
    result.timing_panel.list_scroll_offset = 0.0f;
    result.timing_panel.list_scrollbar_dragging = false;
    result.timing_panel.list_scrollbar_drag_offset = 0.0f;
    scroll_timing_list_to_bottom(result.timing_panel, result.state->data().timing_events.size());

    result.meter_map.rebuild(result.state->data());

    editor_scene_sync_context sync_context{
        *result.state,
        result.meter_map,
        result.timing_panel,
        result.metadata_panel,
        result.selected_note_index,
    };
    editor_scene_sync::sync_metadata_inputs(sync_context);
    editor_scene_sync::load_timing_event_inputs(sync_context);

    audio_manager& audio = audio_manager::instance();
    const std::filesystem::path audio_path = path_utils::join_utf8(request.song.directory, request.song.meta.audio_file);
    if (std::filesystem::exists(audio_path) && audio.load_bgm(path_utils::to_utf8(audio_path))) {
        audio.set_bgm_volume(g_settings.bgm_volume);
        result.audio_loaded = true;
    }

    if (std::filesystem::exists(audio_path)) {
        result.waveform_summary = audio_waveform::build(path_utils::to_utf8(audio_path));
        if (result.waveform_summary.length_seconds > 0.0) {
            result.audio_length_tick = std::max(
                result.audio_length_tick,
                result.state->engine().ms_to_tick(result.waveform_summary.length_seconds * 1000.0));
        }
    }

    result.hitsounds = load_hitsound_paths();
    result.hitsound_path = result.hitsounds.tap;
    preload_hitsounds(audio, result.hitsounds);

    if (request.resume_state.has_value() && result.audio_loaded) {
        const double target_seconds =
            result.state->engine().tick_to_ms(std::max(0, request.resume_state->playback_tick)) / 1000.0;
        audio.seek_bgm(target_seconds);
    }

    editor_transport_context transport_context;
    transport_context.state = result.state.get();
    transport_context.audio_loaded = result.audio_loaded;
    transport_context.previous_playback_tick = 0;
    transport_context.previous_audio_playing = false;
    transport_context.hitsound_path = &result.hitsound_path;
    transport_context.hitsounds = &result.hitsounds;
    if (result.audio_loaded && audio.is_bgm_loaded()) {
        transport_context.bgm_clock = audio.get_bgm_clock();
        transport_context.bgm_length_seconds = audio.get_bgm_length_seconds();
    }

    const editor_transport_result transport_result = editor_transport_controller::sync(transport_context);
    result.audio_loaded = transport_result.audio_loaded;
    result.audio_playing = transport_result.audio_playing;
    result.audio_time_seconds = transport_result.audio_time_seconds;
    result.playback_tick = transport_result.playback_tick;
    result.previous_playback_tick = transport_result.previous_playback_tick;
    result.previous_audio_playing = transport_result.previous_audio_playing;
    result.audio_length_tick = std::max(result.audio_length_tick, transport_result.audio_length_tick);

    if (!request.resume_state.has_value()) {
        result.bottom_tick = 0.0f;
        result.bottom_tick_target = 0.0f;
    }

    return result;
}

}  // namespace editor_session_loader
