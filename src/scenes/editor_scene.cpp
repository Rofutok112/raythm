#include "editor_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <limits>
#include <memory>

#include "audio.h"
#include "chart_parser.h"
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
constexpr float kStartPaddingTicks = 240.0f;
constexpr float kScrollLerpSpeed = 12.0f;
constexpr float kScrollWheelViewportRatio = 0.36f;
constexpr float kNoteHeadHeight = 14.0f;
constexpr int kSnapDivisions[] = {1, 2, 4, 8, 16, 32};
constexpr const char* kSnapLabels[] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32"};
constexpr Rectangle kHeaderToolsRect = ui::place(kHeaderRect, 360.0f, 34.0f,
                                                 ui::anchor::center_right, ui::anchor::center_right,
                                                 {-18.0f, 0.0f});
constexpr float kDropdownItemHeight = 30.0f;
constexpr float kDropdownItemSpacing = 4.0f;
constexpr Rectangle kSnapDropdownRect = kHeaderToolsRect;
constexpr Rectangle kSnapDropdownMenuRect = {
    kSnapDropdownRect.x,
    kSnapDropdownRect.y + kSnapDropdownRect.height + 4.0f,
    kSnapDropdownRect.width,
    12.0f + static_cast<float>(std::size(kSnapLabels)) * kDropdownItemHeight +
        static_cast<float>(std::size(kSnapLabels) - 1) * kDropdownItemSpacing
};

std::vector<timing_event> sorted_meter_events(const chart_data& data) {
    std::vector<timing_event> meter_events;
    for (const timing_event& event : data.timing_events) {
        if (event.type == timing_event_type::meter) {
            meter_events.push_back(event);
        }
    }

    std::sort(meter_events.begin(), meter_events.end(), [](const timing_event& left, const timing_event& right) {
        return left.tick < right.tick;
    });

    return meter_events;
}

