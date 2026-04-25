#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace mv {

// Color stored as RGBA (0-255 each)
struct color {
    uint8_t r = 255, g = 255, b = 255, a = 255;
};

// Parse "#rrggbb" or "#rrggbbaa"; returns white on failure
color parse_color(const std::string& hex);

// ---- Node types ----

struct vec2 { float x = 0, y = 0; };

struct rect_node {
    float x = 0, y = 0, w = 100, h = 100;
    color fill{255, 255, 255, 255};
    float rotation = 0; // degrees
    float opacity = 1.0f;
};

struct line_node {
    float x1 = 0, y1 = 0, x2 = 100, y2 = 100;
    color stroke{255, 255, 255, 255};
    float thickness = 2.0f;
    float opacity = 1.0f;
};

struct text_node {
    std::string text;
    float x = 0, y = 0;
    int font_size = 20;
    color fill{255, 255, 255, 255};
    float opacity = 1.0f;
};

struct circle_node {
    float cx = 0, cy = 0, radius = 50;
    color fill{255, 255, 255, 255};
    float opacity = 1.0f;
};

struct polyline_node {
    std::vector<vec2> points;
    color stroke{255, 255, 255, 255};
    float thickness = 2.0f;
    float opacity = 1.0f;
};

struct spectrum_bar_node {
    float x = 0, y = 0, w = 400, h = 200;
    int bar_count = 32;
    color fill{100, 200, 255, 255};
    float opacity = 1.0f;
    std::vector<float> spectrum; // amplitude values 0..1, filled by ctx
};

struct beat_grid_node {
    float x = 0, y = 0, w = 1920, h = 1080;
    color stroke{255, 255, 255, 60};
    float thickness = 1.0f;
    float beat_phase = 0; // 0..1, fraction within current beat
    float opacity = 1.0f;
};

struct pulse_ring_node {
    float cx = 960, cy = 540, radius = 100;
    color stroke{255, 255, 255, 255};
    float thickness = 3.0f;
    float beat_phase = 0; // drives expansion/fade
    float opacity = 1.0f;
};

struct background_node {
    color fill{0, 0, 0, 255};
    float opacity = 1.0f;
};

// Variant of all node types
using scene_node = std::variant<
    rect_node,
    line_node,
    text_node,
    circle_node,
    polyline_node,
    spectrum_bar_node,
    beat_grid_node,
    pulse_ring_node,
    background_node
>;

// A complete scene returned by draw(ctx)
struct scene {
    color clear_color{0, 0, 0, 0}; // transparent by default (no clear)
    std::vector<scene_node> nodes;  // rendered front-to-back (later = on top)
};

} // namespace mv
