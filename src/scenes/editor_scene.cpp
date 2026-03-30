#include "editor_scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>

#include "audio.h"
#include "chart_parser.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "theme.h"
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

void draw_label_value_marquee(Rectangle rect, const char* label, const char* value,
                              int font_size, Color label_color, Color value_color,
                              float label_width, double time) {
    const Rectangle label_rect = {rect.x, rect.y, label_width, rect.height};
    const Rectangle value_rect = {rect.x + label_width, rect.y, rect.width - label_width, rect.height};
    ui::draw_text_in_rect(label, font_size, label_rect, label_color, ui::text_align::left);
    draw_marquee_text(value, static_cast<int>(value_rect.x), static_cast<int>(value_rect.y + 4.0f), font_size,
                      value_color, value_rect.width, time);
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
    ticks_per_pixel_ = 2.0f;
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
    (void) dt;

    if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        return;
    }

    if (ui::is_clicked(kBackButtonRect)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_));
        return;
    }

    apply_scroll_and_zoom();
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
    const Rectangle title_rect = ui::place(kHeaderRect, 420.0f, 24.0f,
                                           ui::anchor::center_right, ui::anchor::center_right,
                                           {-20.0f, 0.0f});
    draw_marquee_text(song_.meta.title.c_str(), static_cast<int>(title_rect.x), static_cast<int>(title_rect.y + 2.0f),
                      20, t.text_secondary, title_rect.width, now);

    draw_left_panel();
    draw_timeline();
    draw_right_panel();
    draw_cursor_hud();

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

void editor_scene::apply_scroll_and_zoom() {
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const float wheel = GetMouseWheelMove();
    const Rectangle content = timeline_content_rect();
    const Rectangle track = timeline_scrollbar_track_rect();

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        scrollbar_dragging_ = false;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, track)) {
        const ui::scroll_metrics metrics = ui::vertical_scroll_metrics(track, content_height_pixels(), scroll_offset_pixels(), 40.0f);
        if (CheckCollisionPointRec(mouse, metrics.thumb_rect)) {
            scrollbar_dragging_ = true;
            scrollbar_drag_offset_ = mouse.y - metrics.thumb_rect.y;
        } else {
            const float thumb_half = metrics.thumb_rect.height * 0.5f;
            const float available = std::max(1.0f, track.height - metrics.thumb_rect.height);
            const float thumb_top = std::clamp(mouse.y - thumb_half - track.y, 0.0f, available);
            bottom_tick_ = -kStartPaddingTicks + (max_bottom_tick() + kStartPaddingTicks) * (thumb_top / available);
        }
    }

    if (scrollbar_dragging_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        const ui::scroll_metrics metrics = ui::vertical_scroll_metrics(track, content_height_pixels(), scroll_offset_pixels(), 40.0f);
        const float available = std::max(1.0f, track.height - metrics.thumb_rect.height);
        const float thumb_top = std::clamp(mouse.y - scrollbar_drag_offset_ - track.y, 0.0f, available);
        bottom_tick_ = -kStartPaddingTicks + (max_bottom_tick() + kStartPaddingTicks) * (thumb_top / available);
    }

    if (wheel == 0.0f || !CheckCollisionPointRec(mouse, content)) {
        bottom_tick_ = std::clamp(bottom_tick_, -kStartPaddingTicks, max_bottom_tick());
        return;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        const int anchor_tick = timeline_y_to_tick(mouse.y);
        const float zoom_scale = wheel > 0.0f ? 0.85f : 1.15f;
        ticks_per_pixel_ = std::clamp(ticks_per_pixel_ * zoom_scale, kMinTicksPerPixel, kMaxTicksPerPixel);
        bottom_tick_ = static_cast<float>(anchor_tick) - (mouse.y - content.y) * ticks_per_pixel_;
        bottom_tick_ = std::clamp(bottom_tick_, -kStartPaddingTicks, max_bottom_tick());
        return;
    }

    bottom_tick_ = std::clamp(bottom_tick_ - wheel * visible_tick_span() * 0.08f, -kStartPaddingTicks, max_bottom_tick());
}

