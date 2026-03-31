#include "editor_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
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
    note_dragging_ = false;
    drag_lane_ = 0;
    drag_start_tick_ = 0;
    drag_current_tick_ = 0;
    rebuild_meter_segments();

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
    if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        return;
    }

    if (ui::is_clicked(kBackButtonRect)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        return;
    }

    handle_shortcuts();
    handle_timeline_interaction();
    apply_scroll_and_zoom(dt);
}

void editor_scene::draw() {
    const auto& t = *g_theme;
    const double now = GetTime();
    virtual_screen::begin();
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
    draw_cursor_hud();
    draw_header_tools();

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

std::string editor_scene::bar_beat_label(int tick) const {
    if (meter_segments_.empty()) {
        return "1:1";
    }

    const auto it = std::upper_bound(meter_segments_.begin(), meter_segments_.end(), tick,
                                     [](int value, const meter_segment& segment) {
                                         return value < segment.start_tick;
                                     });
    const meter_segment& segment = it == meter_segments_.begin() ? meter_segments_.front() : *std::prev(it);
    const int numerator = std::max(segment.numerator, 1);
    const int beat_ticks = std::max(1, state_.data().meta.resolution * 4 / std::max(segment.denominator, 1));
    const int local_beat_index = std::max(0, (tick - segment.start_tick) / beat_ticks);
    const int measure = segment.measure_index_offset + local_beat_index / numerator + 1;
    const int beat_in_measure = local_beat_index % numerator + 1;
    return std::to_string(measure) + ":" + std::to_string(beat_in_measure);
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
        rebuild_meter_segments();
    }

    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Y)) {
        state_.redo();
        selected_note_index_.reset();
        rebuild_meter_segments();
    }

    if (IsKeyPressed(KEY_DELETE) && selected_note_index_.has_value()) {
        const size_t selected_index = *selected_note_index_;
        if (state_.remove_note(selected_index)) {
            selected_note_index_.reset();
        }
    }

    if (selected_note_index_.has_value() && *selected_note_index_ >= state_.data().notes.size()) {
        selected_note_index_.reset();
    }
}

