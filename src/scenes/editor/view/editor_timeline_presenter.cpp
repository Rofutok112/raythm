#include "editor/view/editor_timeline_presenter.h"

#include <algorithm>
#include <cmath>

namespace {
editor_timeline_note make_timeline_note(const note_data& note, size_t source_index) {
    editor_timeline_note_type type = editor_timeline_note_type::tap;
    switch (note.type) {
        case note_type::tap:
            type = editor_timeline_note_type::tap;
            break;
        case note_type::hold:
            type = editor_timeline_note_type::hold;
            break;
        case note_type::release:
            type = editor_timeline_note_type::release;
            break;
        case note_type::stay:
            type = editor_timeline_note_type::stay;
            break;
    }

    return {type, note.tick, note.lane, note.end_tick, note.is_ray, note_lane_width(note), source_index};
}

bool note_intersects_tick_range(const note_data& note, int min_tick, int max_tick) {
    const int start_tick = note.tick;
    const int end_tick = note.type == note_type::hold ? std::max(note.tick, note.end_tick) : note.tick;
    return end_tick >= min_tick && start_tick <= max_tick;
}
}

void editor_timeline_presenter::draw(const editor_timeline_presenter_model& model) {
    const editor_timeline_metrics metrics = editor_timeline_viewport::metrics(model.viewport);
    const float visible_tick_span = editor_timeline_viewport::visible_tick_span(model.viewport);
    const int min_tick = static_cast<int>(std::floor(model.viewport.viewport.bottom_tick - visible_tick_span * 0.1f));
    const int max_tick = static_cast<int>(std::ceil(model.viewport.viewport.bottom_tick + visible_tick_span));

    std::vector<editor_timeline_note> notes;
    std::vector<editor_timeline_note> minimap_notes;
    notes.reserve(std::min<std::size_t>(model.state.data().notes.size(), 4096));
    minimap_notes.reserve(model.state.data().notes.size());
    for (size_t index = 0; index < model.state.data().notes.size(); ++index) {
        const note_data& note = model.state.data().notes[index];
        editor_timeline_note timeline_note = make_timeline_note(note, index);
        minimap_notes.push_back(timeline_note);
        if (note_intersects_tick_range(note, min_tick, max_tick)) {
            notes.push_back(timeline_note);
        }
    }

    std::vector<editor_timeline_scroll_automation_point> scroll_automation;
    scroll_automation.reserve(model.state.data().scroll_automation.size());
    for (const scroll_automation_point& point : model.state.data().scroll_automation) {
        scroll_automation.push_back({point.tick, point.multiplier, point.curve_to_next});
    }

    std::vector<editor_timeline_note> preview_notes;
    preview_notes.reserve(model.preview_notes.size());
    for (size_t index = 0; index < model.preview_notes.size(); ++index) {
        preview_notes.push_back(make_timeline_note(model.preview_notes[index], index));
    }

    editor_timeline_view::draw({
        metrics,
        model.meter_map.visible_grid_lines(min_tick, max_tick),
        std::move(scroll_automation),
        std::move(notes),
        std::move(minimap_notes),
        model.selected_note_indices,
        model.selected_scroll_event_index,
        model.audio_loaded ? std::optional<int>(model.playback_tick) : std::nullopt,
        model.loop_enabled,
        model.loop_start_tick,
        model.loop_end_tick,
        model.waveform_summary,
        &model.state.engine(),
        model.waveform_visible,
        model.waveform_offset_ms,
        std::move(preview_notes),
        model.preview_note_indices,
        model.preview_has_overlap,
        model.selection_rect,
        min_tick,
        max_tick,
        editor_timeline_viewport::snap_interval(model.viewport),
        editor_timeline_viewport::content_height_pixels(model.viewport),
        editor_timeline_viewport::scroll_offset_pixels(model.viewport)
    });
}
