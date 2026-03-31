#include "editor_scene.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <memory>

#include "audio_manager.h"
#include "audio_waveform.h"
#include "chart_parser.h"
#include "chart_serializer.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {
constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kHeaderRect = ui::place(kScreenRect, 1220.0f, 48.0f,
                                            ui::anchor::top_center, ui::anchor::top_center,
                                            {0.0f, 12.0f});
constexpr Rectangle kLeftPanelRect = ui::place(kScreenRect, 248.0f, 620.0f,
                                               ui::anchor::top_left, ui::anchor::top_left,
                                               {20.0f, 72.0f});
constexpr Rectangle kTimelineRect = ui::place(kScreenRect, 724.0f, 620.0f,
                                              ui::anchor::top_center, ui::anchor::top_center,
                                              {0.0f, 72.0f});
constexpr Rectangle kRightPanelRect = ui::place(kScreenRect, 248.0f, 620.0f,
                                                ui::anchor::top_right, ui::anchor::top_right,
                                                {-20.0f, 72.0f});
constexpr Rectangle kBackButtonRect = ui::place(kHeaderRect, 120.0f, 34.0f,
                                                ui::anchor::center_left, ui::anchor::center_left,
                                                {16.0f, 0.0f});
constexpr float kTimelinePadding = 18.0f;
constexpr float kLaneGap = 6.0f;
constexpr float kScrollbarWidth = 10.0f;
constexpr float kScrollbarGap = 10.0f;
constexpr float kMinTicksPerPixel = 0.9f;
constexpr float kMaxTicksPerPixel = 28.0f;
constexpr float kMinVisibleTicks = 1920.0f;
constexpr float kScrollLerpSpeed = 12.0f;
constexpr float kScrollWheelViewportRatio = 0.36f;
constexpr float kNoteHeadHeight = 14.0f;
constexpr float kTimelineLeadInTicks = 960.0f;
constexpr int kSnapDivisions[] = {1, 2, 4, 8, 16, 32};
constexpr const char* kSnapLabels[] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32"};
constexpr Rectangle kHeaderToolsRect = ui::place(kHeaderRect, 168.0f, 34.0f,
                                                 ui::anchor::center_right, ui::anchor::center_right,
                                                 {-18.0f, 0.0f});
constexpr Rectangle kChartOffsetRect = ui::place(kHeaderRect, 188.0f, 34.0f,
                                                 ui::anchor::center_right, ui::anchor::center_right,
                                                 {-338.0f, 0.0f});
constexpr Rectangle kWaveformToggleRect = ui::place(kHeaderRect, 132.0f, 34.0f,
                                                    ui::anchor::center_right, ui::anchor::center_right,
                                                    {-198.0f, 0.0f});
constexpr Rectangle kPlaybackRect = ui::place(kTimelineRect, 232.0f, 34.0f,
                                              ui::anchor::bottom_left, ui::anchor::bottom_left,
                                              {12.0f, -54.0f});
constexpr float kDropdownItemHeight = 30.0f;
constexpr float kDropdownItemSpacing = 4.0f;
constexpr Rectangle kMetadataConfirmRect = ui::place(kScreenRect, 420.0f, 196.0f,
                                                     ui::anchor::center, ui::anchor::center);
constexpr Rectangle kUnsavedChangesRect = ui::place(kScreenRect, 456.0f, 210.0f,
                                                    ui::anchor::center, ui::anchor::center);
constexpr Rectangle kSaveDialogRect = ui::place(kScreenRect, 520.0f, 224.0f,
                                                ui::anchor::center, ui::anchor::center);
constexpr Rectangle kSnapDropdownRect = kHeaderToolsRect;
constexpr Rectangle kSnapDropdownMenuRect = {
    kSnapDropdownRect.x,
    kSnapDropdownRect.y + kSnapDropdownRect.height + 4.0f,
    kSnapDropdownRect.width,
    12.0f + static_cast<float>(std::size(kSnapLabels)) * kDropdownItemHeight +
        static_cast<float>(std::size(kSnapLabels) - 1) * kDropdownItemSpacing
};
constexpr float kPlaybackFollowViewportRatio = 0.35f;
constexpr float kPlaybackRestartEpsilonSeconds = 0.01f;

const char* note_type_label(note_type type) {
    return type == note_type::hold ? "Hold" : "Tap";
}

const char* key_count_label(int key_count) {
    return key_count == 6 ? "6K" : "4K";
}

editor_timeline_note make_timeline_note(const note_data& note) {
    return {
        note.type == note_type::hold ? editor_timeline_note_type::hold : editor_timeline_note_type::tap,
        note.tick,
        note.lane,
        note.end_tick
    };
}

const char* timing_event_type_label(timing_event_type type) {
    return type == timing_event_type::bpm ? "BPM" : "Meter";
}

bool timing_event_sort_less(const timing_event& left, size_t left_index,
                            const timing_event& right, size_t right_index) {
    if (left.tick != right.tick) {
        return left.tick < right.tick;
    }
    if (left.type != right.type) {
        return left.type == timing_event_type::bpm;
    }
    return left_index < right_index;
}

bool accepts_metadata_character(int codepoint, const std::string&) {
    return codepoint >= 32 && codepoint <= 126;
}

bool accepts_chart_file_character(int codepoint, const std::string&) {
    return (codepoint >= 'a' && codepoint <= 'z') ||
           (codepoint >= 'A' && codepoint <= 'Z') ||
           (codepoint >= '0' && codepoint <= '9') ||
           codepoint == '-' ||
           codepoint == '_' ||
           codepoint == '.';
}

