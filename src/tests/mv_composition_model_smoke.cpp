#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "mv/composition/mv_composition_evaluator.h"
#include "mv/composition/mv_composition_serializer.h"

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

mv::composition::component make_effect(std::string id, std::string type, std::string target, float amount) {
    mv::composition::component effect;
    effect.id = std::move(id);
    effect.kind = "component";
    effect.type = std::move(type);
    effect.target = std::move(target);
    effect.amount = amount;
    return effect;
}

void add_transform_and_renderer(mv::composition::layer& layer,
                                const std::string& type,
                                const std::string& fill,
                                float opacity,
                                const std::string& asset_id = {}) {
    mv::composition::transform transform;
    transform.opacity = opacity;
    mv::composition::component renderer = mv::composition::make_component(type);
    renderer.fill = fill;
    renderer.asset_id = asset_id;
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
}

bool has_renderer(const mv::composition::layer& layer,
                  const std::string& type,
                  const std::string& fill = {},
                  const std::string& asset_id = {}) {
    const mv::composition::component* renderer = mv::composition::renderable_component(layer);
    return renderer != nullptr &&
           renderer->type == type &&
           (fill.empty() || renderer->fill == fill) &&
           (asset_id.empty() || renderer->asset_id == asset_id);
}

}  // namespace