void editor_scene::draw_left_panel() const {
    const auto& t = *g_theme;
    const double now = GetTime();
    const Rectangle content = ui::inset(kLeftPanelRect, ui::edge_insets::uniform(16.0f));
    const bool has_file = !state_.file_path().empty();
    const char* status_label = state_.is_dirty() ? "Modified" : (has_file ? "Saved" : "Unsaved");

    ui::draw_header_block(ui::place(content, content.width, 54.0f, ui::anchor::top_left, ui::anchor::top_left),
                          "Chart", has_file ? "Existing chart" : "New chart", 28, 18, 4.0f);

    const Rectangle meta_box = {content.x, content.y + 72.0f, content.width, 142.0f};
    ui::draw_section(meta_box);
    ui::draw_label_value({meta_box.x + 12.0f, meta_box.y + 12.0f, meta_box.width - 24.0f, 28.0f},
                         "Mode", TextFormat("%dK", state_.data().meta.key_count), 18, t.text_muted, t.text, 86.0f);
    draw_label_value_marquee({meta_box.x + 12.0f, meta_box.y + 42.0f, meta_box.width - 24.0f, 28.0f},
                             "Level", state_.data().meta.difficulty.c_str(), 18, t.text_muted, t.text, 86.0f, now);
    draw_label_value_marquee({meta_box.x + 12.0f, meta_box.y + 72.0f, meta_box.width - 24.0f, 28.0f},
                             "Author", state_.data().meta.chart_author.c_str(), 18, t.text_muted, t.text, 86.0f, now);
    ui::draw_label_value({meta_box.x + 12.0f, meta_box.y + 100.0f, meta_box.width - 24.0f, 28.0f},
                         "Status", status_label, 18, t.text_muted,
                         state_.is_dirty() ? t.error : t.success, 86.0f);

    const Rectangle tools_box = {content.x, meta_box.y + meta_box.height + 12.0f, content.width, 152.0f};
    ui::draw_section(tools_box);
    ui::draw_text_in_rect("Tools", 22,
                          {tools_box.x + 12.0f, tools_box.y + 10.0f, tools_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);
    ui::draw_button_colored({tools_box.x + 12.0f, tools_box.y + 42.0f, tools_box.width - 24.0f, 30.0f},
                            "ADD NOTE", 18, t.row, t.row_hover, t.text_hint);
    ui::draw_button_colored({tools_box.x + 12.0f, tools_box.y + 78.0f, tools_box.width - 24.0f, 30.0f},
                            "TIMING", 18, t.row, t.row_hover, t.text_hint);
    ui::draw_button_colored({tools_box.x + 12.0f, tools_box.y + 114.0f, tools_box.width - 24.0f, 30.0f},
                            "SAVE", 18, t.row, t.row_hover, t.text_hint);

    if (!load_errors_.empty()) {
        ui::draw_text_in_rect(load_errors_.front().c_str(), 18,
                              {content.x, tools_box.y + tools_box.height + 18.0f, content.width, 52.0f},
                              t.error, ui::text_align::left);
    }
}

void editor_scene::draw_right_panel() const {
    const auto& t = *g_theme;
    const double now = GetTime();
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
        draw_label_value_marquee({timing_box.x + 12.0f, row_y, timing_box.width - 24.0f, 22.0f},
                                 label.c_str(), position.c_str(), 16,
                                 t.text_secondary, t.text_muted, 118.0f, now);
        row_y += 24.0f;
    }

    ui::draw_section(property_box);
    ui::draw_text_in_rect("Properties", 22,
                          {property_box.x + 12.0f, property_box.y + 10.0f, property_box.width - 24.0f, 28.0f},
                          t.text, ui::text_align::left);
    ui::draw_text_in_rect("Selection pending", 18,
                          {property_box.x + 12.0f, property_box.y + 50.0f, property_box.width - 24.0f, 22.0f},
                          t.text_hint, ui::text_align::left);
    ui::draw_label_value({property_box.x + 12.0f, property_box.y + 92.0f, property_box.width - 24.0f, 24.0f},
                         "Notes", TextFormat("%d", static_cast<int>(state_.data().notes.size())), 16,
                         t.text_secondary, t.text, 82.0f);
    ui::draw_label_value({property_box.x + 12.0f, property_box.y + 118.0f, property_box.width - 24.0f, 24.0f},
                         "Resolution", TextFormat("%d", state_.data().meta.resolution), 16,
                         t.text_secondary, t.text, 82.0f);
    ui::draw_label_value({property_box.x + 12.0f, property_box.y + 144.0f, property_box.width - 24.0f, 24.0f},
                         "Undo", state_.can_undo() ? "Available" : "Empty", 16,
                         t.text_secondary, t.text, 82.0f);
    ui::draw_label_value({property_box.x + 12.0f, property_box.y + 170.0f, property_box.width - 24.0f, 24.0f},
                         "Redo", state_.can_redo() ? "Available" : "Empty", 16,
                         t.text_secondary, t.text, 82.0f);
}

