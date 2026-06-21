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

struct source {
    std::string type = "background";
    std::string shape;
    std::string text;
    std::string fill = "#101216";
    std::string asset_id;
};

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

struct effect {
    std::string id;
    std::string type;
    std::string target;
    float amount = 0.0f;
};

struct event_action {
    std::string type;
    std::string effect_id;
    std::string property;
    std::string value;
};

struct event_trigger {
    std::string event;
    double time_ms = -1.0;
    std::vector<event_action> actions;
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

struct layer {
    std::string id;
    std::string name;
    bool visible = true;
    bool locked = false;
    int z = 0;
    double start_ms = 0.0;
    double duration_ms = 0.0;
    source source_data;
    transform transform_data;
    std::vector<effect> effects;
    std::vector<keyframe_track> keyframes;
    std::vector<event_trigger> event_triggers;
};

struct mv_composition {
    std::string format = "raythm.mv.composition";
    int format_version = 1;
    std::string composition_id;
    canvas canvas_data;
    double duration_ms = 0.0;
    std::vector<layer> layers;
    std::vector<asset_ref> assets;
};

mv_composition make_default_for_song(const std::string& composition_id, double duration_ms = 0.0);

}  // namespace mv::composition
