#include "mv/composition/mv_composition.h"

namespace mv::composition {
namespace {

constexpr double kFallbackCompositionDurationMs = 120000.0;

}  // namespace

mv_composition make_default_for_song(const std::string& composition_id, double duration_ms) {
    const double effective_duration_ms = duration_ms > 0.0 ? duration_ms : kFallbackCompositionDurationMs;
    mv_composition result;
    result.composition_id = composition_id;
    result.duration_ms = effective_duration_ms;

    layer background;
    background.id = "layer-background";
    background.name = "Background";
    background.z = 0;
    background.start_ms = 0.0;
    background.duration_ms = effective_duration_ms;
    background.source_data.type = "background";
    background.source_data.fill = result.canvas_data.background;
    background.transform_data.position_x = 960.0f;
    background.transform_data.position_y = 540.0f;
    result.layers.push_back(std::move(background));

    layer title;
    title.id = "layer-title";
    title.name = "Title";
    title.z = 10;
    title.start_ms = 0.0;
    title.duration_ms = effective_duration_ms;
    title.source_data.type = "text";
    title.source_data.text = "New MV";
    title.source_data.fill = "#d8d4ff";
    title.transform_data.position_x = 960.0f;
    title.transform_data.position_y = 540.0f;
    title.transform_data.opacity = 0.9f;
    result.layers.push_back(std::move(title));

    return result;
}

}  // namespace mv::composition
