#include "editor_scene_sync.h"

#include <algorithm>

namespace editor_scene_sync {

void sync_metadata_inputs(editor_scene_sync_context context) {
    context.metadata_panel.difficulty_input.value = context.state.data().meta.difficulty;
    context.metadata_panel.chart_author_input.value = context.state.data().meta.chart_author;
    context.metadata_panel.key_count = context.state.data().meta.key_count;
    context.metadata_panel.error.clear();
}

void sync_timing_event_selection(editor_scene_sync_context context) {
    if (context.timing_panel.selected_event_index.has_value() &&
        *context.timing_panel.selected_event_index >= context.state.data().timing_events.size()) {
        context.timing_panel.selected_event_index = context.state.data().timing_events.empty()
            ? std::nullopt
            : std::optional<size_t>(context.state.data().timing_events.size() - 1);
    }
}

void clear_timing_event_inputs(editor_scene_sync_context context) {
    context.timing_panel.inputs.bpm_bar.value.clear();
    context.timing_panel.inputs.bpm_value.value.clear();
    context.timing_panel.inputs.meter_bar.value.clear();
    context.timing_panel.inputs.meter_numerator.value.clear();
    context.timing_panel.inputs.meter_denominator.value.clear();
    context.timing_panel.inputs.bpm_bar.active = false;
    context.timing_panel.inputs.bpm_value.active = false;
    context.timing_panel.inputs.meter_bar.active = false;
    context.timing_panel.inputs.meter_numerator.active = false;
    context.timing_panel.inputs.meter_denominator.active = false;
}

void load_timing_event_inputs(editor_scene_sync_context context) {
    sync_timing_event_selection(context);
    if (!context.timing_panel.selected_event_index.has_value() ||
        *context.timing_panel.selected_event_index >= context.state.data().timing_events.size()) {
        clear_timing_event_inputs(context);
        return;
    }

    context.timing_panel.inputs.bpm_bar.active = false;
    context.timing_panel.inputs.bpm_value.active = false;
    context.timing_panel.inputs.meter_bar.active = false;
    context.timing_panel.inputs.meter_numerator.active = false;
    context.timing_panel.inputs.meter_denominator.active = false;

    const timing_event& event = context.state.data().timing_events[*context.timing_panel.selected_event_index];
    context.timing_panel.inputs.bpm_bar.value = context.meter_map.bar_beat_label(event.tick);
    context.timing_panel.inputs.bpm_value.value = TextFormat("%.1f", event.bpm);
    context.timing_panel.inputs.meter_bar.value = context.meter_map.bar_beat_label(event.tick);
    context.timing_panel.inputs.meter_numerator.value = std::to_string(event.numerator);
    context.timing_panel.inputs.meter_denominator.value = std::to_string(event.denominator);
}

void rebuild_waveform_samples(editor_scene_sync_context context) {
    context.waveform_samples.clear();
    context.waveform_samples.reserve(context.waveform_summary.peaks.size());
    for (const audio_waveform_peak& peak : context.waveform_summary.peaks) {
        const double shifted_ms = peak.seconds * 1000.0 + static_cast<double>(context.waveform_offset_ms);
        context.waveform_samples.push_back({
            context.state.engine().ms_to_tick(shifted_ms),
            peak.amplitude
        });
    }
}

void sync_after_history_change(editor_scene_sync_context context) {
    context.selected_note_index.reset();
    sync_timing_event_selection(context);
    sync_metadata_inputs(context);
    context.meter_map.rebuild(context.state.data());
    rebuild_waveform_samples(context);
    load_timing_event_inputs(context);
    context.timing_panel.input_error.clear();
}

void sync_after_timing_change(editor_scene_sync_context context) {
    context.meter_map.rebuild(context.state.data());
    load_timing_event_inputs(context);
    context.timing_panel.input_error.clear();
}

void sync_after_metadata_change(editor_scene_sync_context context, bool key_count_changed) {
    if (key_count_changed) {
        context.selected_note_index.reset();
    }

    context.metadata_panel.error.clear();
    context.metadata_panel.key_count_confirm_open = false;
    context.metadata_panel.pending_key_count = context.state.data().meta.key_count;
    sync_metadata_inputs(context);
    rebuild_waveform_samples(context);
}

void sync_after_offset_change(editor_scene_sync_context context) {
    context.metadata_panel.error.clear();
    sync_metadata_inputs(context);
    rebuild_waveform_samples(context);
}

}  // namespace editor_scene_sync