const char* note_type_label(note_type type) {
    return type == note_type::hold ? "Hold" : "Tap";
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

bool accepts_decimal(editor_scene::timing_input_field field) {
    return field == editor_scene::timing_input_field::bpm_value;
}

bool accepts_bar_beat(editor_scene::timing_input_field field) {
    return field == editor_scene::timing_input_field::bpm_measure ||
           field == editor_scene::timing_input_field::meter_measure;
}

bool accepts_character(editor_scene::timing_input_field field, int codepoint, const std::string& value) {
    if (field == editor_scene::timing_input_field::none) {
        return false;
    }
    if (codepoint >= '0' && codepoint <= '9') {
        return true;
    }
    if (codepoint == ':' && accepts_bar_beat(field) && value.find(':') == std::string::npos) {
        return true;
    }
    if (codepoint == '.' && accepts_decimal(field) && value.find('.') == std::string::npos) {
        return true;
    }
    return false;
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

bool try_parse_bar_beat(const std::string& text, editor_scene::bar_beat_position& out_value) {
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

    bottom_tick_ = -kStartPaddingTicks;
    bottom_tick_target_ = bottom_tick_;
    ticks_per_pixel_ = 2.0f;
    snap_index_ = 4;
    snap_dropdown_open_ = false;
    selected_note_index_.reset();
    selected_timing_event_index_ = state_.data().timing_events.empty() ? std::nullopt : std::optional<size_t>(0);
    active_timing_input_field_ = timing_input_field::none;
    timing_input_error_.clear();
    timing_bar_pick_mode_ = false;
    timing_list_scroll_offset_ = 0.0f;
    timing_list_scrollbar_dragging_ = false;
    timing_list_scrollbar_drag_offset_ = 0.0f;
    note_dragging_ = false;
    drag_lane_ = 0;
    drag_start_tick_ = 0;
    drag_current_tick_ = 0;
    rebuild_meter_segments();
    load_timing_event_inputs();

    const std::filesystem::path audio_path = std::filesystem::path(song_.directory) / song_.meta.audio_file;
    if (std::filesystem::exists(audio_path)) {
        audio music;
        music.load(audio_path.string());
        if (music.is_loaded()) {
            const double length_ms = music.get_length_seconds() * 1000.0;
            audio_length_tick_ = std::max(0, state_.engine().ms_to_tick(length_ms));
        }
    }
}

void editor_scene::update(float dt) {
    rebuild_hit_regions();

    if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        return;
    }

    if (ui::is_clicked(kBackButtonRect)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        return;
    }

    handle_shortcuts();
    handle_text_input();
    handle_timeline_interaction();
    apply_scroll_and_zoom(dt);
}

void editor_scene::rebuild_hit_regions() const {
    ui::begin_hit_regions();
    if (snap_dropdown_open_) {
        ui::register_hit_region(kSnapDropdownMenuRect, ui::draw_layer::overlay);
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
    ui::draw_text_in_rect("CHART EDITOR", 28,
                          ui::place(kHeaderRect, 280.0f, 32.0f, ui::anchor::center, ui::anchor::center),
                          t.text);

    draw_left_panel();
    draw_timeline();
    draw_right_panel();
    draw_header_tools();
    draw_cursor_hud();

    ui::flush_draw_queue();
    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

chart_data editor_scene::make_new_chart_data() const {
    chart_data data;
    data.meta.chart_id = song_.meta.song_id.empty() ? "new-chart" : song_.meta.song_id + "-new";
    data.meta.key_count = new_chart_key_count_;
    data.meta.difficulty = "New";
    data.meta.level = 1;
    data.meta.chart_author = "Unknown";
    data.meta.format_version = 1;
    data.meta.resolution = 480;
    data.timing_events = {
        {timing_event_type::bpm, 0, std::max(song_.meta.base_bpm, 120.0f), 4, 4},
        {timing_event_type::meter, 0, 0.0f, 4, 4},
    };
    return data;
}

void editor_scene::rebuild_meter_segments() {
    meter_segments_.clear();

    const std::vector<timing_event> events = sorted_meter_events(state_.data());
    if (events.empty() || events.front().tick != 0) {
        meter_segments_.push_back({0, 4, 4, 0, 0});
    }

    for (const timing_event& event : events) {
        if (!meter_segments_.empty() && meter_segments_.back().start_tick == event.tick) {
            meter_segments_.back().numerator = event.numerator;
            meter_segments_.back().denominator = event.denominator;
            continue;
        }

        meter_segment segment;
        segment.start_tick = event.tick;
        segment.numerator = event.numerator;
        segment.denominator = event.denominator;

        if (meter_segments_.empty()) {
            segment.beat_index_offset = 0;
            segment.measure_index_offset = 0;
        } else {
            const meter_segment& previous = meter_segments_.back();
            const int beat_ticks = state_.data().meta.resolution * 4 / previous.denominator;
            const int measure_ticks = beat_ticks * std::max(previous.numerator, 1);
            const int beat_count = std::max(0, (segment.start_tick - previous.start_tick) / std::max(1, beat_ticks));
            const int measure_count = std::max(0, (segment.start_tick - previous.start_tick) / std::max(1, measure_ticks));
            segment.beat_index_offset = previous.beat_index_offset + beat_count;
            segment.measure_index_offset = previous.measure_index_offset + measure_count;
        }

        meter_segments_.push_back(segment);
    }
}

std::vector<editor_scene::grid_line> editor_scene::visible_grid_lines(int min_tick, int max_tick) const {
    std::vector<grid_line> lines;
    if (max_tick < min_tick) {
        return lines;
    }

    for (size_t i = 0; i < meter_segments_.size(); ++i) {
        const meter_segment& segment = meter_segments_[i];
        const int next_tick = i + 1 < meter_segments_.size() ? meter_segments_[i + 1].start_tick : max_tick + state_.data().meta.resolution * 16;
        const int beat_ticks = std::max(1, state_.data().meta.resolution * 4 / std::max(segment.denominator, 1));
        const int measure_ticks = beat_ticks * std::max(segment.numerator, 1);
        const int start_tick = std::max(min_tick, segment.start_tick);
        const int end_tick = std::min(max_tick, next_tick - (i + 1 < meter_segments_.size() ? 1 : 0));
        int tick = segment.start_tick;
        if (tick < start_tick) {
            const int delta = start_tick - tick;
            tick += (delta / beat_ticks) * beat_ticks;
            while (tick < start_tick) {
                tick += beat_ticks;
            }
        }

        for (; tick <= end_tick; tick += beat_ticks) {
            const int relative_tick = tick - segment.start_tick;
            const int local_beat_index = relative_tick / beat_ticks;
            const int beat = local_beat_index % std::max(segment.numerator, 1) + 1;
            const int measure = segment.measure_index_offset + local_beat_index / std::max(segment.numerator, 1) + 1;
            lines.push_back({tick, beat == 1, measure, beat});
        }
    }

    return lines;
}

std::vector<size_t> editor_scene::sorted_timing_event_indices() const {
    std::vector<size_t> indices(state_.data().timing_events.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }

    std::stable_sort(indices.begin(), indices.end(), [this](size_t left_index, size_t right_index) {
        const timing_event& left = state_.data().timing_events[left_index];
        const timing_event& right = state_.data().timing_events[right_index];
        return timing_event_sort_less(left, left_index, right, right_index);
    });

    return indices;
}

Rectangle editor_scene::timeline_content_rect() const {
    return {
        kTimelineRect.x + kTimelinePadding,
        kTimelineRect.y + kTimelinePadding,
        kTimelineRect.width - kTimelinePadding * 2.0f - kScrollbarGap - kScrollbarWidth,
        kTimelineRect.height - kTimelinePadding * 2.0f
    };
}

Rectangle editor_scene::timeline_scrollbar_track_rect() const {
    const Rectangle content = timeline_content_rect();
    return {
        content.x + content.width + kScrollbarGap,
        content.y,
        kScrollbarWidth,
        content.height
    };
}

float editor_scene::visible_tick_span() const {
    return timeline_content_rect().height * ticks_per_pixel_;
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

    return std::max(visible_tick_span(), static_cast<float>(max_tick) + state_.data().meta.resolution * 4.0f + kStartPaddingTicks);
}

float editor_scene::content_height_pixels() const {
    return content_tick_span() / ticks_per_pixel_;
}

float editor_scene::scroll_offset_pixels() const {
    return (bottom_tick_ + kStartPaddingTicks) / ticks_per_pixel_;
}

float editor_scene::max_bottom_tick() const {
    return std::max(-kStartPaddingTicks, content_tick_span() - visible_tick_span());
}

float editor_scene::tick_to_timeline_y(int tick) const {
    const Rectangle content = timeline_content_rect();
    return content.y + (static_cast<float>(tick) - bottom_tick_) / ticks_per_pixel_;
}

int editor_scene::timeline_y_to_tick(float y) const {
    const Rectangle content = timeline_content_rect();
    return static_cast<int>(std::lround(bottom_tick_ + (y - content.y) * ticks_per_pixel_));
}

float editor_scene::lane_width() const {
    const int key_count = std::max(1, state_.data().meta.key_count);
    const float content_width = timeline_content_rect().width;
    return (content_width - kLaneGap * static_cast<float>(key_count - 1)) / static_cast<float>(key_count);
}

Rectangle editor_scene::lane_rect(int lane) const {
    const Rectangle content = timeline_content_rect();
    const float width = lane_width();
    return {
        content.x + lane * (width + kLaneGap),
        content.y,
        width,
        content.height
    };
}

double editor_scene::beat_number_at_tick(int tick) const {
    if (meter_segments_.empty()) {
        return 1.0;
    }

    const auto it = std::upper_bound(meter_segments_.begin(), meter_segments_.end(), tick,
                                     [](int value, const meter_segment& segment) {
                                         return value < segment.start_tick;
                                     });
    const meter_segment& segment = it == meter_segments_.begin() ? meter_segments_.front() : *std::prev(it);
    const int beat_ticks = std::max(1, state_.data().meta.resolution * 4 / std::max(segment.denominator, 1));
    const double local_beats = static_cast<double>(tick - segment.start_tick) / static_cast<double>(beat_ticks);
    return static_cast<double>(segment.beat_index_offset) + local_beats + 1.0;
}

editor_scene::bar_beat_position editor_scene::bar_beat_at_tick(int tick) const {
    if (meter_segments_.empty()) {
        return {};
    }

    const auto it = std::upper_bound(meter_segments_.begin(), meter_segments_.end(), tick,
                                     [](int value, const meter_segment& segment) {
                                         return value < segment.start_tick;
                                     });
    const meter_segment& segment = it == meter_segments_.begin() ? meter_segments_.front() : *std::prev(it);
    const int numerator = std::max(segment.numerator, 1);
    const int beat_ticks = std::max(1, state_.data().meta.resolution * 4 / std::max(segment.denominator, 1));
    const int local_beat_index = std::max(0, static_cast<int>(std::llround(
        static_cast<double>(tick - segment.start_tick) / static_cast<double>(beat_ticks))));
    return {
        segment.measure_index_offset + local_beat_index / numerator + 1,
        local_beat_index % numerator + 1
    };
}

std::optional<int> editor_scene::tick_from_bar_beat(int measure, int beat) const {
    if (measure <= 0 || beat <= 0 || meter_segments_.empty()) {
        return std::nullopt;
    }

    for (size_t i = 0; i < meter_segments_.size(); ++i) {
        const meter_segment& segment = meter_segments_[i];
        const int first_measure = segment.measure_index_offset + 1;
        const int next_measure = i + 1 < meter_segments_.size()
            ? meter_segments_[i + 1].measure_index_offset + 1
            : std::numeric_limits<int>::max();
        if (measure < first_measure || measure >= next_measure) {
            continue;
        }

        const int numerator = std::max(segment.numerator, 1);
        if (beat > numerator) {
            return std::nullopt;
        }

        const int beat_ticks = std::max(1, state_.data().meta.resolution * 4 / std::max(segment.denominator, 1));
        const int measure_ticks = beat_ticks * numerator;
        return segment.start_tick + (measure - first_measure) * measure_ticks + (beat - 1) * beat_ticks;
    }

    const meter_segment& segment = meter_segments_.back();
    const int first_measure = segment.measure_index_offset + 1;
    const int numerator = std::max(segment.numerator, 1);
    if (measure < first_measure || beat > numerator) {
        return std::nullopt;
    }

    const int beat_ticks = std::max(1, state_.data().meta.resolution * 4 / std::max(segment.denominator, 1));
    const int measure_ticks = beat_ticks * numerator;
    return segment.start_tick + (measure - first_measure) * measure_ticks + (beat - 1) * beat_ticks;
}

std::string editor_scene::bar_beat_label(int tick) const {
    const bar_beat_position position = bar_beat_at_tick(tick);
    return std::to_string(position.measure) + ":" + std::to_string(position.beat);
}

int editor_scene::snap_division() const {
    return kSnapDivisions[std::clamp(snap_index_, 0, static_cast<int>(std::size(kSnapDivisions)) - 1)];
}

int editor_scene::snap_interval() const {
    return std::max(1, state_.data().meta.resolution * 4 / snap_division());
}

int editor_scene::snap_tick(int raw_tick) const {
    return state_.snap_tick(raw_tick, snap_division());
}

int editor_scene::default_timing_event_tick() const {
    if (selected_timing_event_index_.has_value() && *selected_timing_event_index_ < state_.data().timing_events.size()) {
        return snap_tick(state_.data().timing_events[*selected_timing_event_index_].tick + snap_interval());
    }
    return std::max(snap_interval(), snap_tick(static_cast<int>(bottom_tick_ + visible_tick_span() * 0.5f)));
}

std::optional<int> editor_scene::lane_at_position(Vector2 point) const {
    const Rectangle content = timeline_content_rect();
    if (!CheckCollisionPointRec(point, content)) {
        return std::nullopt;
    }

    for (int lane = 0; lane < state_.data().meta.key_count; ++lane) {
        if (CheckCollisionPointRec(point, lane_rect(lane))) {
            return lane;
        }
    }

    return std::nullopt;
}

editor_scene::note_draw_info editor_scene::note_rects(const note_data& note) const {
    const Rectangle lane = lane_rect(note.lane);
    const float start_y = tick_to_timeline_y(note.tick);
    note_draw_info info;
    info.head_rect = {lane.x + 6.0f, start_y - kNoteHeadHeight * 0.5f, lane.width - 12.0f, kNoteHeadHeight};

    if (note.type == note_type::hold) {
        const float end_y = tick_to_timeline_y(note.end_tick);
        const float top = std::min(start_y, end_y);
        const float height = std::fabs(end_y - start_y);
        info.body_rect = {lane.x + lane.width * 0.3f, top, lane.width * 0.4f, std::max(height, 6.0f)};
        info.tail_rect = {lane.x + 10.0f, end_y - 5.0f, lane.width - 20.0f, 10.0f};
        info.has_body = true;
    }

    return info;
}

std::optional<size_t> editor_scene::note_at_position(Vector2 point) const {
    const Rectangle content = timeline_content_rect();
    if (!CheckCollisionPointRec(point, content)) {
        return std::nullopt;
    }

    for (size_t i = state_.data().notes.size(); i > 0; --i) {
        const size_t index = i - 1;
        const note_data& note = state_.data().notes[index];
        if (note.lane < 0 || note.lane >= state_.data().meta.key_count) {
            continue;
        }

        const note_draw_info info = note_rects(note);
        if (CheckCollisionPointRec(point, info.head_rect) ||
            (info.has_body && (CheckCollisionPointRec(point, info.body_rect) || CheckCollisionPointRec(point, info.tail_rect)))) {
            return index;
        }
    }

    return std::nullopt;
}

void editor_scene::handle_shortcuts() {
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Z)) {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            state_.redo();
        } else {
            state_.undo();
        }
        selected_note_index_.reset();
        sync_timing_event_selection();
        rebuild_meter_segments();
        load_timing_event_inputs();
        timing_input_error_.clear();
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Y)) {
        state_.redo();
        selected_note_index_.reset();
        sync_timing_event_selection();
        rebuild_meter_segments();
        load_timing_event_inputs();
        timing_input_error_.clear();
    }

    if (active_timing_input_field_ == timing_input_field::none &&
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
    std::string* active_input = nullptr;
    switch (active_timing_input_field_) {
        case timing_input_field::bpm_measure:
            active_input = &timing_tick_input_;
            break;
        case timing_input_field::bpm_value:
            active_input = &timing_bpm_input_;
            break;
        case timing_input_field::meter_measure:
            active_input = &timing_measure_input_;
            break;
        case timing_input_field::meter_numerator:
            active_input = &timing_numerator_input_;
            break;
        case timing_input_field::meter_denominator:
            active_input = &timing_denominator_input_;
            break;
        case timing_input_field::none:
            break;
    }

    if (active_input == nullptr) {
        return;
    }

    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        if (accepts_character(active_timing_input_field_, codepoint, *active_input) && active_input->size() < 16) {
            active_input->push_back(static_cast<char>(codepoint));
            timing_input_error_.clear();
        }
        codepoint = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !active_input->empty()) {
        active_input->pop_back();
        timing_input_error_.clear();
    }

    if (IsKeyPressed(KEY_ENTER)) {
        apply_selected_timing_event();
        active_timing_input_field_ = timing_input_field::none;
        timing_bar_pick_mode_ = false;
    }
}

