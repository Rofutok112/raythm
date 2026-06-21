#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

#include "mv/composition/mv_composition_evaluator.h"
#include "mv/composition/mv_composition_presets.h"
#include "mv/composition/mv_composition_serializer.h"

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

const mv::composition::layer* find_layer(const mv::composition::mv_composition& composition,
                                         const std::string& id) {
    const auto it = std::find_if(composition.layers.begin(), composition.layers.end(), [&](const auto& layer) {
        return layer.id == id;
    });
    return it == composition.layers.end() ? nullptr : &*it;
}

}  // namespace

int main() {
    bool ok = true;
    mv::composition::mv_composition composition =
        mv::composition::make_default_for_song("preset-smoke", 90000.0);
    const std::size_t initial_layer_count = composition.layers.size();

    expect(mv::composition::available_presets().size() >= 3,
           "Expected built-in MV presets.",
           ok);

    const auto lyric = mv::composition::apply_preset(composition, "lyricPop", 1000.0, 2400.0);
    expect(lyric.success, "Expected lyricPop preset to apply.", ok);
    expect(composition.layers.size() == initial_layer_count + 1,
           "Expected lyricPop to add one layer.",
           ok);
    const mv::composition::layer* lyric_layer = find_layer(composition, lyric.selected_layer_id);
    expect(lyric_layer != nullptr && lyric_layer->source_data.type == "text" &&
               lyric_layer->source_data.text == "LYRIC",
           "Expected lyricPop to add editable text layer.",
           ok);
    if (lyric_layer != nullptr) {
        expect(lyric_layer->keyframes.size() >= 3,
               "Expected lyricPop to create transform keyframes.",
               ok);
        const mv::composition::transform evaluated =
            mv::composition::evaluate_transform(*lyric_layer, 1180.0);
        expect(evaluated.opacity > 0.9f && evaluated.scale_x > 1.0f,
               "Expected lyricPop to evaluate visible pop motion.",
               ok);
    }

    const auto flash = mv::composition::apply_preset(composition, "chorusFlash", 2000.0, 900.0);
    expect(flash.success, "Expected chorusFlash preset to apply.", ok);
    const mv::composition::layer* flash_layer = find_layer(composition, flash.selected_layer_id);
    expect(flash_layer != nullptr && flash_layer->source_data.type == "shape" &&
               flash_layer->source_data.fill == "#ffffff",
           "Expected chorusFlash to add white shape layer.",
           ok);
    if (flash_layer != nullptr) {
        const mv::composition::transform peak =
            mv::composition::evaluate_transform(*flash_layer, 2090.0);
        expect(peak.opacity > 0.7f, "Expected chorusFlash peak opacity.", ok);
    }

    const auto bass = mv::composition::apply_preset(composition, "bassPulse", 0.0, 8000.0);
    expect(bass.success && bass.added_layer_ids.size() == 3,
           "Expected bassPulse to add background, grid, and spectrum layers.",
           ok);
    expect(std::any_of(composition.layers.begin(), composition.layers.end(), [](const auto& layer) {
               return layer.source_data.type == "beatGrid" && layer.name == "Bass Grid";
           }),
           "Expected bassPulse to add beat grid source.",
           ok);
    expect(std::any_of(composition.layers.begin(), composition.layers.end(), [](const auto& layer) {
               return layer.source_data.type == "spectrum" && layer.name == "Spectrum Floor";
           }),
           "Expected bassPulse to add spectrum source.",
           ok);

    const std::string serialized = mv::composition::serialize(composition);
    const auto parsed = mv::composition::parse(serialized);
    expect(parsed.success, "Expected composition with presets to serialize and parse.", ok);
    expect(parsed.success && mv::composition::fingerprint(parsed.composition) ==
                                 mv::composition::fingerprint(composition),
           "Expected preset composition fingerprint to round-trip.",
           ok);

    const auto unknown = mv::composition::apply_preset(composition, "missingPreset", 0.0, 0.0);
    expect(!unknown.success, "Expected unknown preset to fail.", ok);

    if (!ok) {
        return EXIT_FAILURE;
    }
    std::cout << "mv_composition_presets smoke test passed\n";
    return EXIT_SUCCESS;
}
