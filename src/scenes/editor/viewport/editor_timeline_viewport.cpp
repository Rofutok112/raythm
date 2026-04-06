#include "editor/viewport/editor_timeline_viewport.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "editor/view/editor_layout.h"
#include "ui_draw.h"

namespace {
namespace layout = editor::layout;

constexpr float kTimelinePadding = 18.0f;
constexpr float kLaneGap = 6.0f;
constexpr float kScrollbarWidth = 10.0f;
constexpr float kScrollbarGap = 10.0f;
constexpr float kMinTicksPerPixel = 0.9f;
constexpr float kMaxTicksPerPixel = 28.0f;
constexpr float kScrollLerpSpeed = 12.0f;
constexpr float kScrollWheelViewportRatio = 0.55f;
constexpr float kNoteHeadHeight = 14.0f;
constexpr float kTimelineLeadInTicks = 960.0f;
constexpr float kPlaybackFollowViewportRatio = 0.35f;
constexpr std::array<int, 11> kSnapDivisions = {1, 2, 3, 4, 8, 12, 16, 24, 32, 64, 128};
constexpr std::array<const char*, 11> kSnapLabels = {"1/1", "1/2", "1/3", "1/4", "1/8", "1/12", "1/16", "1/12", "1/32", "1/64", "1/128"};
}

editor_timeline_metrics editor_timeline_viewport::metrics(const editor_timeline_viewport_model& model) {
    return {
        layout::kTimelineRect,
        kTimelinePadding,
        kScrollbarGap,
        kScrollbarWidth,
        kLaneGap,
        kNoteHeadHeight,
        model.viewport.bottom_tick,
        model.viewport.ticks_per_pixel,
        model.state->data().meta.key_count
    };
}

float editor_timeline_viewport::visible_tick_span(const editor_timeline_viewport_model& model) {
    return metrics(model).visible_tick_span();
} 

float editor_timeline_viewport::content_tick_span(const editor_timeline_viewport_model& model) {
    int max_tick = model.state->data().meta.resolution * 8;
    for (const note_data& note : model.state->data().notes) {
        max_tick = std::max(max_tick, note.type == note_type::hold ? note.end_tick : note.tick);
    }
    for (const timing_event& event : model.state->data().timing_events) {
        max_tick = std::max(max_tick, event.tick);
    }
    max_tick = std::max(max_tick, model.audio_length_tick);

    return std::max(visible_tick_span(model), static_cast<float>(max_tick) + model.state->data().meta.resolution * 4.0f);
}

float editor_timeline_viewport::content_height_pixels(const editor_timeline_viewport_model& model) {
    return (content_tick_span(model) - min_bottom_tick()) / model.viewport.ticks_per_pixel;
}

float editor_timeline_viewport::scroll_offset_pixels(const editor_timeline_viewport_model& model) {
    return (max_bottom_tick(model) - model.viewport.bottom_tick) / model.viewport.ticks_per_pixel;
}

float editor_timeline_viewport::min_bottom_tick() {
    return -kTimelineLeadInTicks;
}

float editor_timeline_viewport::max_bottom_tick(const editor_timeline_viewport_model& model) {
    return std::max(min_bottom_tick(), content_tick_span(model) - visible_tick_span(model));
}

int editor_timeline_viewport::snap_division(const editor_timeline_viewport_state& viewport) {
    return kSnapDivisions[std::clamp(viewport.snap_index, 0, static_cast<int>(kSnapDivisions.size()) - 1)];
}

int editor_timeline_viewport::snap_interval(const editor_timeline_viewport_model& model) {
    return std::max(1, model.state->data().meta.resolution * 4 / snap_division(model.viewport));
}

int editor_timeline_viewport::snap_tick(const editor_timeline_viewport_model& model, int raw_tick) {
    return std::max(0, model.state->snap_tick(std::max(0, raw_tick), snap_division(model.viewport)));
}