void editor_scene::handle_timeline_interaction() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const Rectangle content = timeline_content_rect();
    const bool timeline_hovered = ui::is_hovered(content, ui::draw_layer::base);

    if (timing_bar_pick_mode_) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && timeline_hovered && selected_timing_event_index_.has_value()) {
            const int tick = snap_tick(timeline_y_to_tick(mouse.y));
            const bar_beat_position position = bar_beat_at_tick(tick);
            if (*selected_timing_event_index_ < state_.data().timing_events.size()) {
                const timing_event& event = state_.data().timing_events[*selected_timing_event_index_];
                const std::string value = std::to_string(position.measure) + ":" + std::to_string(position.beat);
                if (event.type == timing_event_type::bpm) {
                    timing_tick_input_ = value;
                } else {
                    timing_measure_input_ = value;
                }
                timing_input_error_.clear();
            }
            timing_bar_pick_mode_ = false;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE)) {
            timing_bar_pick_mode_ = false;
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        selected_note_index_ = timeline_hovered ? note_at_position(mouse) : std::nullopt;
    }

    if (!timeline_hovered) {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            note_dragging_ = false;
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const std::optional<int> lane = lane_at_position(mouse);
        if (lane.has_value()) {
            note_dragging_ = true;
            drag_lane_ = *lane;
            drag_start_tick_ = snap_tick(timeline_y_to_tick(mouse.y));
            drag_current_tick_ = drag_start_tick_;
        }
    }

    if (note_dragging_ && (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT))) {
        drag_current_tick_ = snap_tick(timeline_y_to_tick(mouse.y));
    }

    if (!note_dragging_ || !IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        return;
    }

    note_dragging_ = false;
    drag_current_tick_ = snap_tick(timeline_y_to_tick(mouse.y));

    note_data note;
    note.lane = drag_lane_;
    note.tick = std::min(drag_start_tick_, drag_current_tick_);
    note.end_tick = std::max(drag_start_tick_, drag_current_tick_);
    note.type = (note.end_tick - note.tick) >= snap_interval() ? note_type::hold : note_type::tap;
    if (note.type == note_type::tap) {
        note.end_tick = note.tick;
    }

    if (state_.has_note_overlap(note)) {
        return;
    }

    state_.add_note(note);
    selected_note_index_ = state_.data().notes.empty() ? std::nullopt : std::optional<size_t>(state_.data().notes.size() - 1);
}