void editor_scene::draw_timeline() const {
    const auto& t = *g_theme;
    const int min_tick = std::max(0, static_cast<int>(std::floor(bottom_tick_ - kMinVisibleTicks * 0.1f)));
    const int max_tick = static_cast<int>(std::ceil(bottom_tick_ + visible_tick_span()));
    const Rectangle content = timeline_content_rect();
    const Rectangle track = timeline_scrollbar_track_rect();

    DrawRectangleRec(ui::inset(kTimelineRect, 10.0f), t.section);
    BeginScissorMode(static_cast<int>(content.x), static_cast<int>(content.y),
                     static_cast<int>(content.width), static_cast<int>(content.height));
    draw_timeline_grid(min_tick, max_tick);
    draw_timeline_notes();
    EndScissorMode();

    ui::draw_scrollbar(track, content_height_pixels(), scroll_offset_pixels(),
                       t.scrollbar_track, t.scrollbar_thumb, 40.0f);

    DrawRectangleLinesEx(kTimelineRect, 2.0f, t.border);
}

void editor_scene::draw_timeline_grid(int min_tick, int max_tick) const {
    const auto& t = *g_theme;
    const int key_count = std::max(1, state_.data().meta.key_count);

    for (int lane = 0; lane < key_count; ++lane) {
        const Rectangle rect = lane_rect(lane);
        DrawRectangleRec(rect, lane % 2 == 0 ? with_alpha(t.row, 100) : with_alpha(t.section, 110));
        DrawRectangleLinesEx(rect, 1.0f, with_alpha(t.border_light, 180));
        ui::draw_text_in_rect(TextFormat("L%d", lane + 1), 16,
                              {rect.x, kTimelineRect.y + 4.0f, rect.width, 20.0f},
                              t.text_hint);
    }

    for (const grid_line& line : visible_grid_lines(min_tick, max_tick)) {
        const float y = tick_to_timeline_y(line.tick);
        const Color color = line.major ? t.border_active : t.border_light;
        const Rectangle content = timeline_content_rect();
        DrawLine(static_cast<int>(content.x), static_cast<int>(y),
                 static_cast<int>(content.x + content.width), static_cast<int>(y), color);
        DrawText(TextFormat("%d:%d", line.measure, line.beat), static_cast<int>(content.x + 8.0f), static_cast<int>(y - 10.0f),
                 line.major ? 16 : 14, line.major ? t.text_secondary : t.text_hint);
    }
}

void editor_scene::draw_timeline_notes() const {
    const auto& t = *g_theme;
    for (const note_data& note : state_.data().notes) {
        if (note.lane < 0 || note.lane >= state_.data().meta.key_count) {
            continue;
        }

        const Rectangle lane = lane_rect(note.lane);
        const float start_y = tick_to_timeline_y(note.tick);
        const float head_height = 14.0f;
        const Rectangle head_rect = {lane.x + 6.0f, start_y - head_height * 0.5f, lane.width - 12.0f, head_height};

        if (note.type == note_type::hold) {
            const float end_y = tick_to_timeline_y(note.end_tick);
            const float top = std::min(start_y, end_y);
            const float height = std::fabs(end_y - start_y);
            const Rectangle hold_rect = {lane.x + lane.width * 0.3f, top, lane.width * 0.4f, std::max(height, 6.0f)};
            const Rectangle tail_rect = {lane.x + 10.0f, end_y - 5.0f, lane.width - 20.0f, 10.0f};
            DrawRectangleRounded(hold_rect, 0.4f, 6, with_alpha(t.accent, 170));
            DrawRectangleRounded(tail_rect, 0.4f, 6, with_alpha(t.accent, 220));
            DrawRectangleLinesEx(tail_rect, 1.5f, t.note_outline);
        }

        DrawRectangleRounded(head_rect, 0.3f, 6, t.note_color);
        DrawRectangleLinesEx(head_rect, 1.5f, t.note_outline);
    }
}

void editor_scene::draw_cursor_hud() const {
    const auto& t = *g_theme;
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    if (!CheckCollisionPointRec(mouse, kTimelineRect)) {
        return;
    }

    const int tick = std::max(0, timeline_y_to_tick(mouse.y));
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
    DrawText(TextFormat("bar %d:%d   beat %.2f", measure, beat_in_measure, beat),
             static_cast<int>(hud_rect.x + 12.0f), static_cast<int>(hud_rect.y + 8.0f), 18, t.text);
}
