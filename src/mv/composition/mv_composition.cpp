#include "mv/composition/mv_composition.h"

#include <algorithm>
#include <utility>

#include "mv/composition/mv_component_registry.h"

namespace mv::composition {
namespace {

constexpr double kFallbackCompositionDurationMs = 120000.0;

}  // namespace

component make_transform_component(transform value) {
    component result;
    result.id = "transform";
    result.type = "transform";
    apply_transform_to_component(result, value);
    return result;
}

component make_component(std::string type) {
    component result = make_default_component(std::move(type));
    result.id = "component-" + result.type;
    return result;
}

std::string canonical_component_type(std::string type) {
    if (type == "background") {
        return "BackgroundRenderer";
    }
    if (type == "text") {
        return "TextRenderer";
    }
    if (type == "shape") {
        return "ShapeRenderer";
    }
    if (type == "image") {
        return "ImageRenderer";
    }
    if (type == "beatGrid") {
        return "BeatGridRenderer";
    }
    if (type == "waveform") {
        return "WaveformRenderer";
    }
    if (type == "spectrum") {
        return "SpectrumRenderer";
    }
    if (type == "fade") {
        return "Fade";
    }
    if (type == "pulse") {
        return "Pulse";
    }
    if (type == "beatPulse") {
        return "BeatReactive";
    }
    if (type == "flash") {
        return "Flash";
    }
    if (type == "shake") {
        return "Shake";
    }
    return type;
}

bool is_renderer_component_type(const std::string& type) {
    return category_for_component_type(type) == component_category::renderer;
}

bool is_modifier_component_type(const std::string& type) {
    return category_for_component_type(type) == component_category::modifier;
}

component_category category_for_component_type(const std::string& type) {
    if (const component_definition* definition = find_component_definition(type); definition != nullptr) {
        return definition->category;
    }
    return component_category::unknown;
}

component* find_component(object& target, const std::string& type) {
    const std::string canonical = canonical_component_type(type);
    const auto it = std::find_if(target.components.begin(), target.components.end(), [&](const component& current) {
        return canonical_component_type(current.type) == canonical;
    });
    return it == target.components.end() ? nullptr : &*it;
}

const component* find_component(const object& target, const std::string& type) {
    const std::string canonical = canonical_component_type(type);
    const auto it = std::find_if(target.components.begin(), target.components.end(), [&](const component& current) {
        return canonical_component_type(current.type) == canonical;
    });
    return it == target.components.end() ? nullptr : &*it;
}

component* transform_component(object& target) {
    return find_component(target, "transform");
}

const component* transform_component(const object& target) {
    return find_component(target, "transform");
}

component* renderable_component(object& target) {
    const auto it = std::find_if(target.components.begin(), target.components.end(), [](const component& current) {
        return current.enabled && is_renderer_component_type(current.type);
    });
    return it == target.components.end() ? nullptr : &*it;
}

const component* renderable_component(const object& target) {
    const auto it = std::find_if(target.components.begin(), target.components.end(), [](const component& current) {
        return current.enabled && is_renderer_component_type(current.type);
    });
    return it == target.components.end() ? nullptr : &*it;
}

std::vector<component*> effect_components(object& target) {
    std::vector<component*> result;
    for (component& current : target.components) {
        if (current.enabled && is_modifier_component_type(current.type)) {
            result.push_back(&current);
        }
    }
    return result;
}

std::vector<const component*> effect_components(const object& target) {
    std::vector<const component*> result;
    for (const component& current : target.components) {
        if (current.enabled && is_modifier_component_type(current.type)) {
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

component component_from_transform(transform value) {
    return make_transform_component(value);
}

void apply_transform_to_object(object& target, const transform& value) {
    target.transform_data = value;
    if (component* legacy_transform = transform_component(target)) {
        apply_transform_to_component(*legacy_transform, value);
    }
}

mv_composition make_default_for_song(const std::string& composition_id, double duration_ms) {
    const double effective_duration_ms = duration_ms > 0.0 ? duration_ms : kFallbackCompositionDurationMs;
    mv_composition result;
    result.composition_id = composition_id;
    result.duration_ms = effective_duration_ms;

    object background;
    background.id = "object-background";
    background.name = "Background";
    background.order = 0;
    background.start_ms = 0.0;
    background.duration_ms = effective_duration_ms;
    transform background_transform;
    background_transform.position_x = 960.0f;
    background_transform.position_y = 540.0f;
    background.transform_data = background_transform;
    background.components.push_back(make_transform_component(background_transform));
    component background_renderer = make_component("BackgroundRenderer");
    background_renderer.fill = result.canvas_data.background;
    background.components.push_back(std::move(background_renderer));
    result.objects.push_back(std::move(background));

    object title;
    title.id = "object-title";
    title.name = "Title";
    title.order = 10;
    title.start_ms = 0.0;
    title.duration_ms = effective_duration_ms;
    transform title_transform;
    title_transform.position_x = 960.0f;
    title_transform.position_y = 540.0f;
    title_transform.opacity = 0.9f;
    title.transform_data = title_transform;
    title.components.push_back(make_transform_component(title_transform));
    component title_renderer = make_component("TextRenderer");
    title_renderer.text = "New MV";
    title_renderer.fill = "#d8d4ff";
    title.components.push_back(std::move(title_renderer));
    result.objects.push_back(std::move(title));

    return result;
}

}  // namespace mv::composition
