#include "mv_renderer.h"

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

#include "ui/ui_font.h"
#include "scenes/scene_common.h"

namespace mv {

namespace {

Color to_raylib(const color& c, float opacity = 1.0f) {
    uint8_t a = static_cast<uint8_t>(std::clamp(c.a * opacity, 0.0f, 255.0f));
    return {c.r, c.g, c.b, a};
}

void draw_node(const rect_node& n) {
    Color col = to_raylib(n.fill, n.opacity);
    if (n.rotation != 0.0f) {
        Rectangle rec = {n.x + n.w * 0.5f, n.y + n.h * 0.5f, n.w, n.h};
        Vector2 origin = {n.w * 0.5f, n.h * 0.5f};
        DrawRectanglePro(rec, origin, n.rotation, col);
    } else {
        DrawRectangle(static_cast<int>(n.x), static_cast<int>(n.y),
                      static_cast<int>(n.w), static_cast<int>(n.h), col);
    }
}

void draw_node(const line_node& n) {
    Color col = to_raylib(n.stroke, n.opacity);
    DrawLineEx({n.x1, n.y1}, {n.x2, n.y2}, n.thickness, col);
}

void draw_node(const text_node& n) {
    Color col = to_raylib(n.fill, n.opacity);
    ui::draw_text_auto(n.text.c_str(), {n.x, n.y}, static_cast<float>(n.font_size), 0.0f, col);
}

void draw_node(const circle_node& n) {
    Color col = to_raylib(n.fill, n.opacity);
    DrawCircle(static_cast<int>(n.cx), static_cast<int>(n.cy),
               n.radius, col);
}

void draw_node(const polyline_node& n) {
    if (n.points.size() < 2) return;
    Color col = to_raylib(n.stroke, n.opacity);
    for (size_t i = 0; i + 1 < n.points.size(); i++) {
        DrawLineEx({n.points[i].x, n.points[i].y},
                   {n.points[i + 1].x, n.points[i + 1].y},
                   n.thickness, col);
    }
}

void draw_node(const spectrum_bar_node& n) {
    if (n.bar_count <= 0) return;
    Color col = to_raylib(n.fill, n.opacity);
    float bar_w = n.w / static_cast<float>(n.bar_count);
    float gap = bar_w * 0.1f;
    for (int i = 0; i < n.bar_count; i++) {
        float amp = 0.0f;
        if (i < static_cast<int>(n.spectrum.size())) {
            amp = std::clamp(n.spectrum[i], 0.0f, 1.0f);
        }
        float bar_h = n.h * amp;
        float bx = n.x + i * bar_w + gap * 0.5f;
        float by = n.y + n.h - bar_h;
        DrawRectangle(static_cast<int>(bx), static_cast<int>(by),
                      static_cast<int>(bar_w - gap), static_cast<int>(bar_h), col);
    }
}

void draw_node(const beat_grid_node& n) {
    Color col = to_raylib(n.stroke, n.opacity);
    // Horizontal lines that pulse with beat
    float phase_offset = n.beat_phase * 40.0f; // scroll speed
    float spacing = 40.0f;
    for (float y = n.y - spacing + std::fmod(phase_offset, spacing);
         y < n.y + n.h; y += spacing) {
        if (y >= n.y) {
            DrawLineEx({n.x, y}, {n.x + n.w, y}, n.thickness, col);
        }
    }
    // Vertical lines (static)
    for (float x = n.x; x <= n.x + n.w; x += spacing) {
        DrawLineEx({x, n.y}, {x, n.y + n.h}, n.thickness, col);
    }
}

void draw_node(const pulse_ring_node& n) {
    Color col = to_raylib(n.stroke, n.opacity);
    // Ring expands and fades with beat_phase
    float scale = 1.0f + n.beat_phase * 0.5f;
    float alpha_mul = 1.0f - n.beat_phase;
    col.a = static_cast<uint8_t>(std::clamp(col.a * alpha_mul, 0.0f, 255.0f));
    DrawCircleLines(static_cast<int>(n.cx), static_cast<int>(n.cy),
                    n.radius * scale, col);
}

void draw_node(const background_node& n) {
    Color col = to_raylib(n.fill, n.opacity);
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, col);
}

} // anonymous namespace

void render_scene(const scene& sc) {
    // Clear with scene clear color if alpha > 0
    if (sc.clear_color.a > 0) {
        DrawRectangle(0, 0, kScreenWidth, kScreenHeight,
                      to_raylib(sc.clear_color));
    }

    for (const auto& node : sc.nodes) {
        std::visit([](const auto& n) { draw_node(n); }, node);
    }
}

} // namespace mv
