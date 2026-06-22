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
    std::string target;
    std::vector<keyframe> points;
};

struct component {
    std::string id;
    std::string kind = "component";
    std::string type = "background";
    std::string shape;
    std::string text;
    std::string fill = "#101216";
    std::string asset_id;
    std::string target;
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

using source = component;
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

struct layer {
    std::string id;
    std::string name;
    bool visible = true;
    bool locked = false;
    int z = 0;
    double start_ms = 0.0;
    double duration_ms = 0.0;
    std::vector<component> components;
    std::vector<keyframe_track> keyframes;
};

struct mv_composition {
    std::string format = "raythm.mv.composition";
    int format_version = 2;
    std::string composition_id;
    canvas canvas_data;
    double duration_ms = 0.0;
    std::vector<layer> layers;
    std::vector<asset_ref> assets;
};

mv_composition make_default_for_song(const std::string& composition_id, double duration_ms = 0.0);
component make_transform_component(transform value = {});
component make_component(std::string type);
component* find_component(layer& target, const std::string& type);
const component* find_component(const layer& target, const std::string& type);
component* transform_component(layer& target);
const component* transform_component(const layer& target);
component* renderable_component(layer& target);
const component* renderable_component(const layer& target);
std::vector<component*> effect_components(layer& target);
std::vector<const component*> effect_components(const layer& target);
transform transform_from_component(const component& value);
void apply_transform_to_component(component& target, const transform& value);

}  // namespace mv::composition