void editor_scene::apply_scroll_and_zoom(float dt) {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const float wheel = GetMouseWheelMove();
    const Rectangle content = timeline_content_rect();
    const Rectangle track = timeline_scrollbar_track_rect();
    const ui::scrollbar_interaction scrollbar = ui::update_vertical_scrollbar(
        track, content_height_pixels(), scroll_offset_pixels(), scrollbar_dragging_, scrollbar_drag_offset_, 40.0f);
    bottom_tick_target_ = -kStartPaddingTicks + scrollbar.scroll_offset * ticks_per_pixel_;
    if (scrollbar.changed || scrollbar.dragging) {
        bottom_tick_ = bottom_tick_target_;
    }

    if (wheel == 0.0f || !CheckCollisionPointRec(mouse, content)) {
        bottom_tick_target_ = std::clamp(bottom_tick_target_, -kStartPaddingTicks, max_bottom_tick());
        if (bottom_tick_target_ <= -kStartPaddingTicks || bottom_tick_target_ >= max_bottom_tick()) {
            bottom_tick_ = bottom_tick_target_;
            return;
        }
        bottom_tick_ += (bottom_tick_target_ - bottom_tick_) * std::min(1.0f, kScrollLerpSpeed * dt);
        if (std::fabs(bottom_tick_ - bottom_tick_target_) < 0.5f) {
            bottom_tick_ = bottom_tick_target_;
        }
        return;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        const int anchor_tick = timeline_y_to_tick(mouse.y);
        const float zoom_scale = wheel > 0.0f ? 0.85f : 1.15f;
        ticks_per_pixel_ = std::clamp(ticks_per_pixel_ * zoom_scale, kMinTicksPerPixel, kMaxTicksPerPixel);
        bottom_tick_target_ = static_cast<float>(anchor_tick) - (mouse.y - content.y) * ticks_per_pixel_;
        bottom_tick_target_ = std::clamp(bottom_tick_target_, -kStartPaddingTicks, max_bottom_tick());
        bottom_tick_ = bottom_tick_target_;
        return;
    }

    bottom_tick_target_ = std::clamp(bottom_tick_target_ - wheel * visible_tick_span() * kScrollWheelViewportRatio,
                                     -kStartPaddingTicks, max_bottom_tick());
    if (bottom_tick_target_ <= -kStartPaddingTicks || bottom_tick_target_ >= max_bottom_tick()) {
        bottom_tick_ = bottom_tick_target_;
        return;
    }
    bottom_tick_ += (bottom_tick_target_ - bottom_tick_) * std::min(1.0f, kScrollLerpSpeed * dt);
    if (std::fabs(bottom_tick_ - bottom_tick_target_) < 0.5f) {
        bottom_tick_ = bottom_tick_target_;
    }
}

void editor_scene::select_timing_event(std::optional<size_t> index, bool scroll_into_view) {
    selected_timing_event_index_ = index;
    active_timing_input_field_ = timing_input_field::none;
    timing_input_error_.clear();
    timing_bar_pick_mode_ = false;
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
            if (row_top < timing_list_scroll_offset_) {
                timing_list_scroll_offset_ = row_top;
            } else if (row_bottom > timing_list_scroll_offset_ + kTimingListViewportHeight) {
                timing_list_scroll_offset_ = row_bottom - kTimingListViewportHeight;
            }
        }
    }
}

void editor_scene::scroll_to_tick(int tick) {
    const float target = std::clamp(static_cast<float>(tick) - visible_tick_span() * 0.5f,
                                    -kStartPaddingTicks, max_bottom_tick());
    bottom_tick_target_ = target;
    bottom_tick_ = target;
}

void editor_scene::sync_timing_event_selection() {
    if (selected_timing_event_index_.has_value() &&
        *selected_timing_event_index_ >= state_.data().timing_events.size()) {
        selected_timing_event_index_ = state_.data().timing_events.empty()
            ? std::nullopt
            : std::optional<size_t>(state_.data().timing_events.size() - 1);
    }
}

