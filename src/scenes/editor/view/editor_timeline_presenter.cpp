#include "editor/view/editor_timeline_presenter.h"

#include <cmath>

namespace {
editor_timeline_note make_timeline_note(const note_data& note) {
    return {
        note.type == note_type::hold ? editor_timeline_note_type::hold : editor_timeline_note_type::tap,
        note.tick,
        note.lane,
        note.end_tick
    };
}
}

void editor_timeline_presenter::draw(const editor_timeline_presenter_model& model) {
    const editor_timeline_metrics metrics = editor_timeline_viewport::metrics(model.viewport);
    const float visible_tick_span = editor_timeline_viewport::visible_tick_span(model.viewport);
    const int min_tick = static_cast<int>(std::floor(model.viewport.viewport.bottom_tick - visible_tick_span * 0.1f));
    const int max_tick = static_cast<int>(std::ceil(model.viewport.viewport.bottom_tick + visible_tick_span));

    std::vector<editor_timeline_note> notes;
    notes.reserve(model.state.data().notes.size());
    for (const note_data& note : model.state.data().notes) {
        notes.push_back(make_timeline_note(note));
    }

    std::optional<editor_timeline_note> preview_note;
    if (model.preview_note.has_value()) {
        preview_note = make_timeline_note(*model.preview_note);
    }

    editor_timeline_view::draw({
        metrics,
        model.meter_map.visible_grid_lines(min_tick, max_tick),
        std::move(notes),
        model.selected_note_index,
        model.audio_loaded ? std::optional<int>(model.playback_tick) : std::nullopt,
        model.waveform_summary,
        &model.state.engine(),
        model.waveform_visible,
        model.waveform_offset_ms,
        preview_note,
        model.preview_has_overlap,
        min_tick,
        max_tick,
        editor_timeline_viewport::snap_interval(model.viewport),
        editor_timeline_viewport::content_height_pixels(model.viewport),
        editor_timeline_viewport::scroll_offset_pixels(model.viewport)
    });
}