void editor_scene::handle_timeline_interaction() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const Rectangle content = timeline_content_rect();

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        selected_note_index_ = note_at_position(mouse);
    }

    if (!CheckCollisionPointRec(mouse, content)) {
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

void editor_scene::draw_right_panel() const {
    const auto& t = *g_theme;
    const Rectangle content = ui::inset(kRightPanelRect, ui::edge_insets::uniform(16.0f));
    const Rectangle timing_box = {content.x, content.y, content.width, 252.0f};
    const Rectangle property_box = {content.x, timing_box.y + timing_box.height + 12.0f, content.width, 198.0f};

    ui::draw_section(timing_box);
    ui::draw_text_in_rect("Timing Events", 22,
                          {timing_box.x + 12.0f, timing_box.y + 10.0f, timing_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);

    float row_y = timing_box.y + 42.0f;
    for (const timing_event& event : state_.data().timing_events) {
        if (row_y + 24.0f > timing_box.y + timing_box.height - 8.0f) {
            break;
        }

        const std::string label = event.type == timing_event_type::bpm
            ? "BPM " + std::to_string(static_cast<int>(std::round(event.bpm)))
            : "METER " + std::to_string(event.numerator) + "/" + std::to_string(event.denominator);
        const std::string position = bar_beat_label(event.tick);
        ui::draw_label_value({timing_box.x + 12.0f, row_y, timing_box.width - 24.0f, 22.0f},
                             label.c_str(), position.c_str(), 16,
                             t.text_secondary, t.text_muted, 118.0f);
        row_y += 24.0f;
    }

    ui::draw_section(property_box);
    ui::draw_text_in_rect("Properties", 22,
                          {property_box.x + 12.0f, property_box.y + 10.0f, property_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);
    if (selected_note_index_.has_value() && *selected_note_index_ < state_.data().notes.size()) {
        const note_data& note = state_.data().notes[*selected_note_index_];
        ui::draw_label_value({property_box.x + 12.0f, property_box.y + 50.0f, property_box.width - 24.0f, 24.0f},
                             "Type", note_type_label(note.type), 16,
                             t.text_secondary, t.text, 82.0f);
        ui::draw_label_value({property_box.x + 12.0f, property_box.y + 76.0f, property_box.width - 24.0f, 24.0f},
                             "Tick", TextFormat("%d", note.tick), 16,
                             t.text_secondary, t.text, 82.0f);
        ui::draw_label_value({property_box.x + 12.0f, property_box.y + 102.0f, property_box.width - 24.0f, 24.0f},
                             "Lane", TextFormat("%d", note.lane + 1), 16,
                             t.text_secondary, t.text, 82.0f);
        ui::draw_label_value({property_box.x + 12.0f, property_box.y + 128.0f, property_box.width - 24.0f, 24.0f},
                             "End", note.type == note_type::hold ? TextFormat("%d", note.end_tick) : "-", 16,
                             t.text_secondary, t.text, 82.0f);
    } else {
        ui::draw_text_in_rect("Right click a note to inspect it.", 18,
                              {property_box.x + 12.0f, property_box.y + 50.0f, property_box.width - 24.0f, 22.0f},
                              t.text_hint, ui::text_align::left);
    }

    ui::draw_label_value({property_box.x + 12.0f, property_box.y + 144.0f, property_box.width - 24.0f, 24.0f},
                         "Notes", TextFormat("%d", static_cast<int>(state_.data().notes.size())), 16,
                         t.text_secondary, t.text, 82.0f);
    ui::draw_label_value({property_box.x + 12.0f, property_box.y + 170.0f, property_box.width - 24.0f, 24.0f},
                         "Undo", state_.can_undo() ? "Available" : "Empty", 16,
                         t.text_secondary, state_.can_undo() ? t.success : t.text, 82.0f);
}

void editor_scene::draw_timeline() const {
    const auto& t = *g_theme;
    const int min_tick = std::max(0, static_cast<int>(std::floor(bottom_tick_ - kMinVisibleTicks * 0.1f)));
    const int max_tick = static_cast<int>(std::ceil(bottom_tick_ + visible_tick_span()));
    const Rectangle content = timeline_content_rect();
    const Rectangle track = timeline_scrollbar_track_rect();

    DrawRectangleRec(ui::inset(kTimelineRect, 10.0f), t.section);
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
        ui::draw_line_f(content.x, y, content.x + content.width, y, with_alpha(t.border_light, 80));
    }

    for (const grid_line& line : visible_grid_lines(min_tick, max_tick)) {
        const float y = tick_to_timeline_y(line.tick);
        const Color color = line.major ? with_alpha(t.border_active, 255) : with_alpha(t.border_light, 220);
        ui::draw_line_f(content.x, y, content.x + content.width, y, color);
        if (line.major) {
            ui::draw_line_f(content.x, y + 1.0f, content.x + content.width, y + 1.0f,
                            with_alpha(t.border_active, 180));
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
    const auto& t = *g_theme;
    const Rectangle tools_rect = ui::place(kHeaderRect, 360.0f, 34.0f,
                                           ui::anchor::center_right, ui::anchor::center_right,
                                           {-18.0f, 0.0f});
    const Rectangle dropdown_rect = tools_rect;
    const Rectangle dropdown_menu_rect = {
        dropdown_rect.x,
        dropdown_rect.y + dropdown_rect.height + 4.0f,
        dropdown_rect.width,
        12.0f + static_cast<float>(std::size(kSnapLabels)) * 30.0f + static_cast<float>(std::size(kSnapLabels) - 1) * 4.0f
    };
    const ui::dropdown_state dropdown = ui::draw_dropdown(dropdown_rect, dropdown_menu_rect,
                                                          "Tools", kSnapLabels[snap_index_],
                                                          std::span<const char* const>(kSnapLabels, std::size(kSnapLabels)),
                                                          snap_index_, snap_dropdown_open_, 16, 64.0f);
    if (dropdown.trigger.clicked) {
        snap_dropdown_open_ = !snap_dropdown_open_;
    }
    if (dropdown.clicked_index >= 0) {
        snap_index_ = dropdown.clicked_index;
        snap_dropdown_open_ = false;
    } else if (snap_dropdown_open_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
               !ui::is_hovered(dropdown_rect) &&
               !CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), dropdown_menu_rect)) {
        snap_dropdown_open_ = false;
    }
}