int main() {
    bool ok = true;

    mv::composition::mv_composition composition = mv::composition::make_default_for_song("mv-model-smoke", 120000.0);
    expect(composition.format == "raythm.mv.composition", "Expected composition format marker.", ok);
    expect(composition.format_version == 2, "Expected composition formatVersion 2.", ok);
    expect(composition.layers.size() >= 2, "Expected default composition layers.", ok);

    const mv::composition::mv_composition fallback_duration =
        mv::composition::make_default_for_song("mv-fallback-duration-smoke");
    expect(fallback_duration.duration_ms >= 60000.0,
           "Expected default MV composition duration to be long enough for song authoring.",
           ok);
    expect(std::all_of(fallback_duration.layers.begin(), fallback_duration.layers.end(),
                       [](const mv::composition::layer& layer) {
                           return layer.duration_ms >= 60000.0;
                       }),
           "Expected default MV layers to cover the fallback composition duration.",
           ok);

    mv::composition::asset_ref asset;
    asset.id = "asset-jacket";
    asset.type = "image";
    asset.path = "assets/images/jacket.png";
    asset.sha256 = "abc123";
    composition.assets.push_back(asset);

    const std::size_t animated_layer_index = composition.layers.size() - 1;
    const std::string animated_layer_id = composition.layers[animated_layer_index].id;
    composition.layers[animated_layer_index].components.push_back(
        make_effect("fx-pulse", "pulse", "transform.scale", 0.12f));
    mv::composition::layer beat_grid;
    beat_grid.id = "layer-beat-grid";
    beat_grid.name = "Beat Grid";
    beat_grid.z = 5;
    beat_grid.start_ms = 0.0;
    beat_grid.duration_ms = 120000.0;
    add_transform_and_renderer(beat_grid, "beatGrid", "#8b7cf6", 0.8f);
    composition.layers.push_back(beat_grid);
    mv::composition::layer waveform;
    waveform.id = "layer-waveform";
    waveform.name = "Waveform";
    waveform.z = 6;
    waveform.start_ms = 0.0;
    waveform.duration_ms = 120000.0;
    add_transform_and_renderer(waveform, "waveform", "#6ee7b7", 0.85f);
    composition.layers.push_back(waveform);
    mv::composition::layer spectrum;
    spectrum.id = "layer-spectrum";
    spectrum.name = "Spectrum";
    spectrum.z = 7;
    spectrum.start_ms = 0.0;
    spectrum.duration_ms = 120000.0;
    add_transform_and_renderer(spectrum, "spectrum", "#38bdf8", 0.82f);
    composition.layers.push_back(spectrum);
    mv::composition::layer image_layer;
    image_layer.id = "layer-image";
    image_layer.name = "Image";
    image_layer.z = 8;
    image_layer.start_ms = 0.0;
    image_layer.duration_ms = 120000.0;
    add_transform_and_renderer(image_layer, "image", "#ffffff", 0.9f, "asset-jacket");
    composition.layers.push_back(image_layer);
    mv::composition::keyframe_track& position_track =
        mv::composition::ensure_keyframe_track(composition.layers[animated_layer_index], "transform.position.x");
    mv::composition::upsert_keyframe(position_track, {0.0, 100.0f, "linear"});
    mv::composition::upsert_keyframe(position_track, {1000.0, 300.0f, "linear"});

    const std::string serialized = mv::composition::serialize(composition);
    expect(serialized.find("\"components\"") != std::string::npos,
           "Expected serialized objects to contain components.",
           ok);
    expect(serialized.find("\"source\"") == std::string::npos &&
               serialized.find("\"effects\"") == std::string::npos &&
               serialized.find("\"eventTriggers\"") == std::string::npos,
           "Expected serialized objects to avoid legacy source/effects/eventTriggers fields.",
           ok);
    const auto parsed = mv::composition::parse(serialized);
    expect(parsed.success, "Expected serialized composition to parse.", ok);
    if (parsed.success) {
        expect(parsed.composition.composition_id == composition.composition_id,
               "Expected composition id to round-trip.",
               ok);
        expect(parsed.composition.duration_ms == 120000.0,
               "Expected durationMs to round-trip.",
               ok);
        expect(parsed.composition.assets.size() == 1,
               "Expected assets to round-trip.",
               ok);
        const auto animated_it = std::find_if(parsed.composition.layers.begin(), parsed.composition.layers.end(),
                                              [&](const mv::composition::layer& layer) {
                                                  return layer.id == animated_layer_id;
                                              });
        expect(animated_it != parsed.composition.layers.end(),
               "Expected animated layer to round-trip.",
               ok);
        if (animated_it != parsed.composition.layers.end()) {
            expect(!animated_it->keyframes.empty() &&
                       animated_it->keyframes.front().points.size() == 2,
                   "Expected keyframe points to round-trip.",
                   ok);
        }
        expect(std::any_of(parsed.composition.layers.begin(), parsed.composition.layers.end(),
                           [](const mv::composition::layer& layer) {
                               const std::vector<const mv::composition::component*> effects =
                                   mv::composition::effect_components(layer);
                               return std::any_of(effects.begin(), effects.end(),
                                                  [](const mv::composition::component* effect) {
                                                      return effect->id == "fx-pulse" &&
                                                             effect->amount > 0.11f;
                                                  });
                           }),
               "Expected effects to round-trip.",
               ok);
        expect(std::any_of(parsed.composition.layers.begin(), parsed.composition.layers.end(),
                           [](const mv::composition::layer& layer) {
                               return has_renderer(layer, "beatGrid", "#8b7cf6");
                           }),
               "Expected generated beatGrid source to round-trip.",
               ok);
        expect(std::any_of(parsed.composition.layers.begin(), parsed.composition.layers.end(),
                           [](const mv::composition::layer& layer) {
                               return has_renderer(layer, "waveform", "#6ee7b7");
                           }),
               "Expected generated waveform source to round-trip.",
               ok);
        expect(std::any_of(parsed.composition.layers.begin(), parsed.composition.layers.end(),
                           [](const mv::composition::layer& layer) {
                               return has_renderer(layer, "spectrum", "#38bdf8");
                           }),
               "Expected generated spectrum source to round-trip.",
               ok);
        expect(std::any_of(parsed.composition.layers.begin(), parsed.composition.layers.end(),
                           [](const mv::composition::layer& layer) {
                               return has_renderer(layer, "image", {}, "asset-jacket");
                           }),
               "Expected image source asset reference to round-trip.",
               ok);
        const mv::composition::transform evaluated =
            mv::composition::evaluate_transform(animated_it != parsed.composition.layers.end()
                                                    ? *animated_it
                                                    : parsed.composition.layers.back(),
                                                500.0);
        expect(static_cast<int>(evaluated.position_x) == 200,
               "Expected keyframe interpolation to evaluate midpoint.",
               ok);
        mv::composition::layer edited_layer = parsed.composition.layers.back();
        mv::composition::keyframe_track& opacity_track =
            mv::composition::ensure_keyframe_track(edited_layer, "transform.opacity");
        mv::composition::upsert_keyframe(opacity_track, {500.0, 0.4f, "linear"});
        mv::composition::keyframe_track& text_track =
            mv::composition::ensure_keyframe_track(edited_layer, "text.content");
        mv::composition::upsert_keyframe(text_track, {500.0, 1.0f, "linear"});
        expect(mv::composition::count_transform_keyframes_near(edited_layer, 500.0, 24.0) == 1,
               "Expected transform keyframe count near playhead.",
               ok);
        expect(mv::composition::erase_transform_keyframes_near(edited_layer, 500.0, 24.0) == 1,
               "Expected transform keyframe removal near playhead.",
               ok);
        expect(mv::composition::count_transform_keyframes_near(edited_layer, 500.0, 24.0) == 0,
               "Expected transform keyframe removal to clear matching points.",
               ok);
        expect(std::any_of(edited_layer.keyframes.begin(), edited_layer.keyframes.end(),
                           [](const mv::composition::keyframe_track& track) {
                               return track.target == "text.content" && track.points.size() == 1;
                           }),
               "Expected transform keyframe removal to preserve non-transform tracks.",
               ok);
        mv::composition::layer effect_layer = parsed.composition.layers.back();
        effect_layer.start_ms = 0.0;
        effect_layer.duration_ms = 4000.0;
        if (mv::composition::component* transform = mv::composition::transform_component(effect_layer)) {
            transform->opacity = 1.0f;
            transform->scale_x = 1.0f;
            transform->scale_y = 1.0f;
        }
        effect_layer.components.push_back(make_effect("fx-fade", "fade", "transform.opacity", 1000.0f));
        effect_layer.components.push_back(make_effect("fx-pulse", "pulse", "transform.scale", 0.10f));
        const mv::composition::transform faded_start =
            mv::composition::evaluate_transform(effect_layer, 0.0);
        const mv::composition::transform pulsed =
            mv::composition::evaluate_transform(effect_layer, 125.0);
        expect(faded_start.opacity == 0.0f,
               "Expected fade effect to reduce opacity at layer start.",
               ok);
        expect(pulsed.scale_x > 1.09f && pulsed.scale_y > 1.09f,
               "Expected pulse effect to increase scale at pulse peak.",
               ok);
        mv::composition::layer flash_layer = parsed.composition.layers.back();
        flash_layer.start_ms = 0.0;
        flash_layer.duration_ms = 2000.0;
        if (mv::composition::component* transform = mv::composition::transform_component(flash_layer)) {
            transform->opacity = 0.20f;
        }
        flash_layer.components.push_back(make_effect("fx-flash", "flash", "transform.opacity", 0.50f));
        const mv::composition::transform flashed =
            mv::composition::evaluate_transform(flash_layer, 0.0);
        expect(flashed.opacity > 0.55f,
               "Expected flash effect to boost opacity at flash start.",
               ok);
        mv::composition::layer shake_layer = parsed.composition.layers.back();
        shake_layer.start_ms = 0.0;
        shake_layer.duration_ms = 2000.0;
        if (mv::composition::component* transform = mv::composition::transform_component(shake_layer)) {
            transform->position_x = 100.0f;
            transform->position_y = 200.0f;
        }
        shake_layer.components.push_back(make_effect("fx-shake", "shake", "transform.position", 24.0f));
        const mv::composition::transform shaken =
            mv::composition::evaluate_transform(shake_layer, 125.0);
        expect(shaken.position_x != 100.0f || shaken.position_y != 200.0f,
               "Expected shake effect to offset position.",
               ok);
        expect(mv::composition::fingerprint(parsed.composition) == mv::composition::fingerprint(composition),
               "Expected fingerprint to be stable across round-trip.",
               ok);
    }

    const auto unsupported = mv::composition::parse(
        "{\n"
        "  \"format\": \"raythm.mv.composition\",\n"
        "  \"formatVersion\": 99,\n"
        "  \"compositionId\": \"bad\",\n"
        "  \"canvas\": {\"width\": 1920, \"height\": 1080, \"background\": \"#000000\"},\n"
        "  \"durationMs\": 1000,\n"
        "  \"layers\": [],\n"
        "  \"assets\": []\n"
        "}\n");
    expect(!unsupported.success, "Expected unsupported formatVersion to fail.", ok);

    mv::composition::mv_composition absolute_asset = composition;
    absolute_asset.assets.front().path = "C:/leaked/source.png";
    const auto absolute_asset_result = mv::composition::parse(mv::composition::serialize(absolute_asset));
    expect(!absolute_asset_result.success, "Expected absolute asset paths to fail validation.", ok);

    mv::composition::mv_composition missing_asset_id = composition;
    mv::composition::renderable_component(missing_asset_id.layers.back())->asset_id.clear();
    const auto missing_asset_id_result =
        mv::composition::parse(mv::composition::serialize(missing_asset_id));
    expect(!missing_asset_id_result.success, "Expected image layers without assetId to fail validation.", ok);

    mv::composition::mv_composition unknown_asset_id = composition;
    mv::composition::renderable_component(unknown_asset_id.layers.back())->asset_id = "missing-asset";
    const auto unknown_asset_id_result =
        mv::composition::parse(mv::composition::serialize(unknown_asset_id));
    expect(!unknown_asset_id_result.success, "Expected unknown image assetId references to fail validation.", ok);

    mv::composition::mv_composition duplicate_asset_id = composition;
    duplicate_asset_id.assets.push_back(duplicate_asset_id.assets.front());
    duplicate_asset_id.assets.back().path = "assets/images/duplicate.png";
    const auto duplicate_asset_id_result =
        mv::composition::parse(mv::composition::serialize(duplicate_asset_id));
    expect(!duplicate_asset_id_result.success, "Expected duplicate asset ids to fail validation.", ok);

    mv::composition::mv_composition duplicate_layer_id = composition;
    duplicate_layer_id.layers.push_back(duplicate_layer_id.layers.back());
    duplicate_layer_id.layers.back().name = "Duplicate Layer";
    const auto duplicate_layer_id_result =
        mv::composition::parse(mv::composition::serialize(duplicate_layer_id));
    expect(!duplicate_layer_id_result.success, "Expected duplicate layer ids to fail validation.", ok);

    mv::composition::mv_composition duplicate_effect_id = composition;
    duplicate_effect_id.layers[animated_layer_index].components.push_back(
        make_effect("fx-pulse", "flash", "transform.opacity", 0.5f));
    const auto duplicate_effect_id_result =
        mv::composition::parse(mv::composition::serialize(duplicate_effect_id));
    expect(!duplicate_effect_id_result.success, "Expected duplicate layer effect ids to fail validation.", ok);

    mv::composition::mv_composition missing_effect_id = composition;
    mv::composition::effect_components(missing_effect_id.layers[animated_layer_index]).front()->id.clear();
    const auto missing_effect_id_result =
        mv::composition::parse(mv::composition::serialize(missing_effect_id));
    expect(!missing_effect_id_result.success, "Expected effects without ids to fail validation.", ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "mv_composition_model smoke test passed\n";
    return EXIT_SUCCESS;
}
