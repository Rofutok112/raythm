#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "mv/composition/mv_composition_evaluator.h"
#include "mv/composition/mv_component_registry.h"
#include "mv/composition/mv_composition_serializer.h"
#include "mv/composition/mv_lua_runtime.h"

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
    expect(composition.format_version == 3, "Expected composition formatVersion 3.", ok);
    expect(composition.objects.size() >= 2, "Expected default composition layers.", ok);
    expect(mv::composition::category_for_component_type("SpectrumRenderer") ==
               mv::composition::component_category::renderer,
           "Expected SpectrumRenderer to be a Renderer component.",
           ok);
    expect(mv::composition::canonical_component_type("spectrum") == "SpectrumRenderer",
           "Expected legacy spectrum component aliases to canonicalize.",
           ok);
    const mv::composition::component_definition* lua_definition =
        mv::composition::find_component_definition("LuaBehaviour");
    expect(lua_definition != nullptr &&
               lua_definition->category == mv::composition::component_category::behaviour,
           "Expected LuaBehaviour to be registered as a Behaviour component.",
           ok);

    const mv::composition::mv_composition fallback_duration =
        mv::composition::make_default_for_song("mv-fallback-duration-smoke");
    expect(fallback_duration.duration_ms >= 60000.0,
           "Expected default MV composition duration to be long enough for song authoring.",
           ok);
    expect(std::all_of(fallback_duration.objects.begin(), fallback_duration.objects.end(),
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
    mv::composition::asset_ref script_asset;
    script_asset.id = "script-bounce";
    script_asset.type = "script";
    script_asset.path = "assets/scripts/script-bounce.lua";
    script_asset.sha256 = "def456";
    composition.assets.push_back(script_asset);

    const std::size_t animated_layer_index = composition.objects.size() - 1;
    const std::string animated_layer_id = composition.objects[animated_layer_index].id;
    composition.objects[animated_layer_index].components.push_back(
        make_effect("fx-pulse", "Pulse", "transform.scale", 0.12f));
    mv::composition::component lua_behaviour = mv::composition::make_component("LuaBehaviour");
    lua_behaviour.id = "lua-bounce";
    lua_behaviour.script_asset_id = "script-bounce";
    lua_behaviour.script_entry = "update";
    composition.objects[animated_layer_index].components.push_back(lua_behaviour);
    mv::composition::layer beat_grid;
    beat_grid.id = "layer-beat-grid";
    beat_grid.name = "Beat Grid";
    beat_grid.order = 5;
    beat_grid.start_ms = 0.0;
    beat_grid.duration_ms = 120000.0;
    add_transform_and_renderer(beat_grid, "BeatGridRenderer", "#8b7cf6", 0.8f);
    composition.objects.push_back(beat_grid);
    mv::composition::layer waveform;
    waveform.id = "layer-waveform";
    waveform.name = "Waveform";
    waveform.order = 6;
    waveform.start_ms = 0.0;
    waveform.duration_ms = 120000.0;
    add_transform_and_renderer(waveform, "WaveformRenderer", "#6ee7b7", 0.85f);
    composition.objects.push_back(waveform);
    mv::composition::layer spectrum;
    spectrum.id = "layer-spectrum";
    spectrum.name = "Spectrum";
    spectrum.order = 7;
    spectrum.start_ms = 0.0;
    spectrum.duration_ms = 120000.0;
    add_transform_and_renderer(spectrum, "SpectrumRenderer", "#38bdf8", 0.82f);
    composition.objects.push_back(spectrum);
    mv::composition::layer image_layer;
    image_layer.id = "layer-image";
    image_layer.name = "Image";
    image_layer.order = 8;
    image_layer.start_ms = 0.0;
    image_layer.duration_ms = 120000.0;
    add_transform_and_renderer(image_layer, "ImageRenderer", "#ffffff", 0.9f, "asset-jacket");
    composition.objects.push_back(image_layer);
    mv::composition::keyframe_track& position_track =
        mv::composition::ensure_keyframe_track(composition.objects[animated_layer_index], "transform.position.x");
    mv::composition::upsert_keyframe(position_track, {0.0, 100.0f, "linear"});
    mv::composition::upsert_keyframe(position_track, {1000.0, 300.0f, "linear"});

    const std::string serialized = mv::composition::serialize(composition);
    expect(serialized.find("\"objects\"") != std::string::npos && serialized.find("\"components\"") != std::string::npos && serialized.find("\"animationTracks\"") != std::string::npos,
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
        expect(parsed.composition.assets.size() == 2,
               "Expected assets to round-trip.",
               ok);
        const auto animated_it = std::find_if(parsed.composition.objects.begin(), parsed.composition.objects.end(),
                                              [&](const mv::composition::layer& layer) {
                                                  return layer.id == animated_layer_id;
                                              });
        expect(animated_it != parsed.composition.objects.end(),
               "Expected animated layer to round-trip.",
               ok);
        if (animated_it != parsed.composition.objects.end()) {
            expect(!animated_it->keyframes.empty() &&
                       animated_it->keyframes.front().points.size() == 2,
                   "Expected keyframe points to round-trip.",
                   ok);
        }
        expect(std::any_of(parsed.composition.objects.begin(), parsed.composition.objects.end(),
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
        expect(std::any_of(parsed.composition.objects.begin(), parsed.composition.objects.end(),
                           [](const mv::composition::layer& layer) {
                               return std::any_of(layer.components.begin(), layer.components.end(),
                                                  [](const mv::composition::component& component) {
                                                      return component.type == "LuaBehaviour" &&
                                                             component.script_asset_id == "script-bounce" &&
                                                             component.script_entry == "update";
                                                  });
                           }),
               "Expected LuaBehaviour props to round-trip.",
               ok);
        mv::composition::layer lua_layer = parsed.composition.objects[animated_layer_index];
        mv::composition::component* lua_component =
            mv::composition::find_component(lua_layer, "LuaBehaviour");
        expect(lua_component != nullptr, "Expected LuaBehaviour component to be findable.", ok);
        if (lua_component != nullptr) {
            lua_component->script_source =
                "function update(self, ctx)\n"
                "  self.transform.position.x = self.transform.position.x + ctx.songTimeMs / 100\n"
                "  self.transform.scale.y = 2.5\n"
                "end\n";
            mv::composition::apply_transform_to_object(lua_layer, mv::composition::evaluate_transform(lua_layer, 500.0));
            const mv::composition::lua_update_result lua_result =
                mv::composition::apply_lua_behaviours(lua_layer, {.song_time_ms = 500.0, .delta_ms = 16.0});
            const mv::composition::transform lua_transform =
                mv::composition::transform_from_component(*mv::composition::transform_component(lua_layer));
            expect(lua_result.success, "Expected LuaBehaviour update to succeed.", ok);
            expect(static_cast<int>(lua_transform.position_x) == 205,
                   "Expected LuaBehaviour to mutate transform position.",
                   ok);
            expect(lua_transform.scale_y == 2.5f,
                   "Expected LuaBehaviour to mutate transform scale.",
                   ok);

            lua_component->script_source =
                "count = 0\n"
                "function start(self, ctx)\n"
                "  self.transform.position.x = self.transform.position.x + 3\n"
                "end\n"
                "function update(self, ctx)\n"
                "  count = count + 1\n"
                "  self.transform.position.x = self.transform.position.x + count\n"
                "end\n";
            mv::composition::lua_behaviour_runtime runtime;
            mv::composition::layer first_lua_frame = lua_layer;
            mv::composition::apply_transform_to_object(
                first_lua_frame,
                mv::composition::evaluate_transform(first_lua_frame, 500.0));
            const mv::composition::lua_update_result first_persistent_result =
                runtime.apply_lua_behaviours(first_lua_frame, {.song_time_ms = 500.0, .delta_ms = 16.0});
            const mv::composition::transform first_persistent_transform =
                mv::composition::transform_from_component(*mv::composition::transform_component(first_lua_frame));
            expect(first_persistent_result.success,
                   "Expected persistent LuaBehaviour first update to succeed.",
                   ok);
            expect(static_cast<int>(first_persistent_transform.position_x) == 204,
                   "Expected LuaBehaviour start and first update to mutate transform.",
                   ok);

            mv::composition::layer second_lua_frame = lua_layer;
            mv::composition::apply_transform_to_object(
                second_lua_frame,
                mv::composition::evaluate_transform(second_lua_frame, 500.0));
            const mv::composition::lua_update_result second_persistent_result =
                runtime.apply_lua_behaviours(second_lua_frame, {.song_time_ms = 516.0, .delta_ms = 16.0});
            const mv::composition::transform second_persistent_transform =
                mv::composition::transform_from_component(*mv::composition::transform_component(second_lua_frame));
            expect(second_persistent_result.success,
                   "Expected persistent LuaBehaviour second update to succeed.",
                   ok);
            expect(static_cast<int>(second_persistent_transform.position_x) == 202,
                   "Expected LuaBehaviour state to persist while start runs once.",
                   ok);
        }
        expect(std::any_of(parsed.composition.objects.begin(), parsed.composition.objects.end(),
                           [](const mv::composition::layer& layer) {
                               return has_renderer(layer, "BeatGridRenderer", "#8b7cf6");
                           }),
               "Expected generated beatGrid source to round-trip.",
               ok);
        expect(std::any_of(parsed.composition.objects.begin(), parsed.composition.objects.end(),
                           [](const mv::composition::layer& layer) {
                               return has_renderer(layer, "WaveformRenderer", "#6ee7b7");
                           }),
               "Expected generated waveform source to round-trip.",
               ok);
        expect(std::any_of(parsed.composition.objects.begin(), parsed.composition.objects.end(),
                           [](const mv::composition::layer& layer) {
                               return has_renderer(layer, "SpectrumRenderer", "#38bdf8");
                           }),
               "Expected generated spectrum source to round-trip.",
               ok);
        expect(std::any_of(parsed.composition.objects.begin(), parsed.composition.objects.end(),
                           [](const mv::composition::layer& layer) {
                               return has_renderer(layer, "ImageRenderer", {}, "asset-jacket");
                           }),
               "Expected image source asset reference to round-trip.",
               ok);
        const mv::composition::transform evaluated =
            mv::composition::evaluate_transform(animated_it != parsed.composition.objects.end()
                                                    ? *animated_it
                                                    : parsed.composition.objects.back(),
                                                500.0);
        expect(static_cast<int>(evaluated.position_x) == 200,
               "Expected keyframe interpolation to evaluate midpoint.",
               ok);
        mv::composition::layer edited_layer = parsed.composition.objects.back();
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
        mv::composition::layer effect_layer = parsed.composition.objects.back();
        effect_layer.start_ms = 0.0;
        effect_layer.duration_ms = 4000.0;
        if (mv::composition::component* transform = mv::composition::transform_component(effect_layer)) {
            transform->opacity = 1.0f;
            transform->scale_x = 1.0f;
            transform->scale_y = 1.0f;
        }
        effect_layer.components.push_back(make_effect("fx-fade", "Fade", "transform.opacity", 1000.0f));
        effect_layer.components.push_back(make_effect("fx-pulse", "Pulse", "transform.scale", 0.10f));
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
        mv::composition::layer flash_layer = parsed.composition.objects.back();
        flash_layer.start_ms = 0.0;
        flash_layer.duration_ms = 2000.0;
        if (mv::composition::component* transform = mv::composition::transform_component(flash_layer)) {
            transform->opacity = 0.20f;
        }
        flash_layer.components.push_back(make_effect("fx-flash", "Flash", "transform.opacity", 0.50f));
        const mv::composition::transform flashed =
            mv::composition::evaluate_transform(flash_layer, 0.0);
        expect(flashed.opacity > 0.55f,
               "Expected flash effect to boost opacity at flash start.",
               ok);
        mv::composition::layer shake_layer = parsed.composition.objects.back();
        shake_layer.start_ms = 0.0;
        shake_layer.duration_ms = 2000.0;
        if (mv::composition::component* transform = mv::composition::transform_component(shake_layer)) {
            transform->position_x = 100.0f;
            transform->position_y = 200.0f;
        }
        shake_layer.components.push_back(make_effect("fx-shake", "Shake", "transform.position", 24.0f));
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
    mv::composition::renderable_component(missing_asset_id.objects.back())->asset_id.clear();
    const auto missing_asset_id_result =
        mv::composition::parse(mv::composition::serialize(missing_asset_id));
    expect(!missing_asset_id_result.success, "Expected image layers without assetId to fail validation.", ok);

    mv::composition::mv_composition unknown_asset_id = composition;
    mv::composition::renderable_component(unknown_asset_id.objects.back())->asset_id = "missing-asset";
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
    duplicate_layer_id.objects.push_back(duplicate_layer_id.objects.back());
    duplicate_layer_id.objects.back().name = "Duplicate Layer";
    const auto duplicate_layer_id_result =
        mv::composition::parse(mv::composition::serialize(duplicate_layer_id));
    expect(!duplicate_layer_id_result.success, "Expected duplicate layer ids to fail validation.", ok);

    mv::composition::mv_composition duplicate_effect_id = composition;
    duplicate_effect_id.objects[animated_layer_index].components.push_back(
        make_effect("fx-pulse", "Flash", "transform.opacity", 0.5f));
    const auto duplicate_effect_id_result =
        mv::composition::parse(mv::composition::serialize(duplicate_effect_id));
    expect(!duplicate_effect_id_result.success, "Expected duplicate layer effect ids to fail validation.", ok);

    mv::composition::mv_composition missing_effect_id = composition;
    mv::composition::effect_components(missing_effect_id.objects[animated_layer_index]).front()->id.clear();
    const auto missing_effect_id_result =
        mv::composition::parse(mv::composition::serialize(missing_effect_id));
    expect(!missing_effect_id_result.success, "Expected effects without ids to fail validation.", ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "mv_composition_model smoke test passed\n";
    return EXIT_SUCCESS;
}