bool editor_scene::apply_selected_timing_event() {
    sync_timing_event_selection();
    if (!selected_timing_event_index_.has_value()) {
        timing_input_error_ = "Select a timing event first.";
        return false;
    }

    const size_t index = *selected_timing_event_index_;
    if (index >= state_.data().timing_events.size()) {
        timing_input_error_ = "Selected timing event is out of range.";
        return false;
    }

    timing_event updated = state_.data().timing_events[index];
    if (updated.type == timing_event_type::bpm) {
        bar_beat_position position;
        float bpm = 0.0f;
        if (!try_parse_bar_beat(timing_tick_input_, position)) {
            timing_input_error_ = "Bar must be in M:B format.";
            return false;
        }
        if (!try_parse_float(timing_bpm_input_, bpm) || bpm <= 0.0f) {
            timing_input_error_ = "BPM must be greater than zero.";
            return false;
        }
        const std::optional<int> tick = tick_from_bar_beat(position.measure, position.beat);
        if (!tick.has_value()) {
            timing_input_error_ = "Bar is outside the current meter layout.";
            return false;
        }
        updated.tick = *tick;
        updated.bpm = bpm;
    } else {
        bar_beat_position position;
        int numerator = 0;
        int denominator = 0;
        if (!try_parse_bar_beat(timing_measure_input_, position)) {
            timing_input_error_ = "Bar must be in M:B format.";
            return false;
        }
        if (!try_parse_int(timing_numerator_input_, numerator) || numerator <= 0) {
            timing_input_error_ = "Numerator must be 1 or greater.";
            return false;
        }
        if (!try_parse_int(timing_denominator_input_, denominator) || denominator <= 0) {
            timing_input_error_ = "Denominator must be 1 or greater.";
            return false;
        }
        const std::optional<int> tick = tick_from_bar_beat(position.measure, position.beat);
        if (!tick.has_value()) {
            timing_input_error_ = "Bar is outside the current meter layout.";
            return false;
        }
        updated.tick = *tick;
        updated.numerator = numerator;
        updated.denominator = denominator;
    }

    if (updated.type == timing_event_type::bpm && state_.data().timing_events[index].tick == 0 && updated.tick != 0) {
        timing_input_error_ = "The BPM event at tick 0 must stay at tick 0.";
        return false;
    }

    if (!state_.modify_timing_event(index, updated)) {
        timing_input_error_ = "Failed to update the timing event.";
        return false;
    }

    rebuild_meter_segments();
    timing_input_error_.clear();
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
        const bar_beat_position position = bar_beat_at_tick(event.tick);
        const std::optional<int> snapped_tick = tick_from_bar_beat(position.measure, 1);
        event.tick = snapped_tick.value_or(event.tick);
        event.bpm = 0.0f;
        event.numerator = 4;
        event.denominator = 4;
    }

    state_.add_timing_event(event);
    rebuild_meter_segments();
    select_timing_event(state_.data().timing_events.size() - 1, true);
}

void editor_scene::delete_selected_timing_event() {
    sync_timing_event_selection();
    if (!selected_timing_event_index_.has_value()) {
        timing_input_error_ = "Select a timing event first.";
        return;
    }
    if (!can_delete_selected_timing_event()) {
        timing_input_error_ = "The BPM event at tick 0 cannot be deleted.";
        return;
    }

    const size_t index = *selected_timing_event_index_;
    if (!state_.remove_timing_event(index)) {
        timing_input_error_ = "Failed to delete the timing event.";
        return;
    }

    rebuild_meter_segments();
    sync_timing_event_selection();
    timing_input_error_.clear();
    load_timing_event_inputs();
}

bool editor_scene::can_delete_selected_timing_event() const {
    if (!selected_timing_event_index_.has_value() || *selected_timing_event_index_ >= state_.data().timing_events.size()) {
        return false;
    }
    const timing_event& event = state_.data().timing_events[*selected_timing_event_index_];
    return !(event.type == timing_event_type::bpm && event.tick == 0);
}

void editor_scene::load_timing_event_inputs() {
    sync_timing_event_selection();
    if (!selected_timing_event_index_.has_value() || *selected_timing_event_index_ >= state_.data().timing_events.size()) {
        clear_timing_event_inputs();
        return;
    }

    const timing_event& event = state_.data().timing_events[*selected_timing_event_index_];
    timing_tick_input_ = bar_beat_label(event.tick);
    timing_bpm_input_ = TextFormat("%.1f", event.bpm);
    timing_measure_input_ = bar_beat_label(event.tick);
    timing_numerator_input_ = std::to_string(event.numerator);
    timing_denominator_input_ = std::to_string(event.denominator);
}

void editor_scene::clear_timing_event_inputs() {
    timing_tick_input_.clear();
    timing_bpm_input_.clear();
    timing_measure_input_.clear();
    timing_numerator_input_.clear();
    timing_denominator_input_.clear();
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

    const Rectangle meta_box = {content.x, content.y + 100.0f, content.width, 142.0f};
    ui::draw_section(meta_box);
    ui::draw_label_value({meta_box.x + 12.0f, meta_box.y + 12.0f, meta_box.width - 24.0f, 28.0f},
                         "Mode", TextFormat("%dK", state_.data().meta.key_count), 18, t.text_muted, t.text, 86.0f);
    ui::draw_label_value_marquee({meta_box.x + 12.0f, meta_box.y + 42.0f, meta_box.width - 24.0f, 28.0f},
                                 "Level", state_.data().meta.difficulty.c_str(), 18, t.text_muted, t.text, now, 86.0f);
    ui::draw_label_value_marquee({meta_box.x + 12.0f, meta_box.y + 72.0f, meta_box.width - 24.0f, 28.0f},
                                 "Author", state_.data().meta.chart_author.c_str(), 18, t.text_muted, t.text, now, 86.0f);
    ui::draw_label_value({meta_box.x + 12.0f, meta_box.y + 100.0f, meta_box.width - 24.0f, 28.0f},
                         "Status", status_label, 18, t.text_muted,
                         state_.is_dirty() ? t.error : t.success, 86.0f);

    const Rectangle tools_box = {content.x, meta_box.y + meta_box.height + 12.0f, content.width, 86.0f};
    ui::draw_section(tools_box);
    ui::draw_label_value({tools_box.x + 12.0f, tools_box.y + 16.0f, tools_box.width - 24.0f, 24.0f},
                         "Resolution", TextFormat("%d", state_.data().meta.resolution), 16,
                         t.text_secondary, t.text, 92.0f);
    ui::draw_label_value({tools_box.x + 12.0f, tools_box.y + 44.0f, tools_box.width - 24.0f, 24.0f},
                         "Notes", TextFormat("%d", static_cast<int>(state_.data().notes.size())), 16,
                         t.text_secondary, t.text, 92.0f);

    if (!load_errors_.empty()) {
        ui::draw_text_in_rect(load_errors_.front().c_str(), 18,
                              {content.x, tools_box.y + tools_box.height + 18.0f, content.width, 52.0f},
                              t.error, ui::text_align::left);
    }
}