int editor_timeline_viewport::default_timing_event_tick(const editor_timeline_viewport_model& model,
                                                        std::optional<size_t> selected_timing_event_index) {
    if (selected_timing_event_index.has_value() &&
        *selected_timing_event_index < model.state->data().timing_events.size()) {
        return snap_tick(model, model.state->data().timing_events[*selected_timing_event_index].tick + snap_interval(model));
    }
    return std::max(snap_interval(model),
                    snap_tick(model, static_cast<int>(model.viewport.bottom_tick + visible_tick_span(model) * 0.5f)));
}

editor_timeline_viewport_state editor_timeline_viewport::apply_scroll_and_zoom(const editor_timeline_viewport_model& model,
                                                                               const editor_timeline_viewport_scroll_input& input) {
    editor_timeline_viewport_state next = model.viewport;
    const editor_timeline_metrics viewport_metrics = metrics(model);
    const Rectangle content = viewport_metrics.content_rect();
    const Rectangle track = viewport_metrics.scrollbar_track_rect();
    const ui::scrollbar_interaction scrollbar = ui::update_vertical_scrollbar(
        track, content_height_pixels(model), scroll_offset_pixels(model),
        next.scrollbar_dragging, next.scrollbar_drag_offset, 40.0f);
    next.bottom_tick_target = max_bottom_tick(model) - scrollbar.scroll_offset * next.ticks_per_pixel;
    if (scrollbar.changed || scrollbar.dragging) {
        next.bottom_tick = next.bottom_tick_target;
    }

    if (input.wheel != 0.0f && CheckCollisionPointRec(input.mouse, content) && input.ctrl_down) {
        const int anchor_tick = viewport_metrics.y_to_tick(input.mouse.y);
        const float zoom_scale = input.wheel > 0.0f ? 0.85f : 1.15f;
        next.ticks_per_pixel = std::clamp(next.ticks_per_pixel * zoom_scale, kMinTicksPerPixel, kMaxTicksPerPixel);
        next.bottom_tick_target = static_cast<float>(anchor_tick) -
                                  (content.y + content.height - input.mouse.y) * next.ticks_per_pixel;
        const editor_timeline_viewport_model updated_model = {model.state, model.audio_length_tick, next};
        next.bottom_tick_target = std::clamp(next.bottom_tick_target, min_bottom_tick(), max_bottom_tick(updated_model));
    } else if (!input.audio_playing && input.wheel != 0.0f && CheckCollisionPointRec(input.mouse, content)) {
        next.bottom_tick_target = std::clamp(next.bottom_tick_target + input.wheel * visible_tick_span(model) * kScrollWheelViewportRatio,
                                             min_bottom_tick(), max_bottom_tick(model));
    } else if (input.audio_playing) {
        next.bottom_tick_target = std::clamp(static_cast<float>(input.playback_tick) - visible_tick_span(model) * kPlaybackFollowViewportRatio,
                                             min_bottom_tick(), max_bottom_tick(model));
    }

    const editor_timeline_viewport_model updated_model = {model.state, model.audio_length_tick, next};
    next.bottom_tick_target = std::clamp(next.bottom_tick_target, min_bottom_tick(), max_bottom_tick(updated_model));
    if (next.bottom_tick_target <= min_bottom_tick() || next.bottom_tick_target >= max_bottom_tick(updated_model)) {
        next.bottom_tick = next.bottom_tick_target;
        return next;
    }
    next.bottom_tick += (next.bottom_tick_target - next.bottom_tick) * std::min(1.0f, kScrollLerpSpeed * input.dt);
    if (std::fabs(next.bottom_tick - next.bottom_tick_target) < 0.5f) {
        next.bottom_tick = next.bottom_tick_target;
    }
    return next;
}

editor_timeline_viewport_state editor_timeline_viewport::scroll_to_tick(const editor_timeline_viewport_model& model, int tick) {
    editor_timeline_viewport_state next = model.viewport;
    const float target = std::clamp(static_cast<float>(tick) - visible_tick_span(model) * 0.5f,
                                    min_bottom_tick(), max_bottom_tick(model));
    next.bottom_tick_target = target;
    next.bottom_tick = target;
    return next;
}

std::span<const char* const> editor_timeline_viewport::snap_labels() {
    return std::span<const char* const>(kSnapLabels.data(), kSnapLabels.size());
}