bool try_parse_int(const std::string& text, int& out_value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const int value = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        out_value = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool try_parse_float(const std::string& text, float& out_value) {
    if (text.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const float value = std::stof(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        out_value = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool try_parse_bar_beat(const std::string& text, editor_meter_map::bar_beat_position& out_value) {
    if (text.empty()) {
        return false;
    }

    const size_t colon = text.find(':');
    int measure = 0;
    int beat = 1;
    if (colon == std::string::npos) {
        if (!try_parse_int(text, measure)) {
            return false;
        }
    } else {
        if (!try_parse_int(text.substr(0, colon), measure) ||
            !try_parse_int(text.substr(colon + 1), beat)) {
            return false;
        }
    }

    if (measure <= 0 || beat <= 0) {
        return false;
    }

    out_value = {measure, beat};
    return true;
}

std::string format_playback_time(double seconds) {
    const int total_ms = std::max(0, static_cast<int>(std::lround(seconds * 1000.0)));
    const int minutes = total_ms / 60000;
    const int whole_seconds = (total_ms / 1000) % 60;
    const int centiseconds = (total_ms % 1000) / 10;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d.%02d", minutes, whole_seconds, centiseconds);
    return buffer;
}

std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

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

}

editor_scene::editor_scene(scene_manager& manager, song_data song, std::string chart_path)
    : scene(manager), song_(std::move(song)), chart_path_(std::move(chart_path)) {
}

editor_scene::editor_scene(scene_manager& manager, song_data song, int key_count)
    : scene(manager), song_(std::move(song)), chart_path_(std::nullopt), new_chart_key_count_(key_count) {
}

void editor_scene::on_enter() {
    load_errors_.clear();
    audio_length_tick_ = 0;
    audio_loaded_ = false;
    audio_playing_ = false;
    audio_time_seconds_ = 0.0;
    playback_tick_ = 0;
    previous_playback_tick_ = 0;
    previous_audio_playing_ = false;
    hitsound_path_.clear();
    waveform_visible_ = true;
    waveform_offset_ms_ = 0;
    waveform_summary_ = {};
    waveform_samples_.clear();
    save_dialog_ = {};
    unsaved_changes_dialog_ = {};

    if (chart_path_.has_value()) {
        const chart_parse_result result = chart_parser::parse(*chart_path_);
        if (result.success && result.data.has_value()) {
            state_.load(*result.data, *chart_path_);
        } else {
            state_.load(make_new_chart_data(), "");
            chart_path_.reset();
            load_errors_ = result.errors;
        }
    } else {
        state_.load(make_new_chart_data(), "");
    }

    bottom_tick_ = min_bottom_tick();
    bottom_tick_target_ = bottom_tick_;
    ticks_per_pixel_ = 2.0f;
    snap_index_ = 4;
    snap_dropdown_open_ = false;
    selected_note_index_.reset();
    timing_panel_.selected_event_index = state_.data().timing_events.empty() ? std::nullopt : std::optional<size_t>(0);
    timing_panel_.active_input_field = editor_timing_input_field::none;
    timing_panel_.input_error.clear();
    timing_panel_.bar_pick_mode = false;
    timing_panel_.list_scroll_offset = 0.0f;
    timing_panel_.list_scrollbar_dragging = false;
    timing_panel_.list_scrollbar_drag_offset = 0.0f;
    timeline_drag_ = {};
    sync_metadata_inputs();
    meter_map_.rebuild(state_.data());
    load_timing_event_inputs();
    scroll_timing_list_to_bottom();

    const std::filesystem::path audio_path = std::filesystem::path(song_.directory) / song_.meta.audio_file;
    if (std::filesystem::exists(audio_path) &&
        audio_manager::instance().load_bgm(audio_path.string())) {
        audio_loaded_ = true;
        refresh_audio_length_tick();
    }
    rebuild_waveform_data(audio_path.string());

    const std::filesystem::path hitsound_path = repo_root() / "assets" / "audio" / "hitsound.mp3";
    hitsound_path_ = std::filesystem::exists(hitsound_path) ? hitsound_path.string() : "";

    update_audio_clock();
    previous_playback_tick_ = playback_tick_;
}

void editor_scene::on_exit() {
    audio_manager::instance().stop_bgm();
    audio_manager::instance().stop_all_se();
}

void editor_scene::update(float dt) {
    rebuild_hit_regions();
    update_audio_clock();

    if (save_dialog_.submit_requested) {
        save_dialog_.submit_requested = false;
        save_chart_from_dialog();
    }

    if (save_dialog_.open && IsKeyPressed(KEY_ESCAPE)) {
        close_save_dialog();
        return;
    }

    if (unsaved_changes_dialog_.open && IsKeyPressed(KEY_ESCAPE)) {
        unsaved_changes_dialog_ = {};
        return;
    }

    if (metadata_panel_.key_count_confirm_open && IsKeyPressed(KEY_ESCAPE)) {
        close_key_count_confirmation();
        return;
    }

    if (save_dialog_.open) {
        return;
    }

    if (unsaved_changes_dialog_.open) {
        update_unsaved_changes_dialog();
        return;
    }

    if (metadata_panel_.key_count_confirm_open) {
        update_key_count_confirmation();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        request_action(pending_action::exit_to_song_select);
        return;
    }

    if (ui::is_clicked(kBackButtonRect)) {
        request_action(pending_action::exit_to_song_select);
        return;
    }

    handle_shortcuts();
    handle_text_input();
    handle_timeline_interaction();
    update_audio_clock();
    update_note_hitsounds();
    apply_scroll_and_zoom(dt);
}

void editor_scene::rebuild_hit_regions() const {
    ui::begin_hit_regions();
    if (snap_dropdown_open_) {
        ui::register_hit_region(kSnapDropdownMenuRect, ui::draw_layer::overlay);
    }
    if (save_dialog_.open) {
        ui::register_hit_region(kScreenRect, ui::draw_layer::overlay);
        ui::register_hit_region(kSaveDialogRect, ui::draw_layer::modal);
    }
    if (unsaved_changes_dialog_.open) {
        ui::register_hit_region(kScreenRect, ui::draw_layer::overlay);
        ui::register_hit_region(kUnsavedChangesRect, ui::draw_layer::modal);
    }
    if (metadata_panel_.key_count_confirm_open) {
        ui::register_hit_region(kScreenRect, ui::draw_layer::overlay);
        ui::register_hit_region(kMetadataConfirmRect, ui::draw_layer::modal);
    }
}

void editor_scene::draw() {
    const auto& t = *g_theme;
    const double now = GetTime();
    virtual_screen::begin();
    rebuild_hit_regions();
    ui::begin_draw_queue();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);

    ui::draw_panel(kLeftPanelRect);
    ui::draw_panel(kTimelineRect);
    ui::draw_panel(kRightPanelRect);
    ui::draw_panel(kHeaderRect);

    ui::draw_button_colored(kBackButtonRect, "BACK", 20, t.row, t.row_hover, t.text);

    draw_left_panel();
    draw_timeline();
    draw_right_panel();
    draw_header_tools();
    draw_cursor_hud();
    if (unsaved_changes_dialog_.open) {
        draw_unsaved_changes_dialog();
    }
    if (save_dialog_.open) {
        draw_save_dialog();
    }
    if (metadata_panel_.key_count_confirm_open) {
        draw_key_count_confirmation();
    }

    ui::flush_draw_queue();
    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

chart_data editor_scene::make_new_chart_data() const {
    chart_data data;
    data.meta.difficulty = "New";
    data.meta.chart_id = generated_chart_id(data.meta.difficulty);
    data.meta.key_count = new_chart_key_count_;
    data.meta.level = 1;
    data.meta.chart_author = "Unknown";
    data.meta.format_version = 1;
    data.meta.resolution = 480;
    data.meta.offset = 0;
    data.timing_events = {
        {timing_event_type::bpm, 0, std::max(song_.meta.base_bpm, 120.0f), 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    return data;
}

chart_data editor_scene::make_chart_data_for_save() const {
    chart_data data = state_.data();
    if (state_.file_path().empty()) {
        data.meta.chart_id = generated_chart_id(data.meta.difficulty);
    }
    return data;
}

std::string editor_scene::generated_chart_id(const std::string& difficulty) const {
    const std::string song_id = slugify(song_.meta.song_id);
    const std::string difficulty_slug = slugify(difficulty);
    if (!song_id.empty() && !difficulty_slug.empty()) {
        return song_id + "-" + difficulty_slug;
    }
    if (!song_id.empty()) {
        return song_id + "-chart";
    }
    if (!difficulty_slug.empty()) {
        return "chart-" + difficulty_slug;
    }
    return "new-chart";
}

std::string editor_scene::suggested_chart_file_name() const {
    std::string stem = slugify(state_.data().meta.difficulty);
    if (stem.empty()) {
        stem = slugify(state_.data().meta.chart_id);
    }
    if (stem.empty()) {
        stem = "new-chart";
    }
    return stem + ".chart";
}

void editor_scene::open_save_dialog(pending_action action_after_save) {
    save_dialog_.open = true;
    save_dialog_.submit_requested = false;
    save_dialog_.action_after_save = action_after_save;
    save_dialog_.error.clear();
    if (save_dialog_.file_name_input.value.empty()) {
        save_dialog_.file_name_input.value = suggested_chart_file_name();
    }
    save_dialog_.file_name_input.active = false;
}

void editor_scene::close_save_dialog() {
    save_dialog_.open = false;
    save_dialog_.submit_requested = false;
    save_dialog_.action_after_save = pending_action::none;
    save_dialog_.file_name_input.active = false;
}

bool editor_scene::save_to_path(const std::string& file_path) {
    const chart_data data = make_chart_data_for_save();
    const std::filesystem::path path(file_path);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        save_dialog_.error = "Failed to prepare the charts directory.";
        return false;
    }

    if (!chart_serializer::serialize(data, path.string())) {
        save_dialog_.error = "Failed to save the chart file.";
        return false;
    }

    state_.mark_saved(path.string());
    chart_path_ = path.string();
    save_dialog_.error.clear();
    return true;
}

bool editor_scene::save_chart() {
    if (!state_.file_path().empty()) {
        return save_to_path(state_.file_path());
    }

    open_save_dialog();
    return false;
}

bool editor_scene::save_chart_from_dialog() {
    std::string name = trim_copy(save_dialog_.file_name_input.value);
    if (name.empty()) {
        save_dialog_.error = "Enter a file name.";
        save_dialog_.file_name_input.active = true;
        return false;
    }

    if (name.find_first_of("\\/:*?\"<>|") != std::string::npos) {
        save_dialog_.error = "File name contains invalid characters.";
        save_dialog_.file_name_input.active = true;
        return false;
    }

    const std::filesystem::path file_name(name);
    if (file_name.has_parent_path()) {
        save_dialog_.error = "File name must not contain directories.";
        save_dialog_.file_name_input.active = true;
        return false;
    }

    std::filesystem::path chart_path = std::filesystem::path(song_.directory) / "charts" / file_name;
    if (chart_path.extension() != ".chart") {
        chart_path += ".chart";
    }

    if (!save_to_path(chart_path.string())) {
        save_dialog_.file_name_input.active = true;
        return false;
    }

    const pending_action action = save_dialog_.action_after_save;
    close_save_dialog();
    if (action != pending_action::none) {
        perform_action(action);
    }
    return true;
}

void editor_scene::request_action(pending_action action) {
    if (action == pending_action::none) {
        return;
    }

    if (state_.is_dirty()) {
        unsaved_changes_dialog_.open = true;
        unsaved_changes_dialog_.pending = action;
        return;
    }

    perform_action(action);
}

void editor_scene::perform_action(pending_action action) {
    switch (action) {
        case pending_action::none:
            return;
        case pending_action::exit_to_song_select:
            manager_.change_scene(std::make_unique<song_select_scene>(manager_));
            return;
    }
}

bool editor_scene::has_blocking_modal() const {
    return metadata_panel_.key_count_confirm_open || save_dialog_.open || unsaved_changes_dialog_.open;
}

std::optional<note_data> editor_scene::dragged_note() const {
    if (!timeline_drag_.active) {
        return std::nullopt;
    }

    note_data note;
    note.lane = timeline_drag_.lane;
    note.tick = std::min(timeline_drag_.start_tick, timeline_drag_.current_tick);
    note.end_tick = std::max(timeline_drag_.start_tick, timeline_drag_.current_tick);
    note.type = (note.end_tick - note.tick) >= snap_interval() ? note_type::hold : note_type::tap;
    if (note.type == note_type::tap) {
        note.end_tick = note.tick;
    }

    return note;
}

std::vector<size_t> editor_scene::sorted_timing_event_indices() const {
    std::vector<size_t> indices(state_.data().timing_events.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }

    std::stable_sort(indices.begin(), indices.end(), [this](size_t left_index, size_t right_index) {
        const timing_event& left = state_.data().timing_events[left_index];
        const timing_event& right = state_.data().timing_events[right_index];
        return timing_event_sort_less(right, right_index, left, left_index);
    });

    return indices;
}

editor_timeline_metrics editor_scene::timeline_metrics() const {
    return {
        kTimelineRect,
        kTimelinePadding,
        kScrollbarGap,
        kScrollbarWidth,
        kLaneGap,
        kNoteHeadHeight,
        bottom_tick_,
        ticks_per_pixel_,
        state_.data().meta.key_count
    };
}

float editor_scene::visible_tick_span() const {
    return timeline_metrics().visible_tick_span();
}

float editor_scene::content_tick_span() const {
    int max_tick = state_.data().meta.resolution * 8;
    for (const note_data& note : state_.data().notes) {
        max_tick = std::max(max_tick, note.type == note_type::hold ? note.end_tick : note.tick);
    }
    for (const timing_event& event : state_.data().timing_events) {
        max_tick = std::max(max_tick, event.tick);
    }
    max_tick = std::max(max_tick, audio_length_tick_);

    return std::max(visible_tick_span(), static_cast<float>(max_tick) + state_.data().meta.resolution * 4.0f);
}

float editor_scene::content_height_pixels() const {
    return (content_tick_span() - min_bottom_tick()) / ticks_per_pixel_;
}

float editor_scene::scroll_offset_pixels() const {
    return (max_bottom_tick() - bottom_tick_) / ticks_per_pixel_;
}

float editor_scene::min_bottom_tick() const {
    return -kTimelineLeadInTicks;
}

float editor_scene::max_bottom_tick() const {
    return std::max(min_bottom_tick(), content_tick_span() - visible_tick_span());
}

int editor_scene::snap_division() const {
    return kSnapDivisions[std::clamp(snap_index_, 0, static_cast<int>(std::size(kSnapDivisions)) - 1)];
}

int editor_scene::snap_interval() const {
    return std::max(1, state_.data().meta.resolution * 4 / snap_division());
}

int editor_scene::snap_tick(int raw_tick) const {
    return std::max(0, state_.snap_tick(std::max(0, raw_tick), snap_division()));
}

int editor_scene::default_timing_event_tick() const {
    if (timing_panel_.selected_event_index.has_value() &&
        *timing_panel_.selected_event_index < state_.data().timing_events.size()) {
        return snap_tick(state_.data().timing_events[*timing_panel_.selected_event_index].tick + snap_interval());
    }
    return std::max(snap_interval(), snap_tick(static_cast<int>(bottom_tick_ + visible_tick_span() * 0.5f)));
}

void editor_scene::update_audio_clock() {
    if (!audio_loaded_ || !audio_manager::instance().is_bgm_loaded()) {
        audio_loaded_ = false;
        audio_playing_ = false;
        audio_time_seconds_ = 0.0;
        playback_tick_ = 0;
        return;
    }

    const audio_clock_snapshot bgm_clock = audio_manager::instance().get_bgm_clock();
    audio_playing_ = bgm_clock.playing;
    double seconds = audio_playing_ ? bgm_clock.audio_time_seconds : bgm_clock.stream_position_seconds;
    const double length_seconds = audio_manager::instance().get_bgm_length_seconds();
    if (length_seconds > 0.0) {
        seconds = std::clamp(seconds, 0.0, length_seconds);
    } else {
        seconds = std::max(0.0, seconds);
    }

    audio_time_seconds_ = seconds;
    playback_tick_ = state_.engine().ms_to_tick(audio_time_seconds_ * 1000.0);
}

void editor_scene::refresh_audio_length_tick() {
    if (!audio_loaded_ || !audio_manager::instance().is_bgm_loaded()) {
        audio_length_tick_ = 0;
        return;
    }

    const double length_ms = audio_manager::instance().get_bgm_length_seconds() * 1000.0;
    audio_length_tick_ = std::max(0, state_.engine().ms_to_tick(length_ms));
}

void editor_scene::toggle_audio_playback() {
    if (!audio_loaded_) {
        return;
    }

    if (audio_manager::instance().is_bgm_playing()) {
        audio_manager::instance().pause_bgm();
    } else {
        const double length_seconds = audio_manager::instance().get_bgm_length_seconds();
        const double position_seconds = audio_manager::instance().get_bgm_position_seconds();
        const bool restart = length_seconds > 0.0 &&
                             position_seconds >= std::max(0.0, length_seconds - kPlaybackRestartEpsilonSeconds);
        audio_manager::instance().play_bgm(restart);
    }
    update_audio_clock();
    previous_playback_tick_ = playback_tick_;
    previous_audio_playing_ = audio_playing_;
}

void editor_scene::seek_audio_to_tick(int tick, bool scroll_into_view) {
    if (!audio_loaded_) {
        return;
    }

    const int clamped_tick = std::max(0, tick);
    const double target_ms = state_.engine().tick_to_ms(clamped_tick);
    audio_manager::instance().seek_bgm(target_ms / 1000.0);
    update_audio_clock();
    previous_playback_tick_ = playback_tick_;
    previous_audio_playing_ = audio_playing_;

    if (scroll_into_view) {
        scroll_to_tick(playback_tick_);
    }
}

void editor_scene::update_note_hitsounds() {
    if (!audio_loaded_ || hitsound_path_.empty()) {
        previous_playback_tick_ = playback_tick_;
        previous_audio_playing_ = audio_playing_;
        return;
    }

    if (!audio_playing_) {
        previous_playback_tick_ = playback_tick_;
        previous_audio_playing_ = false;
        return;
    }

    if (!previous_audio_playing_) {
        previous_playback_tick_ = playback_tick_;
        previous_audio_playing_ = true;
        return;
    }

    if (playback_tick_ <= previous_playback_tick_) {
        previous_playback_tick_ = playback_tick_;
        previous_audio_playing_ = true;
        return;
    }

    for (const note_data& note : state_.data().notes) {
        if (note.tick > previous_playback_tick_ && note.tick <= playback_tick_) {
            audio_manager::instance().play_se(hitsound_path_, 0.45f);
        }
    }

    previous_playback_tick_ = playback_tick_;
    previous_audio_playing_ = true;
}

void editor_scene::rebuild_waveform_data(const std::string& audio_path) {
    waveform_summary_ = {};
    waveform_samples_.clear();
    if (audio_path.empty() || !std::filesystem::exists(audio_path)) {
        return;
    }

    waveform_summary_ = audio_waveform::build(audio_path);
    rebuild_waveform_samples();
}

void editor_scene::rebuild_waveform_samples() {
    waveform_samples_.clear();
    waveform_samples_.reserve(waveform_summary_.peaks.size());
    for (const audio_waveform_peak& peak : waveform_summary_.peaks) {
        const double shifted_ms = peak.seconds * 1000.0 + static_cast<double>(waveform_offset_ms_);
        waveform_samples_.push_back({
            state_.engine().ms_to_tick(shifted_ms),
            peak.amplitude
        });
    }
}

std::string editor_scene::playback_status_text() const {
    if (!audio_loaded_) {
        return "No audio";
    }

    return std::string(audio_playing_ ? "Playing " : "Paused ") + format_playback_time(audio_time_seconds_);
}

std::optional<int> editor_scene::lane_at_position(Vector2 point) const {
    const editor_timeline_metrics metrics = timeline_metrics();
    const Rectangle content = metrics.content_rect();
    if (!CheckCollisionPointRec(point, content)) {
        return std::nullopt;
    }

    for (int lane = 0; lane < state_.data().meta.key_count; ++lane) {
        if (CheckCollisionPointRec(point, metrics.lane_rect(lane))) {
            return lane;
        }
    }

    return std::nullopt;
}

std::optional<size_t> editor_scene::note_at_position(Vector2 point) const {
    const editor_timeline_metrics metrics = timeline_metrics();
    const Rectangle content = metrics.content_rect();
    if (!CheckCollisionPointRec(point, content)) {
        return std::nullopt;
    }

    for (size_t i = state_.data().notes.size(); i > 0; --i) {
        const size_t index = i - 1;
        const note_data& note = state_.data().notes[index];
        if (note.lane < 0 || note.lane >= state_.data().meta.key_count) {
            continue;
        }

        const editor_timeline_note_draw_info info = metrics.note_rects(make_timeline_note(note));
        if (CheckCollisionPointRec(point, info.head_rect) ||
            (info.has_body && (CheckCollisionPointRec(point, info.body_rect) || CheckCollisionPointRec(point, info.tail_rect)))) {
            return index;
        }
    }

    return std::nullopt;
}

void editor_scene::handle_shortcuts() {
    if (has_blocking_modal()) {
        return;
    }

    if (!has_active_metadata_input() &&
        timing_panel_.active_input_field == editor_timing_input_field::none &&
        !timing_panel_.bar_pick_mode &&
        !timeline_drag_.active &&
        IsKeyPressed(KEY_SPACE)) {
        toggle_audio_playback();
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Z)) {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            state_.redo();
        } else {
            state_.undo();
        }
        selected_note_index_.reset();
        sync_timing_event_selection();
        sync_metadata_inputs();
        meter_map_.rebuild(state_.data());
        rebuild_waveform_samples();
        refresh_audio_length_tick();
        update_audio_clock();
        previous_playback_tick_ = playback_tick_;
        previous_audio_playing_ = audio_playing_;
        load_timing_event_inputs();
        timing_panel_.input_error.clear();
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Y)) {
        state_.redo();
        selected_note_index_.reset();
        sync_timing_event_selection();
        sync_metadata_inputs();
        meter_map_.rebuild(state_.data());
        rebuild_waveform_samples();
        refresh_audio_length_tick();
        update_audio_clock();
        previous_playback_tick_ = playback_tick_;
        previous_audio_playing_ = audio_playing_;
        load_timing_event_inputs();
        timing_panel_.input_error.clear();
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_S)) {
        save_chart();
        return;
    }

    if (!has_active_metadata_input() &&
        timing_panel_.active_input_field == editor_timing_input_field::none &&
        IsKeyPressed(KEY_DELETE) && selected_note_index_.has_value()) {
        const size_t selected_index = *selected_note_index_;
        if (state_.remove_note(selected_index)) {
            selected_note_index_.reset();
        }
    }

    if (selected_note_index_.has_value() && *selected_note_index_ >= state_.data().notes.size()) {
        selected_note_index_.reset();
    }
}

