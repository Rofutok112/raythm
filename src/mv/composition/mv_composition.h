#pragma once

#include <string>
#include <vector>

namespace mv::composition {

struct canvas {
    int width = 1920;
    int height = 1080;
    std::string background = "#101216";
};

struct asset_ref {
    std::string id;
    std::string type;
    std::string path;
    std::string sha256;
};

struct keyframe {
    double time_ms = 0.0;
    float value = 0.0f;
    std::string easing = "linear";
};

struct keyframe_track {
    std::string id;
    std::string object_id;
    std::string component_id;
    std::string target;
    std::string value_type = "number";
    std::string interpolation = "linear";
    std::vector<keyframe> points;
};

enum class component_category {
    renderer,
    modifier,
    behaviour,
    unknown,
};

struct component {
    std::string id;
    std::string type = "BackgroundRenderer";
    bool enabled = true;
    std::string shape;
    std::string text;
    std::string fill;
    std::string asset_id;
    std::string target;
    std::string script_asset_id;
    std::string script_entry;
    std::string script_source;
    float amount = 0.0f;
    float position_x = 960.0f;
    float position_y = 540.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float rotation_deg = 0.0f;
    float anchor_x = 0.5f;
    float anchor_y = 0.5f;
    float opacity = 1.0f;
};

using renderer = component;
using modifier = component;
using effect = component;

struct transform {
    float position_x = 960.0f;
    float position_y = 540.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float rotation_deg = 0.0f;
    float anchor_x = 0.5f;
    float anchor_y = 0.5f;
    float opacity = 1.0f;
};

struct object {
    std::string id;
    std::string name;
    bool active = true;
    bool visible = true;
    bool locked = false;
    int order = 0;
    double start_ms = 0.0;
    double duration_ms = 0.0;
    transform transform_data;
    std::vector<component> components;

    // Editor-facing cache for now. Serialized compositions store these through
    // mv_composition::animation_tracks so keyframes are a composition-level concept.
    std::vector<keyframe_track> keyframes;
};

using layer = object;

struct mv_composition {
    std::string format = "raythm.mv.composition";
    int format_version = 3;
    std::string composition_id;
    canvas canvas_data;
    double duration_ms = 0.0;
    std::vector<object> objects;
    std::vector<keyframe_track> animation_tracks;
    std::vector<asset_ref> assets;
};

mv_composition make_default_for_song(const std::string& composition_id, double duration_ms = 0.0);
component make_transform_component(transform value = {});
component make_component(std::string type);
component* find_component(object& target, const std::string& type);
const component* find_component(const object& target, const std::string& type);
component* transform_component(object& target);
const component* transform_component(const object& target);
component* renderable_component(object& target);
const component* renderable_component(const object& target);
std::vector<component*> effect_components(object& target);
std::vector<const component*> effect_components(const object& target);
component_category category_for_component_type(const std::string& type);
std::string canonical_component_type(std::string type);
bool is_renderer_component_type(const std::string& type);
bool is_modifier_component_type(const std::string& type);
transform transform_from_component(const component& value);
void apply_transform_to_component(component& target, const transform& value);
component component_from_transform(transform value = {});
void apply_transform_to_object(object& target, const transform& value);

}  // namespace mv::composition