void editor_scene::draw_right_panel() {
    const auto& t = *g_theme;
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const Rectangle content = ui::inset(kRightPanelRect, ui::edge_insets::uniform(16.0f));
    const Rectangle timing_box = {content.x, content.y, content.width, 262.0f};
    const Rectangle editor_box = {content.x, timing_box.y + timing_box.height + 12.0f, content.width, 238.0f};
    const Rectangle note_box = {content.x, editor_box.y + editor_box.height + 12.0f, content.width, 56.0f};
    bool clicked_input_row = false;

    auto draw_input_row = [&](Rectangle rect, const char* label, const std::string& value,
                              timing_input_field field, float label_width = 84.0f) {
        const bool selected = active_timing_input_field_ == field;
        const bool picking_bar = timing_bar_pick_mode_ &&
                                 (field == timing_input_field::bpm_measure || field == timing_input_field::meter_measure);
        const ui::row_state row = ui::draw_row(
            rect,
            selected ? t.row_selected : t.row,
            selected ? t.row_selected_hover : t.row_hover,
            selected ? t.border_active : t.border,
            1.5f);
        if (row.clicked) {
            active_timing_input_field_ = field;
            timing_bar_pick_mode_ = field == timing_input_field::bpm_measure ||
                                    field == timing_input_field::meter_measure;
            timing_input_error_.clear();
            clicked_input_row = true;
        }

        const Rectangle content_rect = ui::inset(row.visual, ui::edge_insets::symmetric(0.0f, 12.0f));
        const Rectangle label_rect = {content_rect.x, content_rect.y, label_width, content_rect.height};
        const Rectangle input_rect = {
            content_rect.x + label_width,
            content_rect.y + 4.0f,
            content_rect.width - label_width,
            content_rect.height - 8.0f
        };

        DrawRectangleRec(input_rect, selected ? with_alpha(t.panel, 255) : with_alpha(t.section, 255));
        DrawRectangleLinesEx(input_rect, 1.5f, picking_bar ? t.accent : (selected ? t.border_active : t.border_light));
        ui::draw_text_in_rect(label, 16, label_rect, selected ? t.text : t.text_secondary, ui::text_align::left);

        std::string display_value = value;
        if (display_value.empty()) {
            display_value = picking_bar ? "Click Timeline" : "Enter value";
        }
        if (picking_bar) {
            display_value = "Click Timeline";
        }
        if (selected && !picking_bar && (GetTime() * 2.0 - std::floor(GetTime() * 2.0)) < 0.5) {
            display_value += "_";
        }

        ui::draw_text_in_rect(display_value.c_str(), 16,
                              ui::inset(input_rect, ui::edge_insets::symmetric(0.0f, 10.0f)),
                              value.empty() && !selected ? t.text_hint : (picking_bar ? t.accent : t.text),
                              ui::text_align::left);
    };

    ui::draw_section(timing_box);
    ui::draw_text_in_rect("Timing Events", 22,
                          {timing_box.x + 12.0f, timing_box.y + 10.0f, timing_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);

    const Rectangle timing_list_view_rect = {
        timing_box.x + 10.0f,
        timing_box.y + 42.0f,
        timing_box.width - 32.0f,
        timing_box.height - 88.0f
    };
    const Rectangle timing_list_scrollbar_rect = {
        timing_list_view_rect.x + timing_list_view_rect.width + 6.0f,
        timing_list_view_rect.y,
        6.0f,
        timing_list_view_rect.height
    };
    const auto timing_indices = sorted_timing_event_indices();
    const float timing_row_height = 30.0f;
    const float timing_row_gap = 4.0f;
    const float timing_list_content_height = timing_indices.empty()
        ? timing_list_view_rect.height
        : static_cast<float>(timing_indices.size()) * timing_row_height +
              static_cast<float>(std::max<int>(0, static_cast<int>(timing_indices.size()) - 1)) * timing_row_gap;
    const float timing_list_max_scroll = std::max(0.0f, timing_list_content_height - timing_list_view_rect.height);
    timing_list_scroll_offset_ = std::clamp(timing_list_scroll_offset_, 0.0f, timing_list_max_scroll);

    const ui::scrollbar_interaction timing_scrollbar = ui::update_vertical_scrollbar(
        timing_list_scrollbar_rect, timing_list_content_height, timing_list_scroll_offset_,
        timing_list_scrollbar_dragging_, timing_list_scrollbar_drag_offset_, 28.0f);
    if (timing_scrollbar.changed || timing_scrollbar.dragging) {
        timing_list_scroll_offset_ = timing_scrollbar.scroll_offset;
    }

    if (CheckCollisionPointRec(mouse, timing_list_view_rect) && GetMouseWheelMove() != 0.0f) {
        timing_list_scroll_offset_ = std::clamp(
            timing_list_scroll_offset_ - GetMouseWheelMove() * 42.0f,
            0.0f, timing_list_max_scroll);
    }

    {
        ui::scoped_clip_rect clip_scope(timing_list_view_rect);
        float row_y = timing_list_view_rect.y - timing_list_scroll_offset_;
        for (const size_t index : timing_indices) {
            const timing_event& event = state_.data().timing_events[index];
            const bool selected = selected_timing_event_index_.has_value() && *selected_timing_event_index_ == index;
            const Rectangle row_rect = {timing_list_view_rect.x, row_y, timing_list_view_rect.width, timing_row_height};
            const ui::row_state row = ui::draw_selectable_row(row_rect, selected, 1.5f);
            if (row.clicked) {
                select_timing_event(index, true);
            }

            const std::string label = std::string(timing_event_type_label(event.type)) + " " + bar_beat_label(event.tick);
            const std::string value = event.type == timing_event_type::bpm
                ? TextFormat("%.1f", event.bpm)
                : TextFormat("%d/%d", event.numerator, event.denominator);
            ui::draw_label_value(ui::inset(row.visual, ui::edge_insets::symmetric(0.0f, 10.0f)),
                                 label.c_str(), value.c_str(), 15,
                                 selected ? t.text : t.text_secondary,
                                 selected ? t.text : t.text_muted, 118.0f);
            row_y += timing_row_height + timing_row_gap;
        }
    }
    ui::draw_scrollbar(timing_list_scrollbar_rect, timing_list_content_height, timing_list_scroll_offset_,
                       t.scrollbar_track, t.scrollbar_thumb, 28.0f);

    const float timing_button_gap = 8.0f;
    const float timing_button_width = (timing_box.width - 24.0f - timing_button_gap * 2.0f) / 3.0f;
    const Rectangle add_bpm_rect = {
        timing_box.x + 12.0f,
        timing_box.y + timing_box.height - 42.0f,
        timing_button_width,
        28.0f
    };
    const Rectangle add_meter_rect = {
        add_bpm_rect.x + timing_button_width + timing_button_gap,
        add_bpm_rect.y,
        timing_button_width,
        28.0f
    };
    const Rectangle delete_rect = {
        add_meter_rect.x + timing_button_width + timing_button_gap,
        add_bpm_rect.y,
        timing_button_width,
        28.0f
    };
    if (ui::draw_button(add_bpm_rect, "BPM", 14).clicked) {
        add_timing_event(timing_event_type::bpm);
    }
    if (ui::draw_button(add_meter_rect, "Meter", 14).clicked) {
        add_timing_event(timing_event_type::meter);
    }
    const bool delete_enabled = can_delete_selected_timing_event();
    const ui::button_state delete_button = ui::draw_button_colored(
        delete_rect, "Delete", 14,
        delete_enabled ? t.row : t.section,
        delete_enabled ? t.row_hover : t.section,
        delete_enabled ? t.text : t.text_hint, 1.5f);
    if (delete_enabled && delete_button.clicked) {
        delete_selected_timing_event();
    }

    ui::draw_section(editor_box);
    ui::draw_text_in_rect("Event Editor", 22,
                          {editor_box.x + 12.0f, editor_box.y + 10.0f, editor_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);

    sync_timing_event_selection();
    if (selected_timing_event_index_.has_value() && *selected_timing_event_index_ < state_.data().timing_events.size()) {
        const timing_event& event = state_.data().timing_events[*selected_timing_event_index_];
        ui::draw_label_value({editor_box.x + 12.0f, editor_box.y + 44.0f, editor_box.width - 24.0f, 22.0f},
                             "Type", timing_event_type_label(event.type), 16,
                             t.text_secondary, t.text, 76.0f);
        if (event.type == timing_event_type::bpm) {
            draw_input_row({editor_box.x + 12.0f, editor_box.y + 74.0f, editor_box.width - 24.0f, 32.0f},
                           "Bar", timing_tick_input_, timing_input_field::bpm_measure);
            draw_input_row({editor_box.x + 12.0f, editor_box.y + 112.0f, editor_box.width - 24.0f, 32.0f},
                           "BPM", timing_bpm_input_, timing_input_field::bpm_value);
        } else {
            draw_input_row({editor_box.x + 12.0f, editor_box.y + 74.0f, editor_box.width - 24.0f, 32.0f},
                           "Bar", timing_measure_input_, timing_input_field::meter_measure);
            draw_input_row({editor_box.x + 12.0f, editor_box.y + 112.0f, (editor_box.width - 32.0f) * 0.5f, 32.0f},
                           "Num", timing_numerator_input_, timing_input_field::meter_numerator, 40.0f);
            draw_input_row({editor_box.x + 20.0f + (editor_box.width - 32.0f) * 0.5f, editor_box.y + 112.0f,
                            (editor_box.width - 32.0f) * 0.5f, 32.0f},
                           "Den", timing_denominator_input_, timing_input_field::meter_denominator, 40.0f);
        }

        if (!timing_input_error_.empty()) {
            ui::draw_text_in_rect(timing_input_error_.c_str(), 16,
                                  {editor_box.x + 12.0f, editor_box.y + 182.0f, editor_box.width - 24.0f, 36.0f},
                                  t.error, ui::text_align::left);
        }

        if (ui::draw_button({editor_box.x + editor_box.width - 92.0f, editor_box.y + editor_box.height - 42.0f,
                             80.0f, 28.0f}, "Apply", 14).clicked) {
            apply_selected_timing_event();
            active_timing_input_field_ = timing_input_field::none;
            timing_bar_pick_mode_ = false;
        }
    } else {
        ui::draw_text_in_rect("Select a timing event from the list.", 18,
                              {editor_box.x + 12.0f, editor_box.y + 54.0f, editor_box.width - 24.0f, 24.0f},
                              t.text_hint, ui::text_align::left);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        active_timing_input_field_ != timing_input_field::none &&
        !clicked_input_row &&
        !CheckCollisionPointRec(mouse, editor_box)) {
        active_timing_input_field_ = timing_input_field::none;
        timing_bar_pick_mode_ = false;
    }

    ui::draw_section(note_box);
    if (selected_note_index_.has_value() && *selected_note_index_ < state_.data().notes.size()) {
        const note_data& note = state_.data().notes[*selected_note_index_];
        ui::draw_label_value({note_box.x + 12.0f, note_box.y + 8.0f, note_box.width - 24.0f, 18.0f},
                             "Note", note_type_label(note.type), 15,
                             t.text_secondary, t.text, 56.0f);
        ui::draw_label_value({note_box.x + 12.0f, note_box.y + 28.0f, note_box.width - 24.0f, 18.0f},
                             "Tick", TextFormat("%d  lane %d", note.tick, note.lane + 1), 15,
                             t.text_secondary, t.text_muted, 56.0f);
    } else {
        ui::draw_label_value({note_box.x + 12.0f, note_box.y + 8.0f, note_box.width - 24.0f, 18.0f},
                             "Notes", TextFormat("%d", static_cast<int>(state_.data().notes.size())), 15,
                             t.text_secondary, t.text, 56.0f);
        ui::draw_label_value({note_box.x + 12.0f, note_box.y + 28.0f, note_box.width - 24.0f, 18.0f},
                             "Undo", state_.can_undo() ? "Available" : "Empty", 15,
                             t.text_secondary, state_.can_undo() ? t.success : t.text, 56.0f);
    }
}

void editor_scene::draw_timeline() const {
    const auto& t = *g_theme;
    const int min_tick = std::max(0, static_cast<int>(std::floor(bottom_tick_ - kMinVisibleTicks * 0.1f)));
    const int max_tick = static_cast<int>(std::ceil(bottom_tick_ + visible_tick_span()));
    const Rectangle content = timeline_content_rect();
    const Rectangle track = timeline_scrollbar_track_rect();

    DrawRectangleRec(ui::inset(kTimelineRect, 10.0f), t.section);
    {
        ui::scoped_clip_rect clip_scope(content);
        draw_timeline_grid(min_tick, max_tick);
        draw_timeline_notes();
        if (note_dragging_) {
            note_data preview;
            preview.lane = drag_lane_;
            preview.tick = std::min(drag_start_tick_, drag_current_tick_);
            preview.end_tick = std::max(drag_start_tick_, drag_current_tick_);
            preview.type = (preview.end_tick - preview.tick) >= snap_interval() ? note_type::hold : note_type::tap;
            if (preview.type == note_type::tap) {
                preview.end_tick = preview.tick;
            }

            const note_draw_info info = note_rects(preview);
            const Color fill = state_.has_note_overlap(preview) ? with_alpha(t.error, 150) : with_alpha(t.success, 150);
            const Color outline = state_.has_note_overlap(preview) ? t.error : t.success;
            if (info.has_body) {
                DrawRectangleRounded(info.body_rect, 0.4f, 6, fill);
                DrawRectangleRounded(info.tail_rect, 0.4f, 6, fill);
                DrawRectangleLinesEx(info.tail_rect, 1.5f, outline);
            }
            DrawRectangleRounded(info.head_rect, 0.3f, 6, fill);
            DrawRectangleLinesEx(info.head_rect, 1.5f, outline);
        }
    }
    ui::draw_scrollbar(track, content_height_pixels(), scroll_offset_pixels(),
                       t.scrollbar_track, t.scrollbar_thumb, 40.0f);

    DrawRectangleLinesEx(kTimelineRect, 2.0f, t.border);
}

void editor_scene::draw_timeline_grid(int min_tick, int max_tick) const {
    const auto& t = *g_theme;
    const int key_count = std::max(1, state_.data().meta.key_count);
    const Rectangle content = timeline_content_rect();

    for (int lane = 0; lane < key_count; ++lane) {
        const Rectangle rect = lane_rect(lane);
        DrawRectangleRec(rect, lane % 2 == 0 ? with_alpha(t.row, 100) : with_alpha(t.section, 110));
        DrawRectangleLinesEx(rect, 1.0f, with_alpha(t.border_light, 180));
        ui::draw_text_in_rect(TextFormat("L%d", lane + 1), 16,
                              {rect.x, kTimelineRect.y + 4.0f, rect.width, 20.0f},
                              t.text_hint);
    }

    const int interval = snap_interval();
    const int first_snap_tick = std::max(0, (min_tick / interval) * interval);
    for (int tick = first_snap_tick; tick <= max_tick; tick += interval) {
        const float y = tick_to_timeline_y(tick);
        ui::draw_line_f(content.x, y, content.x + content.width, y, t.editor_grid_snap);
    }

    for (const grid_line& line : visible_grid_lines(min_tick, max_tick)) {
        const float y = tick_to_timeline_y(line.tick);
        const Color color = line.major ? t.editor_grid_major : t.editor_grid_minor;
        ui::draw_line_f(content.x, y, content.x + content.width, y, color);
        if (line.major) {
            ui::draw_line_f(content.x, y + 1.0f, content.x + content.width, y + 1.0f,
                            t.editor_grid_major_glow);
        }
        ui::draw_text_f(TextFormat("%d:%d", line.measure, line.beat), content.x + 8.0f, y - 10.0f,
                        line.major ? 16 : 14, line.major ? t.text : t.text_secondary);
    }
}

void editor_scene::draw_timeline_notes() const {
    const auto& t = *g_theme;
    for (size_t i = 0; i < state_.data().notes.size(); ++i) {
        const note_data& note = state_.data().notes[i];
        if (note.lane < 0 || note.lane >= state_.data().meta.key_count) {
            continue;
        }

        const note_draw_info info = note_rects(note);
        const bool selected = selected_note_index_.has_value() && *selected_note_index_ == i;
        const Color head_fill = selected ? t.row_active : t.note_color;
        const Color outline = selected ? t.border_active : t.note_outline;
        const Color hold_fill = selected ? with_alpha(t.row_active, 200) : with_alpha(t.accent, 170);

        if (info.has_body) {
            DrawRectangleRounded(info.body_rect, 0.4f, 6, hold_fill);
            DrawRectangleRounded(info.tail_rect, 0.4f, 6, selected ? with_alpha(t.row_active, 230) : with_alpha(t.accent, 220));
            DrawRectangleLinesEx(info.tail_rect, 1.5f, outline);
        }

        DrawRectangleRounded(info.head_rect, 0.3f, 6, head_fill);
        DrawRectangleLinesEx(info.head_rect, 1.5f, outline);
    }
}

void editor_scene::draw_cursor_hud() const {
    const auto& t = *g_theme;
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    if (!CheckCollisionPointRec(mouse, kTimelineRect)) {
        return;
    }

    const int tick = std::max(0, timeline_y_to_tick(mouse.y));
    const int snapped_tick = snap_tick(tick);
    const double beat = beat_number_at_tick(tick);
    const int whole_beat = std::max(0, static_cast<int>(std::floor(beat - 1.0)));
    const auto it = std::upper_bound(meter_segments_.begin(), meter_segments_.end(), tick,
                                     [](int value, const meter_segment& segment) {
                                         return value < segment.start_tick;
                                     });
    const meter_segment& segment = it == meter_segments_.begin() ? meter_segments_.front() : *std::prev(it);
    const int numerator = std::max(segment.numerator, 1);
    const int measure = segment.measure_index_offset + (whole_beat - segment.beat_index_offset) / numerator + 1;
    const int beat_in_measure = (whole_beat - segment.beat_index_offset) % numerator + 1;
    const Rectangle hud_rect = ui::place(kTimelineRect, 340.0f, 34.0f,
                                         ui::anchor::bottom_left, ui::anchor::bottom_left,
                                         {12.0f, -12.0f});
    DrawRectangleRec(hud_rect, with_alpha(t.panel, 240));
    DrawRectangleLinesEx(hud_rect, 1.5f, t.border);
    ui::draw_text_f(TextFormat("bar %d:%d   beat %.2f   snap %d", measure, beat_in_measure, beat, snapped_tick),
                    hud_rect.x + 12.0f, hud_rect.y + 8.0f, 18, t.text);
}

void editor_scene::draw_header_tools() {
    // 描画は queue に寄せるが、ヒットテストはまだ即時計算のままにしている。
    // 次段で layer と hit test 優先順位を統合すると、modal / pause 系も同じ仕組みに載せられる。
    const ui::dropdown_state dropdown = ui::enqueue_dropdown(
        kSnapDropdownRect, kSnapDropdownMenuRect,
        "Tools", kSnapLabels[snap_index_],
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