void editor_scene::handle_text_input() {
    if (has_active_metadata_input() || has_blocking_modal()) {
        return;
    }
}

void editor_scene::handle_timeline_interaction() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const editor_timeline_metrics metrics = timeline_metrics();
    const Rectangle content = metrics.content_rect();
    const bool timeline_hovered = ui::is_hovered(content, ui::draw_layer::base);

    if (timing_panel_.bar_pick_mode) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && timeline_hovered && timing_panel_.selected_event_index.has_value()) {
            const int tick = snap_tick(metrics.y_to_tick(mouse.y));
            const editor_meter_map::bar_beat_position position = meter_map_.bar_beat_at_tick(tick);
            if (*timing_panel_.selected_event_index < state_.data().timing_events.size()) {
                const timing_event& event = state_.data().timing_events[*timing_panel_.selected_event_index];
                const std::string value = std::to_string(position.measure) + ":" + std::to_string(position.beat);
                if (event.type == timing_event_type::bpm) {
                    timing_panel_.inputs.bpm_bar.value = value;
                } else {
                    timing_panel_.inputs.meter_bar.value = value;
                }
                apply_selected_timing_event();
            }
            timing_panel_.bar_pick_mode = false;
            timing_panel_.active_input_field = editor_timing_input_field::none;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE)) {
            timing_panel_.bar_pick_mode = false;
            timing_panel_.active_input_field = editor_timing_input_field::none;
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        selected_note_index_ = timeline_hovered ? note_at_position(mouse) : std::nullopt;
    }

    if (!timeline_hovered) {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            timeline_drag_.active = false;
        }
        return;
    }

    if ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        seek_audio_to_tick(snap_tick(metrics.y_to_tick(mouse.y)), !audio_playing_);
        timeline_drag_.active = false;
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const std::optional<int> lane = lane_at_position(mouse);
        if (lane.has_value()) {
            timeline_drag_.active = true;
            timeline_drag_.lane = *lane;
            timeline_drag_.start_tick = snap_tick(metrics.y_to_tick(mouse.y));
            timeline_drag_.current_tick = timeline_drag_.start_tick;
        }
    }

    if (timeline_drag_.active && (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT))) {
        timeline_drag_.current_tick = snap_tick(metrics.y_to_tick(mouse.y));
    }

    if (!timeline_drag_.active || !IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        return;
    }

    timeline_drag_.current_tick = snap_tick(metrics.y_to_tick(mouse.y));
    const std::optional<note_data> note = dragged_note();
    timeline_drag_.active = false;

    if (!note.has_value() || state_.has_note_overlap(*note)) {
        return;
    }

    state_.add_note(*note);
    selected_note_index_ = state_.data().notes.empty() ? std::nullopt : std::optional<size_t>(state_.data().notes.size() - 1);
}

