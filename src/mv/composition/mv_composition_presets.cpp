#include "mv/composition/mv_composition_presets.h"

#include <algorithm>
#include <cmath>

#include "mv/composition/mv_composition_evaluator.h"

namespace mv::composition {

namespace {

std::string next_layer_id(const mv_composition& composition, const std::string& prefix) {
    for (int index = static_cast<int>(composition.layers.size()) + 1; index < 10000; ++index) {
        const std::string id = prefix + "-" + std::to_string(index);
        const auto it = std::find_if(composition.layers.begin(), composition.layers.end(), [&](const auto& layer) {
            return layer.id == id;
        });
        if (it == composition.layers.end()) {
            return id;
        }
    }
    return prefix + "-fallback";
}

int next_z(const mv_composition& composition) {
    int z = 0;
    for (const layer& current : composition.layers) {
        z = std::max(z, current.z);
    }
    return z + 10;
}

double effective_duration(double requested_duration_ms, double fallback_ms) {
    return std::max(250.0, requested_duration_ms > 0.0 ? requested_duration_ms : fallback_ms);
}

void add_key(layer& target, const std::string& property, double time_ms, float value) {
    keyframe_track& track = ensure_keyframe_track(target, property);
    upsert_keyframe(track, {.time_ms = std::max(0.0, time_ms), .value = value, .easing = "linear"});
}

void append_result(preset_apply_result& result, const layer& added) {
    result.added_layer_ids.push_back(added.id);
    result.selected_layer_id = added.id;
}

preset_apply_result apply_chorus_flash(mv_composition& composition, double start_ms, double duration_ms) {
    preset_apply_result result;
    layer flash;
    flash.id = next_layer_id(composition, "preset-chorus-flash");
    flash.name = "Chorus Flash";
    flash.z = next_z(composition);
    flash.start_ms = std::max(0.0, start_ms);
    flash.duration_ms = effective_duration(duration_ms, 900.0);
    flash.source_data.type = "shape";
    flash.source_data.shape = "rect";
    flash.source_data.fill = "#ffffff";
    flash.transform_data.position_x = static_cast<float>(composition.canvas_data.width) * 0.5f;
    flash.transform_data.position_y = static_cast<float>(composition.canvas_data.height) * 0.5f;
    flash.transform_data.scale_x = 4.2f;
    flash.transform_data.scale_y = 4.2f;
    flash.transform_data.opacity = 0.0f;
    add_key(flash, "transform.opacity", flash.start_ms, 0.0f);
    add_key(flash, "transform.opacity", flash.start_ms + 90.0, 0.75f);
    add_key(flash, "transform.opacity", flash.start_ms + flash.duration_ms, 0.0f);
    composition.layers.push_back(flash);
    append_result(result, flash);
    result.success = true;
    result.message = "Added chorus flash preset.";
    return result;
}

preset_apply_result apply_lyric_pop(mv_composition& composition, double start_ms, double duration_ms) {
    preset_apply_result result;
    layer text;
    text.id = next_layer_id(composition, "preset-lyric-pop");
    text.name = "Lyric Pop";
    text.z = next_z(composition);
    text.start_ms = std::max(0.0, start_ms);
    text.duration_ms = effective_duration(duration_ms, 2400.0);
    text.source_data.type = "text";
    text.source_data.text = "LYRIC";
    text.source_data.fill = "#d8d4ff";
    text.transform_data.position_x = static_cast<float>(composition.canvas_data.width) * 0.5f;
    text.transform_data.position_y = static_cast<float>(composition.canvas_data.height) * 0.36f;
    text.transform_data.scale_x = 0.9f;
    text.transform_data.scale_y = 0.9f;
    text.transform_data.opacity = 0.0f;
    add_key(text, "transform.opacity", text.start_ms, 0.0f);
    add_key(text, "transform.opacity", text.start_ms + 180.0, 1.0f);
    add_key(text, "transform.opacity", text.start_ms + text.duration_ms - 220.0, 1.0f);
    add_key(text, "transform.opacity", text.start_ms + text.duration_ms, 0.0f);
    add_key(text, "transform.scale.x", text.start_ms, 0.78f);
    add_key(text, "transform.scale.x", text.start_ms + 180.0, 1.15f);
    add_key(text, "transform.scale.x", text.start_ms + 360.0, 1.0f);
    add_key(text, "transform.scale.y", text.start_ms, 0.78f);
    add_key(text, "transform.scale.y", text.start_ms + 180.0, 1.15f);
    add_key(text, "transform.scale.y", text.start_ms + 360.0, 1.0f);
    composition.layers.push_back(text);
    append_result(result, text);
    result.success = true;
    result.message = "Added lyric pop preset.";
    return result;
}

preset_apply_result apply_bass_pulse(mv_composition& composition, double start_ms, double duration_ms) {
    preset_apply_result result;
    const double layer_duration = effective_duration(duration_ms, std::max(8000.0, composition.duration_ms - start_ms));
    layer pulse_bg;
    pulse_bg.id = next_layer_id(composition, "preset-bass-pulse");
    pulse_bg.name = "Bass Pulse BG";
    pulse_bg.z = next_z(composition);
    pulse_bg.start_ms = std::max(0.0, start_ms);
    pulse_bg.duration_ms = layer_duration;
    pulse_bg.source_data.type = "shape";
    pulse_bg.source_data.shape = "rect";
    pulse_bg.source_data.fill = "#171a21";
    pulse_bg.transform_data.position_x = static_cast<float>(composition.canvas_data.width) * 0.5f;
    pulse_bg.transform_data.position_y = static_cast<float>(composition.canvas_data.height) * 0.5f;
    pulse_bg.transform_data.scale_x = 4.4f;
    pulse_bg.transform_data.scale_y = 4.4f;
    pulse_bg.transform_data.opacity = 0.55f;
    pulse_bg.effects.push_back({
        .id = "fx-bass-pulse",
        .type = "pulse",
        .target = "transform.opacity",
        .amount = 0.18f,
    });
    composition.layers.push_back(pulse_bg);
    append_result(result, pulse_bg);

    layer grid;
    grid.id = next_layer_id(composition, "preset-bass-grid");
    grid.name = "Bass Grid";
    grid.z = next_z(composition);
    grid.start_ms = std::max(0.0, start_ms);
    grid.duration_ms = layer_duration;
    grid.source_data.type = "beatGrid";
    grid.source_data.fill = "#8b7cf6";
    grid.transform_data.position_x = static_cast<float>(composition.canvas_data.width) * 0.5f;
    grid.transform_data.position_y = static_cast<float>(composition.canvas_data.height) * 0.5f;
    grid.transform_data.opacity = 0.35f;
    composition.layers.push_back(grid);
    append_result(result, grid);

    layer spectrum;
    spectrum.id = next_layer_id(composition, "preset-bass-spectrum");
    spectrum.name = "Spectrum Floor";
    spectrum.z = next_z(composition);
    spectrum.start_ms = std::max(0.0, start_ms);
    spectrum.duration_ms = layer_duration;
    spectrum.source_data.type = "spectrum";
    spectrum.source_data.fill = "#38bdf8";
    spectrum.transform_data.position_x = static_cast<float>(composition.canvas_data.width) * 0.5f;
    spectrum.transform_data.position_y = static_cast<float>(composition.canvas_data.height) * 0.78f;
    spectrum.transform_data.opacity = 0.58f;
    composition.layers.push_back(spectrum);
    append_result(result, spectrum);

    result.success = true;
    result.message = "Added bass pulse preset.";
    return result;
}

}  // namespace

const std::vector<preset_definition>& available_presets() {
    static const std::vector<preset_definition> presets = {
        {"chorusFlash", "Flash", "White flash with an opacity pop."},
        {"lyricPop", "Lyric", "Editable text pop with scale and opacity keyframes."},
        {"bassPulse", "Bass", "Pulsing background plus beat grid."},
    };
    return presets;
}

preset_apply_result apply_preset(mv_composition& composition,
                                 const std::string& preset_id,
                                 double start_ms,
                                 double duration_ms) {
    if (preset_id == "chorusFlash") {
        return apply_chorus_flash(composition, start_ms, duration_ms);
    }
    if (preset_id == "lyricPop") {
        return apply_lyric_pop(composition, start_ms, duration_ms);
    }
    if (preset_id == "bassPulse") {
        return apply_bass_pulse(composition, start_ms, duration_ms);
    }
    return {
        .success = false,
        .message = "Unknown MV composition preset: " + preset_id,
    };
}

}  // namespace mv::composition
