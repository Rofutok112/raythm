#include "editor_timeline_types.h"

#include <algorithm>
#include <cmath>

#include "game_settings.h"

namespace {

bool timeline_note_has_duration(editor_timeline_note_type type) {
    return type == editor_timeline_note_type::hold ||
           type == editor_timeline_note_type::decorative_hold;
}

}  // namespace

Rectangle editor_timeline_metrics::content_rect() const {
    constexpr float kLeftLaneWidth = 60.0f;
    constexpr float kLaneGap = 8.0f;
    constexpr float kAutomationWidth = 260.0f;
    return {
        panel_rect.x + padding + scrollbar_width + kLaneGap + kLeftLaneWidth + kLaneGap,
        panel_rect.y + padding,
        panel_rect.width - padding * 2.0f - scrollbar_width - kLeftLaneWidth - kAutomationWidth -
            kLaneGap * 3.0f - right_reserved_width,
        panel_rect.height - padding * 2.0f
    };
}

Rectangle editor_timeline_metrics::scrollbar_track_rect() const {
    return {
        panel_rect.x + padding,
        panel_rect.y + padding,
        scrollbar_width,
        panel_rect.height - padding * 2.0f
    };
}

float editor_timeline_metrics::visible_tick_span() const {
    return content_rect().height * ticks_per_pixel;
}

float editor_timeline_metrics::tick_to_y(int tick) const {
    const Rectangle content = content_rect();
    return content.y + content.height - (static_cast<float>(tick) - bottom_tick) / ticks_per_pixel;
}

int editor_timeline_metrics::y_to_tick(float y) const {
    const Rectangle content = content_rect();
    return static_cast<int>(std::lround(bottom_tick + (content.y + content.height - y) * ticks_per_pixel));
}

float editor_timeline_metrics::lane_width() const {
    const int clamped_key_count = std::max(1, key_count);
    const float content_width = content_rect().width;
    return (content_width - lane_gap * static_cast<float>(clamped_key_count - 1)) / static_cast<float>(clamped_key_count);
}

Rectangle editor_timeline_metrics::lane_rect(int lane) const {
    const Rectangle content = content_rect();
    const float width = lane_width();
    return {
        content.x + lane * (width + lane_gap),
        content.y,
        width,
        content.height
    };
}

editor_timeline_note_geometry editor_timeline_metrics::note_rects(const editor_timeline_note& note) const {
    const int clamped_width = std::max(1, note.lane_width);
    const Rectangle first_lane = lane_rect(note.lane);
    const Rectangle last_lane = lane_rect(std::min(key_count - 1, note.lane + clamped_width - 1));
    const Rectangle lane = {
        std::min(first_lane.x, last_lane.x),
        first_lane.y,
        std::fabs((last_lane.x + last_lane.width) - first_lane.x),
        first_lane.height,
    };
    const float start_y = tick_to_y(note.tick);
    const float single_lane_width = lane_width();
    const float note_body_width = std::max(single_lane_width * 0.2f, lane.width - single_lane_width * 0.08f);
    const float hold_body_width = std::max(single_lane_width * 0.18f, lane.width - single_lane_width * 0.20f);
    const float tap_height = std::clamp(28.0f * g_settings.note_height / std::max(0.001f, ticks_per_pixel),
                                        4.0f,
                                        72.0f);
    const float stay_width = std::max(single_lane_width * 0.54f, note_body_width * 1.04f);
    const float stay_height = std::max(3.0f, tap_height * (0.28f / 0.78f));

    const float head_width = note.type == editor_timeline_note_type::stay ? stay_width : note_body_width;
    const float head_height = note.type == editor_timeline_note_type::stay ? stay_height : tap_height;
    const Rectangle head_rect = {
        lane.x + (lane.width - head_width) * 0.5f,
        start_y - head_height * 0.5f,
        head_width,
        head_height
    };
    const float handle_width = std::clamp(lane.width * 0.16f, 12.0f, 20.0f);
    const float handle_height = head_height + 8.0f;
    Rectangle left_resize_rect = {
        head_rect.x - handle_width * 0.5f,
        start_y - handle_height * 0.5f,
        handle_width,
        handle_height
    };
    Rectangle right_resize_rect = {
        head_rect.x + head_rect.width - handle_width * 0.5f,
        start_y - handle_height * 0.5f,
        handle_width,
        handle_height
    };

    std::optional<Rectangle> body_rect;
    Rectangle tail_rect = {};
    std::optional<Rectangle> start_resize_rect;
    std::optional<Rectangle> end_resize_rect;

    if (timeline_note_has_duration(note.type)) {
        const float end_y = tick_to_y(note.end_tick);
        const float top = std::min(start_y, end_y);
        const float height = std::fabs(end_y - start_y);
        body_rect = Rectangle{
            lane.x + (lane.width - hold_body_width) * 0.5f,
            top,
            hold_body_width,
            std::max(height, 6.0f)
        };
        tail_rect = {
            lane.x + (lane.width - note_body_width) * 0.5f,
            end_y - tap_height * 0.5f,
            note_body_width,
            tap_height
        };
        const float endpoint_handle_height = std::clamp(std::min(tap_height * 0.55f, height * 0.30f),
                                                        8.0f,
                                                        22.0f);
        start_resize_rect = Rectangle{
            lane.x + (lane.width - note_body_width) * 0.5f,
            start_y - endpoint_handle_height * 0.5f,
            note_body_width,
            endpoint_handle_height
        };
        end_resize_rect = Rectangle{
            lane.x + (lane.width - note_body_width) * 0.5f,
            end_y - endpoint_handle_height * 0.5f,
            note_body_width,
            endpoint_handle_height
        };
        left_resize_rect = {
            body_rect->x - handle_width * 0.5f,
            body_rect->y,
            handle_width,
            body_rect->height
        };
        right_resize_rect = {
            body_rect->x + body_rect->width - handle_width * 0.5f,
            body_rect->y,
            handle_width,
            body_rect->height
        };
    }

    return make_editor_timeline_note_geometry(
        note.type,
        head_rect,
        body_rect,
        tail_rect,
        left_resize_rect,
        right_resize_rect,
        start_resize_rect,
        end_resize_rect);
}