void editor_scene::apply_scroll_and_zoom(float dt) {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const float wheel = GetMouseWheelMove();
    const editor_timeline_metrics metrics = timeline_metrics();
    const Rectangle content = metrics.content_rect();
    const Rectangle track = metrics.scrollbar_track_rect();
    const ui::scrollbar_interaction scrollbar = ui::update_vertical_scrollbar(
        track, content_height_pixels(), scroll_offset_pixels(), scrollbar_dragging_, scrollbar_drag_offset_, 40.0f);
    bottom_tick_target_ = max_bottom_tick() - scrollbar.scroll_offset * ticks_per_pixel_;
    if (scrollbar.changed || scrollbar.dragging) {
        bottom_tick_ = bottom_tick_target_;
    }

    if (wheel != 0.0f && CheckCollisionPointRec(mouse, content) &&
        (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
        const int anchor_tick = metrics.y_to_tick(mouse.y);
        const float zoom_scale = wheel > 0.0f ? 0.85f : 1.15f;
        ticks_per_pixel_ = std::clamp(ticks_per_pixel_ * zoom_scale, kMinTicksPerPixel, kMaxTicksPerPixel);
        bottom_tick_target_ = static_cast<float>(anchor_tick) -
                              (content.y + content.height - mouse.y) * ticks_per_pixel_;
        bottom_tick_target_ = std::clamp(bottom_tick_target_, min_bottom_tick(), max_bottom_tick());
    } else if (!audio_playing_ && wheel != 0.0f && CheckCollisionPointRec(mouse, content)) {
        bottom_tick_target_ = std::clamp(bottom_tick_target_ + wheel * visible_tick_span() * kScrollWheelViewportRatio,
                                         min_bottom_tick(), max_bottom_tick());
    } else if (audio_playing_) {
        bottom_tick_target_ = std::clamp(static_cast<float>(playback_tick_) - visible_tick_span() * kPlaybackFollowViewportRatio,
                                         min_bottom_tick(), max_bottom_tick());
    }

    bottom_tick_target_ = std::clamp(bottom_tick_target_, min_bottom_tick(), max_bottom_tick());
    if (bottom_tick_target_ <= min_bottom_tick() || bottom_tick_target_ >= max_bottom_tick()) {
        bottom_tick_ = bottom_tick_target_;
        return;
    }
    bottom_tick_ += (bottom_tick_target_ - bottom_tick_) * std::min(1.0f, kScrollLerpSpeed * dt);
    if (std::fabs(bottom_tick_ - bottom_tick_target_) < 0.5f) {
        bottom_tick_ = bottom_tick_target_;
    }
}

void editor_scene::select_timing_event(std::optional<size_t> index, bool scroll_into_view) {
    timing_panel_.selected_event_index = index;
    timing_panel_.active_input_field = editor_timing_input_field::none;
    timing_panel_.input_error.clear();
    timing_panel_.bar_pick_mode = false;
    load_timing_event_inputs();

    if (scroll_into_view && index.has_value() && *index < state_.data().timing_events.size()) {
        scroll_to_tick(state_.data().timing_events[*index].tick);
        const auto timing_indices = sorted_timing_event_indices();
        const auto it = std::find(timing_indices.begin(), timing_indices.end(), *index);
        if (it != timing_indices.end()) {
            constexpr float kTimingRowHeight = 30.0f;
            constexpr float kTimingRowGap = 4.0f;
            constexpr float kTimingListViewportHeight = 174.0f;
            const float row_top = static_cast<float>(std::distance(timing_indices.begin(), it)) * (kTimingRowHeight + kTimingRowGap);
            const float row_bottom = row_top + kTimingRowHeight;
            if (row_top < timing_panel_.list_scroll_offset) {
                timing_panel_.list_scroll_offset = row_top;
            } else if (row_bottom > timing_panel_.list_scroll_offset + kTimingListViewportHeight) {
                timing_panel_.list_scroll_offset = row_bottom - kTimingListViewportHeight;
            }
        }
    }
}

void editor_scene::scroll_to_tick(int tick) {
    const float target = std::clamp(static_cast<float>(tick) - visible_tick_span() * 0.5f,
                                    min_bottom_tick(), max_bottom_tick());
    bottom_tick_target_ = target;
    bottom_tick_ = target;
}

void editor_scene::sync_timing_event_selection() {
    if (timing_panel_.selected_event_index.has_value() &&
        *timing_panel_.selected_event_index >= state_.data().timing_events.size()) {
        timing_panel_.selected_event_index = state_.data().timing_events.empty()
            ? std::nullopt
            : std::optional<size_t>(state_.data().timing_events.size() - 1);
    }
}

void editor_scene::scroll_timing_list_to_bottom() {
    constexpr float kTimingRowHeight = 30.0f;
    constexpr float kTimingRowGap = 4.0f;
    constexpr float kTimingListViewportHeight = 174.0f;

    const size_t count = state_.data().timing_events.size();
    const float content_height = count == 0
        ? kTimingListViewportHeight
        : static_cast<float>(count) * kTimingRowHeight +
            static_cast<float>(std::max<int>(0, static_cast<int>(count) - 1)) * kTimingRowGap;
    timing_panel_.list_scroll_offset = std::max(0.0f, content_height - kTimingListViewportHeight);
}

bool editor_scene::apply_selected_timing_event() {
    sync_timing_event_selection();
    if (!timing_panel_.selected_event_index.has_value()) {
        timing_panel_.input_error = "Select a timing event first.";
        return false;
    }

    const size_t index = *timing_panel_.selected_event_index;
    if (index >= state_.data().timing_events.size()) {
        timing_panel_.input_error = "Selected timing event is out of range.";
        return false;
    }

    timing_event updated = state_.data().timing_events[index];
    if (updated.type == timing_event_type::bpm) {
        editor_meter_map::bar_beat_position position;
        float bpm = 0.0f;
        if (!try_parse_bar_beat(timing_panel_.inputs.bpm_bar.value, position)) {
            timing_panel_.input_error = "Bar must be in M:B format.";
            return false;
        }
        if (!try_parse_float(timing_panel_.inputs.bpm_value.value, bpm) || bpm <= 0.0f) {
            timing_panel_.input_error = "BPM must be greater than zero.";
            return false;
        }
        const std::optional<int> tick = meter_map_.tick_from_bar_beat(position.measure, position.beat);
        if (!tick.has_value()) {
            timing_panel_.input_error = "Bar is outside the current meter layout.";
            return false;
        }
        updated.tick = *tick;
        updated.bpm = bpm;
    } else {
        editor_meter_map::bar_beat_position position;
        int numerator = 0;
        int denominator = 0;
        if (!try_parse_bar_beat(timing_panel_.inputs.meter_bar.value, position)) {
            timing_panel_.input_error = "Bar must be in M:B format.";
            return false;
        }
        if (!try_parse_int(timing_panel_.inputs.meter_numerator.value, numerator) || numerator <= 0) {
            timing_panel_.input_error = "Numerator must be 1 or greater.";
            return false;
        }
        if (!try_parse_int(timing_panel_.inputs.meter_denominator.value, denominator) || denominator <= 0) {
            timing_panel_.input_error = "Denominator must be 1 or greater.";
            return false;
        }
        const std::optional<int> tick = meter_map_.tick_from_bar_beat(position.measure, position.beat);
        if (!tick.has_value()) {
            timing_panel_.input_error = "Bar is outside the current meter layout.";
            return false;
        }
        updated.tick = *tick;
        updated.numerator = numerator;
        updated.denominator = denominator;
    }

    if (updated.type == timing_event_type::bpm && state_.data().timing_events[index].tick == 0 && updated.tick != 0) {
        timing_panel_.input_error = "The BPM event at tick 0 must stay at tick 0.";
        return false;
    }

    if (!state_.modify_timing_event(index, updated)) {
        timing_panel_.input_error = "Failed to update the timing event.";
        return false;
    }

    meter_map_.rebuild(state_.data());
    timing_panel_.input_error.clear();
    load_timing_event_inputs();
    scroll_to_tick(updated.tick);
    return true;
}

void editor_scene::add_timing_event(timing_event_type type) {
    timing_event event;
    event.type = type;
    event.tick = default_timing_event_tick();
    if (type == timing_event_type::bpm) {
        event.bpm = state_.engine().get_bpm_at(event.tick);
        event.numerator = 4;
        event.denominator = 4;
    } else {
        const editor_meter_map::bar_beat_position position = meter_map_.bar_beat_at_tick(event.tick);
        const std::optional<int> snapped_tick = meter_map_.tick_from_bar_beat(position.measure, 1);
        event.tick = snapped_tick.value_or(event.tick);
        event.bpm = 0.0f;
        event.numerator = 4;
        event.denominator = 4;
    }

    state_.add_timing_event(event);
    meter_map_.rebuild(state_.data());
    select_timing_event(state_.data().timing_events.size() - 1, true);
}

void editor_scene::delete_selected_timing_event() {
    sync_timing_event_selection();
    if (!timing_panel_.selected_event_index.has_value()) {
        timing_panel_.input_error = "Select a timing event first.";
        return;
    }
    if (!can_delete_selected_timing_event()) {
        timing_panel_.input_error = "The BPM event at tick 0 cannot be deleted.";
        return;
    }

    const size_t index = *timing_panel_.selected_event_index;
    if (!state_.remove_timing_event(index)) {
        timing_panel_.input_error = "Failed to delete the timing event.";
        return;
    }

    meter_map_.rebuild(state_.data());
    sync_timing_event_selection();
    timing_panel_.input_error.clear();
    load_timing_event_inputs();
}

bool editor_scene::can_delete_selected_timing_event() const {
    if (!timing_panel_.selected_event_index.has_value() ||
        *timing_panel_.selected_event_index >= state_.data().timing_events.size()) {
        return false;
    }
    const timing_event& event = state_.data().timing_events[*timing_panel_.selected_event_index];
    return !(event.type == timing_event_type::bpm && event.tick == 0);
}

void editor_scene::load_timing_event_inputs() {
    sync_timing_event_selection();
    if (!timing_panel_.selected_event_index.has_value() ||
        *timing_panel_.selected_event_index >= state_.data().timing_events.size()) {
        clear_timing_event_inputs();
        return;
    }

    timing_panel_.inputs.bpm_bar.active = false;
    timing_panel_.inputs.bpm_value.active = false;
    timing_panel_.inputs.meter_bar.active = false;
    timing_panel_.inputs.meter_numerator.active = false;
    timing_panel_.inputs.meter_denominator.active = false;

    const timing_event& event = state_.data().timing_events[*timing_panel_.selected_event_index];
    timing_panel_.inputs.bpm_bar.value = meter_map_.bar_beat_label(event.tick);
    timing_panel_.inputs.bpm_value.value = TextFormat("%.1f", event.bpm);
    timing_panel_.inputs.meter_bar.value = meter_map_.bar_beat_label(event.tick);
    timing_panel_.inputs.meter_numerator.value = std::to_string(event.numerator);
    timing_panel_.inputs.meter_denominator.value = std::to_string(event.denominator);
}

void editor_scene::clear_timing_event_inputs() {
    timing_panel_.inputs.bpm_bar.value.clear();
    timing_panel_.inputs.bpm_value.value.clear();
    timing_panel_.inputs.meter_bar.value.clear();
    timing_panel_.inputs.meter_numerator.value.clear();
    timing_panel_.inputs.meter_denominator.value.clear();
    timing_panel_.inputs.bpm_bar.active = false;
    timing_panel_.inputs.bpm_value.active = false;
    timing_panel_.inputs.meter_bar.active = false;
    timing_panel_.inputs.meter_numerator.active = false;
    timing_panel_.inputs.meter_denominator.active = false;
}

void editor_scene::sync_metadata_inputs() {
    metadata_panel_.difficulty_input.value = state_.data().meta.difficulty;
    metadata_panel_.chart_author_input.value = state_.data().meta.chart_author;
    metadata_panel_.key_count = state_.data().meta.key_count;
    metadata_panel_.error.clear();
}

bool editor_scene::has_active_metadata_input() const {
    return metadata_panel_.difficulty_input.active || metadata_panel_.chart_author_input.active;
}

bool editor_scene::apply_metadata_changes(bool clear_notes_for_key_count_change) {
    chart_meta updated = state_.data().meta;
    updated.difficulty = metadata_panel_.difficulty_input.value;
    updated.chart_author = metadata_panel_.chart_author_input.value;
    updated.key_count = metadata_panel_.key_count;
    if (state_.file_path().empty()) {
        updated.chart_id = generated_chart_id(updated.difficulty);
    }

    const bool key_count_changed = updated.key_count != state_.data().meta.key_count;
    if (key_count_changed && !clear_notes_for_key_count_change && !state_.data().notes.empty()) {
        metadata_panel_.pending_key_count = updated.key_count;
        metadata_panel_.key_count_confirm_open = true;
        metadata_panel_.error = "Changing mode will clear all notes.";
        return false;
    }

    if (!state_.modify_metadata(updated, clear_notes_for_key_count_change)) {
        metadata_panel_.error = "Failed to update chart metadata.";
        metadata_panel_.key_count = state_.data().meta.key_count;
        return false;
    }

    if (key_count_changed) {
        selected_note_index_.reset();
    }

    refresh_audio_length_tick();
    rebuild_waveform_samples();
    update_audio_clock();
    previous_playback_tick_ = playback_tick_;
    previous_audio_playing_ = audio_playing_;
    metadata_panel_.error.clear();
    close_key_count_confirmation();
    sync_metadata_inputs();
    return true;
}

bool editor_scene::apply_chart_offset(int offset_ms) {
    chart_meta updated = state_.data().meta;
    updated.offset = offset_ms;
    if (!state_.modify_metadata(updated)) {
        return false;
    }

    refresh_audio_length_tick();
    rebuild_waveform_samples();
    update_audio_clock();
    previous_playback_tick_ = playback_tick_;
    previous_audio_playing_ = audio_playing_;
    metadata_panel_.error.clear();
    sync_metadata_inputs();
    return true;
}

void editor_scene::close_key_count_confirmation() {
    metadata_panel_.key_count_confirm_open = false;
    metadata_panel_.pending_key_count = state_.data().meta.key_count;
    metadata_panel_.key_count = state_.data().meta.key_count;
}

void editor_scene::update_key_count_confirmation() {
    const Rectangle confirm_button = {kMetadataConfirmRect.x + 94.0f, kMetadataConfirmRect.y + 142.0f, 104.0f, 30.0f};
    const Rectangle cancel_button = {kMetadataConfirmRect.x + 222.0f, kMetadataConfirmRect.y + 142.0f, 104.0f, 30.0f};

    if (ui::is_clicked(confirm_button, ui::draw_layer::modal)) {
        metadata_panel_.key_count = metadata_panel_.pending_key_count;
        apply_metadata_changes(true);
        return;
    }

    if (ui::is_clicked(cancel_button, ui::draw_layer::modal)) {
        metadata_panel_.error.clear();
        close_key_count_confirmation();
    }
}

void editor_scene::update_unsaved_changes_dialog() {
    const Rectangle save_button = {kUnsavedChangesRect.x + 32.0f, kUnsavedChangesRect.y + 154.0f, 112.0f, 32.0f};
    const Rectangle discard_button = {kUnsavedChangesRect.x + 172.0f, kUnsavedChangesRect.y + 154.0f, 112.0f, 32.0f};
    const Rectangle cancel_button = {kUnsavedChangesRect.x + 312.0f, kUnsavedChangesRect.y + 154.0f, 112.0f, 32.0f};

    if (ui::is_clicked(save_button, ui::draw_layer::modal)) {
        const pending_action action = unsaved_changes_dialog_.pending;
        unsaved_changes_dialog_ = {};
        if (state_.file_path().empty()) {
            open_save_dialog(action);
        } else if (save_chart()) {
            perform_action(action);
        }
        return;
    }

    if (ui::is_clicked(discard_button, ui::draw_layer::modal)) {
        const pending_action action = unsaved_changes_dialog_.pending;
        unsaved_changes_dialog_ = {};
        perform_action(action);
        return;
    }

    if (ui::is_clicked(cancel_button, ui::draw_layer::modal)) {
        unsaved_changes_dialog_ = {};
    }
}

void editor_scene::draw_unsaved_changes_dialog() const {
    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(kUnsavedChangesRect, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("Unsaved Changes", 28,
                             {kUnsavedChangesRect.x + 20.0f, kUnsavedChangesRect.y + 20.0f,
                              kUnsavedChangesRect.width - 40.0f, 30.0f},
                             g_theme->text, ui::text_align::center, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("There are unsaved changes.", 18,
                             {kUnsavedChangesRect.x + 28.0f, kUnsavedChangesRect.y + 78.0f,
                              kUnsavedChangesRect.width - 56.0f, 24.0f},
                             g_theme->text_secondary, ui::text_align::center, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("Save before leaving the editor?", 18,
                             {kUnsavedChangesRect.x + 28.0f, kUnsavedChangesRect.y + 104.0f,
                              kUnsavedChangesRect.width - 56.0f, 24.0f},
                             g_theme->text, ui::text_align::center, ui::draw_layer::modal);

    const Rectangle save_button = {kUnsavedChangesRect.x + 32.0f, kUnsavedChangesRect.y + 154.0f, 112.0f, 32.0f};
    const Rectangle discard_button = {kUnsavedChangesRect.x + 172.0f, kUnsavedChangesRect.y + 154.0f, 112.0f, 32.0f};
    const Rectangle cancel_button = {kUnsavedChangesRect.x + 312.0f, kUnsavedChangesRect.y + 154.0f, 112.0f, 32.0f};
    ui::enqueue_button(save_button, "SAVE", 16, ui::draw_layer::modal, 1.5f);
    ui::enqueue_button(discard_button, "DISCARD", 16, ui::draw_layer::modal, 1.5f);
    ui::enqueue_button(cancel_button, "CANCEL", 16, ui::draw_layer::modal, 1.5f);
}

void editor_scene::draw_save_dialog() {
    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(kSaveDialogRect, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("Save Chart", 28,
                             {kSaveDialogRect.x + 20.0f, kSaveDialogRect.y + 18.0f,
                              kSaveDialogRect.width - 40.0f, 30.0f},
                             g_theme->text, ui::text_align::center, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("Save into this song's charts directory.", 18,
                             {kSaveDialogRect.x + 24.0f, kSaveDialogRect.y + 52.0f,
                              kSaveDialogRect.width - 48.0f, 22.0f},
                             g_theme->text_secondary, ui::text_align::center, ui::draw_layer::modal);

    const ui::text_input_result file_name_result = ui::draw_text_input(
        {kSaveDialogRect.x + 20.0f, kSaveDialogRect.y + 88.0f, kSaveDialogRect.width - 40.0f, 38.0f},
        save_dialog_.file_name_input, "File", "normal.chart", "new-chart.chart",
        ui::draw_layer::modal, 16, 48, accepts_chart_file_character, 64.0f);
    if (file_name_result.submitted) {
        save_dialog_.submit_requested = true;
    }

    if (!save_dialog_.error.empty()) {
        ui::draw_text_in_rect(save_dialog_.error.c_str(), 16,
                              {kSaveDialogRect.x + 24.0f, kSaveDialogRect.y + 136.0f,
                               kSaveDialogRect.width - 48.0f, 22.0f},
                              g_theme->error, ui::text_align::left);
    }

    const Rectangle save_button = {kSaveDialogRect.x + 136.0f, kSaveDialogRect.y + 172.0f, 108.0f, 32.0f};
    const Rectangle cancel_button = {kSaveDialogRect.x + 276.0f, kSaveDialogRect.y + 172.0f, 108.0f, 32.0f};
    ui::enqueue_button(save_button, "SAVE", 16, ui::draw_layer::modal, 1.5f);
    ui::enqueue_button(cancel_button, "CANCEL", 16, ui::draw_layer::modal, 1.5f);

    if (ui::is_clicked(save_button, ui::draw_layer::modal)) {
        save_dialog_.submit_requested = true;
    } else if (ui::is_clicked(cancel_button, ui::draw_layer::modal)) {
        close_save_dialog();
    }
}

void editor_scene::draw_key_count_confirmation() const {
    ui::enqueue_fullscreen_overlay(g_theme->pause_overlay, ui::draw_layer::overlay);
    ui::enqueue_panel(kMetadataConfirmRect, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("Change Key Mode", 28,
                             {kMetadataConfirmRect.x + 20.0f, kMetadataConfirmRect.y + 18.0f,
                              kMetadataConfirmRect.width - 40.0f, 30.0f},
                             g_theme->text, ui::text_align::center, ui::draw_layer::modal);
    ui::enqueue_text_in_rect("All placed notes will be cleared.", 18,
                             {kMetadataConfirmRect.x + 28.0f, kMetadataConfirmRect.y + 70.0f,
                              kMetadataConfirmRect.width - 56.0f, 24.0f},
                             g_theme->text_secondary, ui::text_align::center, ui::draw_layer::modal);
    ui::enqueue_text_in_rect(TextFormat("Switch to %s?", key_count_label(metadata_panel_.pending_key_count)), 18,
                             {kMetadataConfirmRect.x + 28.0f, kMetadataConfirmRect.y + 98.0f,
                              kMetadataConfirmRect.width - 56.0f, 24.0f},
                             g_theme->text, ui::text_align::center, ui::draw_layer::modal);

    const Rectangle confirm_button = {kMetadataConfirmRect.x + 94.0f, kMetadataConfirmRect.y + 142.0f, 104.0f, 30.0f};
    const Rectangle cancel_button = {kMetadataConfirmRect.x + 222.0f, kMetadataConfirmRect.y + 142.0f, 104.0f, 30.0f};
    ui::enqueue_button(confirm_button, "CONFIRM", 16, ui::draw_layer::modal, 1.5f);
    ui::enqueue_button(cancel_button, "CANCEL", 16, ui::draw_layer::modal, 1.5f);
}

void editor_scene::draw_left_panel() {
    const auto& t = *g_theme;
    const double now = GetTime();
    const Rectangle content = ui::inset(kLeftPanelRect, ui::edge_insets::uniform(16.0f));
    const bool has_file = !state_.file_path().empty();
    const char* status_label = state_.is_dirty() ? "Modified" : (has_file ? "Saved" : "Unsaved");

    const Rectangle header_rect = ui::place(content, content.width, 54.0f, ui::anchor::top_left, ui::anchor::top_left);
    ui::draw_header_block(header_rect, "Chart", has_file ? "Existing chart" : "New chart", 28, 18, 4.0f);
    const Rectangle song_title_rect = {header_rect.x, header_rect.y + 58.0f, header_rect.width, 24.0f};
    draw_marquee_text(song_.meta.title.c_str(), song_title_rect.x,
                      song_title_rect.y + 2.0f, 18, t.text_secondary, song_title_rect.width, now);

    const Rectangle meta_box = {content.x, content.y + 100.0f, content.width, 214.0f};
    ui::draw_section(meta_box);
    ui::draw_text_in_rect("Metadata", 22,
                          {meta_box.x + 12.0f, meta_box.y + 10.0f, meta_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);

    const ui::text_input_result difficulty_result = ui::draw_text_input(
        {meta_box.x + 12.0f, meta_box.y + 46.0f, meta_box.width - 24.0f, 34.0f},
        metadata_panel_.difficulty_input, "Diff", "Difficulty", "New",
        ui::draw_layer::base, 16, 24, accepts_metadata_character, 58.0f);
    const ui::text_input_result author_result = ui::draw_text_input(
        {meta_box.x + 12.0f, meta_box.y + 86.0f, meta_box.width - 24.0f, 34.0f},
        metadata_panel_.chart_author_input, "Author", "Chart author", "Unknown",
        ui::draw_layer::base, 16, 32, accepts_metadata_character, 58.0f);

    if (difficulty_result.activated || author_result.activated) {
        timing_panel_.active_input_field = editor_timing_input_field::none;
        timing_panel_.bar_pick_mode = false;
        timing_panel_.input_error.clear();
    }

    const ui::selector_state key_count_selector = ui::draw_value_selector(
        {meta_box.x + 12.0f, meta_box.y + 126.0f, meta_box.width - 24.0f, 34.0f},
        "Mode", key_count_label(metadata_panel_.key_count),
        16, 26.0f, 58.0f, 12.0f);
    if ((key_count_selector.left.clicked || key_count_selector.right.clicked) && !metadata_panel_.key_count_confirm_open) {
        metadata_panel_.key_count = metadata_panel_.key_count == 4 ? 6 : 4;
        metadata_panel_.error.clear();
        apply_metadata_changes(false);
    }

    ui::draw_label_value({meta_box.x + 12.0f, meta_box.y + 170.0f, meta_box.width - 24.0f, 20.0f},
                         "Status", status_label, 16, t.text_secondary,
                         state_.is_dirty() ? t.error : t.success, 58.0f);

    if (!metadata_panel_.error.empty()) {
        ui::draw_text_in_rect(metadata_panel_.error.c_str(), 16,
                              {meta_box.x + 12.0f, meta_box.y + 188.0f, meta_box.width - 24.0f, 20.0f},
                              t.error, ui::text_align::left);
    }

    const bool apply_requested = difficulty_result.submitted || author_result.submitted;
    if (apply_requested) {
        apply_metadata_changes(false);
        metadata_panel_.difficulty_input.active = false;
        metadata_panel_.chart_author_input.active = false;
    }

    const Rectangle tools_box = {content.x, meta_box.y + meta_box.height + 12.0f, content.width, 114.0f};
    ui::draw_section(tools_box);
    ui::draw_label_value({tools_box.x + 12.0f, tools_box.y + 16.0f, tools_box.width - 24.0f, 24.0f},
                         "Mode", key_count_label(state_.data().meta.key_count), 16,
                         t.text_secondary, t.text, 92.0f);
    ui::draw_label_value({tools_box.x + 12.0f, tools_box.y + 44.0f, tools_box.width - 24.0f, 24.0f},
                         "Offset", TextFormat("%d ms", state_.data().meta.offset), 16,
                         t.text_secondary, t.text, 92.0f);

    ui::draw_label_value({tools_box.x + 12.0f, tools_box.y + 72.0f, tools_box.width - 24.0f, 24.0f},
                         "Notes", TextFormat("%d", static_cast<int>(state_.data().notes.size())), 16,
                         t.text_secondary, t.text, 92.0f);

    if (!load_errors_.empty()) {
        ui::draw_text_in_rect(load_errors_.front().c_str(), 18,
                              {content.x, tools_box.y + tools_box.height + 18.0f, content.width, 52.0f},
                              t.error, ui::text_align::left);
    }
}

void editor_scene::draw_right_panel() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const Rectangle content = ui::inset(kRightPanelRect, ui::edge_insets::uniform(16.0f));
    sync_timing_event_selection();
    const auto timing_indices = sorted_timing_event_indices();
    std::vector<editor_timing_panel_item> items;
    items.reserve(timing_indices.size());
    for (const size_t index : timing_indices) {
        const timing_event& event = state_.data().timing_events[index];
        items.push_back({
            index,
            std::string(timing_event_type_label(event.type)) + " " + meter_map_.bar_beat_label(event.tick),
            event.type == timing_event_type::bpm ? TextFormat("%.1f", event.bpm) : TextFormat("%d/%d", event.numerator, event.denominator),
            timing_panel_.selected_event_index.has_value() && *timing_panel_.selected_event_index == index
        });
    }
    std::optional<timing_event> selected_event;
    if (timing_panel_.selected_event_index.has_value() &&
        *timing_panel_.selected_event_index < state_.data().timing_events.size()) {
        selected_event = state_.data().timing_events[*timing_panel_.selected_event_index];
    }
    const editor_timing_panel_result panel_result = editor_timing_panel::draw(
        {content, mouse, std::move(items), selected_event, can_delete_selected_timing_event()},
        timing_panel_);

    if (panel_result.selected_event_index.has_value()) {
        select_timing_event(panel_result.selected_event_index, true);
        metadata_panel_.difficulty_input.active = false;
        metadata_panel_.chart_author_input.active = false;
    }
    if (panel_result.add_bpm) {
        add_timing_event(timing_event_type::bpm);
    }
    if (panel_result.add_meter) {
        add_timing_event(timing_event_type::meter);
    }
    if (panel_result.delete_selected) {
        delete_selected_timing_event();
    }
    if (panel_result.apply_selected) {
        apply_selected_timing_event();
        timing_panel_.active_input_field = editor_timing_input_field::none;
        timing_panel_.bar_pick_mode = false;
    }
    if (panel_result.clicked_input_row) {
        metadata_panel_.difficulty_input.active = false;
        metadata_panel_.chart_author_input.active = false;
    }

    const Rectangle editor_box = {content.x, content.y + 372.0f, content.width, 164.0f};
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        timing_panel_.active_input_field != editor_timing_input_field::none &&
        !panel_result.clicked_input_row &&
        !CheckCollisionPointRec(mouse, editor_box)) {
        timing_panel_.active_input_field = editor_timing_input_field::none;
        timing_panel_.bar_pick_mode = false;
    }
}

void editor_scene::draw_timeline() const {
    const editor_timeline_metrics metrics = timeline_metrics();
    const int min_tick = static_cast<int>(std::floor(bottom_tick_ - kMinVisibleTicks * 0.1f));
    const int max_tick = static_cast<int>(std::ceil(bottom_tick_ + visible_tick_span()));
    std::vector<editor_timeline_note> notes;
    notes.reserve(state_.data().notes.size());
    for (const note_data& note : state_.data().notes) {
        notes.push_back(make_timeline_note(note));
    }
    std::optional<editor_timeline_note> preview_note;
    bool preview_has_overlap = false;
    if (const std::optional<note_data> preview_data = dragged_note(); preview_data.has_value()) {
        preview_note = make_timeline_note(*preview_data);
        preview_has_overlap = state_.has_note_overlap(*preview_data);
    }

    editor_timeline_view::draw({
        metrics,
        meter_map_.visible_grid_lines(min_tick, max_tick),
        std::move(notes),
        selected_note_index_,
        audio_loaded_ ? std::optional<int>(playback_tick_) : std::nullopt,
        &waveform_samples_,
        waveform_visible_,
        preview_note,
        preview_has_overlap,
        min_tick,
        max_tick,
        snap_interval(),
        content_height_pixels(),
        scroll_offset_pixels()
    });
}

void editor_scene::draw_cursor_hud() const {
    const auto& t = *g_theme;
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    if (!CheckCollisionPointRec(mouse, kTimelineRect)) {
        return;
    }

    const int tick = std::max(0, timeline_metrics().y_to_tick(mouse.y));
    const int snapped_tick = snap_tick(tick);
    const double beat = meter_map_.beat_number_at_tick(tick);
    const editor_meter_map::bar_beat_position position = meter_map_.bar_beat_at_tick(tick);
    const Rectangle hud_rect = ui::place(kTimelineRect, 340.0f, 34.0f,
                                         ui::anchor::bottom_left, ui::anchor::bottom_left,
                                         {12.0f, -12.0f});
    DrawRectangleRec(hud_rect, with_alpha(t.panel, 240));
    DrawRectangleLinesEx(hud_rect, 1.5f, t.border);
    ui::draw_text_f(TextFormat("bar %d:%d   beat %.2f   snap %d", position.measure, position.beat, beat, snapped_tick),
                    hud_rect.x + 12.0f, hud_rect.y + 8.0f, 18, t.text);
}

void editor_scene::draw_header_tools() {
    const auto& t = *g_theme;
    ui::draw_section(kPlaybackRect);
    const std::string playback_status = playback_status_text();
    ui::draw_label_value(ui::inset(kPlaybackRect, ui::edge_insets::symmetric(0.0f, 12.0f)),
                         "Audio", playback_status.c_str(), 16,
                         t.text, audio_loaded_ ? t.text_secondary : t.text_muted, 56.0f);

    const std::string offset_label =
        (state_.data().meta.offset > 0 ? "+" : "") + std::to_string(state_.data().meta.offset) + " ms";
    const ui::selector_state chart_offset = ui::draw_value_selector(
        kChartOffsetRect, "Offset", offset_label.c_str(),
        16, 24.0f, 68.0f, 10.0f);
    if (chart_offset.left.clicked) {
        apply_chart_offset(std::max(-10000, state_.data().meta.offset - 5));
    } else if (chart_offset.right.clicked) {
        apply_chart_offset(std::min(10000, state_.data().meta.offset + 5));
    }

    const ui::button_state waveform_toggle = ui::draw_button_colored(
        kWaveformToggleRect, waveform_visible_ ? "WAVE ON" : "WAVE OFF", 16,
        waveform_visible_ ? t.row_selected : t.row,
        waveform_visible_ ? t.row_active : t.row_hover,
        waveform_visible_ ? t.text : t.text_secondary);
    if (waveform_toggle.clicked) {
        waveform_visible_ = !waveform_visible_;
    }

    // 描画は queue に寄せるが、ヒットテストはまだ即時計算のままにしている。
    // 次段で layer と hit test 優先順位を統合すると、modal / pause 系も同じ仕組みに載せられる。
    const ui::dropdown_state dropdown = ui::enqueue_dropdown(
        kSnapDropdownRect, kSnapDropdownMenuRect,
        "Snap", kSnapLabels[snap_index_],
        std::span<const char* const>(kSnapLabels, std::size(kSnapLabels)),
        snap_index_, snap_dropdown_open_,
        ui::draw_layer::base, ui::draw_layer::overlay,
        16, 64.0f);
    if (dropdown.trigger.clicked) {
        snap_dropdown_open_ = !snap_dropdown_open_;
    }
    if (dropdown.clicked_index >= 0) {
        snap_index_ = dropdown.clicked_index;
        snap_dropdown_open_ = false;
    } else if (snap_dropdown_open_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
               !ui::is_hovered(kSnapDropdownRect, ui::draw_layer::base) &&
               !ui::is_hovered(kSnapDropdownMenuRect, ui::draw_layer::overlay)) {
        snap_dropdown_open_ = false;
    }
}
