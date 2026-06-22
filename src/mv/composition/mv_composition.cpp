#include "mv/composition/mv_composition.h"

#include <algorithm>
#include <utility>

namespace mv::composition {
namespace {

constexpr double kFallbackCompositionDurationMs = 120000.0;

bool is_renderable_component_type(const std::string& type) {
    return type == "background" ||
           type == "text" ||
           type == "shape" ||
           type == "image" ||
           type == "beatGrid" ||
           type == "waveform" ||
           type == "spectrum";
}

bool is_effect_component_type(const std::string& type) {
    return type == "fade" ||
           type == "pulse" ||
           type == "beatPulse" ||
           type == "flash" ||
           type == "shake";
}

}  // namespace

component make_transform_component(transform value) {
    component result;
    result.id = "transform";
    result.kind = "transform";
    result.type = "transform";
    apply_transform_to_component(result, value);
    return result;
}

component make_component(std::string type) {
    component result;
    result.id = "component-" + type;
    result.kind = "component";
    result.type = std::move(type);
    return result;
}

component* find_component(layer& target, const std::string& type) {
    const auto it = std::find_if(target.components.begin(), target.components.end(), [&](const component& current) {
        return current.type == type;
    });
    return it == target.components.end() ? nullptr : &*it;
}

const component* find_component(const layer& target, const std::string& type) {
    const auto it = std::find_if(target.components.begin(), target.components.end(), [&](const component& current) {
        return current.type == type;
    });
    return it == target.components.end() ? nullptr : &*it;
}

component* transform_component(layer& target) {
    return find_component(target, "transform");
}

const component* transform_component(const layer& target) {
    return find_component(target, "transform");
}

component* renderable_component(layer& target) {
    const auto it = std::find_if(target.components.begin(), target.components.end(), [](const component& current) {
        return is_renderable_component_type(current.type);
    });
    return it == target.components.end() ? nullptr : &*it;
}

const component* renderable_component(const layer& target) {
    const auto it = std::find_if(target.components.begin(), target.components.end(), [](const component& current) {
        return is_renderable_component_type(current.type);
    });
    return it == target.components.end() ? nullptr : &*it;
}

std::vector<component*> effect_components(layer& target) {
    std::vector<component*> result;
    for (component& current : target.components) {
        if (is_effect_component_type(current.type)) {
            result.push_back(&current);
        }
    }
    return result;
}

std::vector<const component*> effect_components(const layer& target) {
    std::vector<const component*> result;
    for (const component& current : target.components) {
        if (is_effect_component_type(current.type)) {
            result.push_back(&current);
        }
    }
    return result;
}

transform transform_from_component(const component& value) {
    return {
        value.position_x,
        value.position_y,
        value.scale_x,
        value.scale_y,
        value.rotation_deg,
        value.anchor_x,
        value.anchor_y,
        value.opacity,
    };
}

void apply_transform_to_component(component& target, const transform& value) {
    target.position_x = value.position_x;
    target.position_y = value.position_y;
    target.scale_x = value.scale_x;
    target.scale_y = value.scale_y;
    target.rotation_deg = value.rotation_deg;
    target.anchor_x = value.anchor_x;
    target.anchor_y = value.anchor_y;
    target.opacity = value.opacity;
}

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
    transform background_transform;
    background_transform.position_x = 960.0f;
    background_transform.position_y = 540.0f;
    background.components.push_back(make_transform_component(background_transform));
    component background_renderer = make_component("background");
    background_renderer.fill = result.canvas_data.background;
    background.components.push_back(std::move(background_renderer));
    result.layers.push_back(std::move(background));

    layer title;
    title.id = "layer-title";
    title.name = "Title";
    title.z = 10;
    title.start_ms = 0.0;
    title.duration_ms = effective_duration_ms;
    transform title_transform;
    title_transform.position_x = 960.0f;
    title_transform.position_y = 540.0f;
    title_transform.opacity = 0.9f;
    title.components.push_back(make_transform_component(title_transform));
    component title_renderer = make_component("text");
    title_renderer.text = "New MV";
    title_renderer.fill = "#d8d4ff";
    title.components.push_back(std::move(title_renderer));
    result.layers.push_back(std::move(title));

    return result;
}

}  // namespace mv::composition
