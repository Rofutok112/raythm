#include "virtual_screen.h"

#include <algorithm>
#include <cmath>

namespace {
RenderTexture2D render_target_{};
RenderTexture2D ui_render_target_{};
bool initialized_ = false;
int top_reserved_pixels_ = 0;

enum class render_mode {
    none,
    standard,
    ui_highres,
};

render_mode active_mode_ = render_mode::none;
render_mode present_mode_ = render_mode::standard;

Camera2D make_ui_camera() {
    Camera2D camera = {};
    camera.offset = {0.0f, 0.0f};
    camera.target = {0.0f, 0.0f};
    camera.rotation = 0.0f;
    camera.zoom = static_cast<float>(virtual_screen::kUiRenderScale);
    return camera;
}

const RenderTexture2D& current_target(render_mode mode) {
    return mode == render_mode::ui_highres ? ui_render_target_ : render_target_;
}

struct presentation_layout {
    float screen_w = 1.0f;
    float reserved_top = 0.0f;
    float screen_h = 1.0f;
    float source_w = 1.0f;
    float source_h = 1.0f;
    float scale = 1.0f;
    float dest_w = 1.0f;
    float dest_h = 1.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

presentation_layout make_presentation_layout() {
    presentation_layout layout;
    layout.screen_w = static_cast<float>(GetScreenWidth());
    layout.reserved_top = static_cast<float>(std::clamp(top_reserved_pixels_, 0, std::max(0, GetScreenHeight() - 1)));
    layout.screen_h = std::max(1.0f, static_cast<float>(GetScreenHeight()) - layout.reserved_top);
    const RenderTexture2D& target = current_target(present_mode_);
    layout.source_w = static_cast<float>(target.texture.width);
    layout.source_h = static_cast<float>(target.texture.height);

    const float scale_x = layout.screen_w / layout.source_w;
    const float scale_y = layout.screen_h / layout.source_h;
    layout.scale = (scale_x < scale_y) ? scale_x : scale_y;
    layout.dest_w = std::round(layout.source_w * layout.scale);
    layout.dest_h = std::round(layout.source_h * layout.scale);
    layout.offset_x = std::round((layout.screen_w - layout.dest_w) * 0.5f);
    layout.offset_y = layout.reserved_top + std::round((layout.screen_h - layout.dest_h) * 0.5f);
    return layout;
}
}  // namespace

namespace virtual_screen {

void init() {
    render_target_ = LoadRenderTexture(kDesignWidth, kDesignHeight);
    ui_render_target_ = LoadRenderTexture(kDesignWidth * kUiRenderScale, kDesignHeight * kUiRenderScale);
    SetTextureFilter(render_target_.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(ui_render_target_.texture, TEXTURE_FILTER_BILINEAR);
    active_mode_ = render_mode::none;
    present_mode_ = render_mode::standard;
    initialized_ = true;
}

void cleanup() {
    if (initialized_) {
        UnloadRenderTexture(render_target_);
        UnloadRenderTexture(ui_render_target_);
        initialized_ = false;
        active_mode_ = render_mode::none;
        present_mode_ = render_mode::standard;
    }
}

void set_top_reserved_pixels(int pixels) {
    top_reserved_pixels_ = std::max(0, pixels);
}

int top_reserved_pixels() {
    return top_reserved_pixels_;
}

void begin() {
    BeginTextureMode(render_target_);
    active_mode_ = render_mode::standard;
    present_mode_ = render_mode::standard;
}

void begin_ui() {
    BeginTextureMode(ui_render_target_);
    BeginMode2D(make_ui_camera());
    active_mode_ = render_mode::ui_highres;
    present_mode_ = render_mode::ui_highres;
}

void end() {
    if (active_mode_ == render_mode::ui_highres) {
        EndMode2D();
    }
    EndTextureMode();
    active_mode_ = render_mode::none;
}

void draw_to_screen(bool use_alpha) {
    const presentation_layout layout = make_presentation_layout();
    const RenderTexture2D& target = current_target(present_mode_);

    // RenderTexture は OpenGL 座標系で上下反転しているため source.height を負にする
    const Rectangle source = {0.0f, 0.0f, layout.source_w, -layout.source_h};
    const Rectangle dest = {layout.offset_x, layout.offset_y, layout.dest_w, layout.dest_h};

    if (!use_alpha) {
        if (layout.offset_y > 0.0f) {
            DrawTexturePro(target.texture,
                           {0.0f, layout.source_h - 1.0f, layout.source_w, -1.0f},
                           {0.0f, layout.reserved_top, layout.screen_w, layout.offset_y - layout.reserved_top},
                           {0.0f, 0.0f}, 0.0f, WHITE);
            DrawTexturePro(target.texture,
                           {0.0f, 0.0f, layout.source_w, -1.0f},
                           {0.0f, layout.offset_y + layout.dest_h,
                            layout.screen_w, layout.screen_h - layout.offset_y - layout.dest_h},
                           {0.0f, 0.0f}, 0.0f, WHITE);
        }
        if (layout.offset_x > 0.0f) {
            DrawTexturePro(target.texture,
                           {0.0f, 0.0f, 1.0f, -layout.source_h},
                           {0.0f, layout.offset_y, layout.offset_x, layout.dest_h},
                           {0.0f, 0.0f}, 0.0f, WHITE);
            DrawTexturePro(target.texture,
                           {layout.source_w - 1.0f, 0.0f, 1.0f, -layout.source_h},
                           {layout.offset_x + layout.dest_w, layout.offset_y,
                            layout.screen_w - layout.offset_x - layout.dest_w, layout.dest_h},
                           {0.0f, 0.0f}, 0.0f, WHITE);
        }
    }

    const Color tint = use_alpha ? WHITE : WHITE;
    DrawTexturePro(target.texture, source, dest, {0.0f, 0.0f}, 0.0f, tint);
}

Vector2 get_virtual_mouse() {
    const Vector2 physical = GetMousePosition();
    const presentation_layout layout = make_presentation_layout();

    const float normalized_x = (physical.x - layout.offset_x) / layout.dest_w;
    const float normalized_y = (physical.y - layout.offset_y) / layout.dest_h;
    return {
        normalized_x * static_cast<float>(kDesignWidth),
        normalized_y * static_cast<float>(kDesignHeight),
    };
}

Rectangle visible_rect() {
    const presentation_layout layout = make_presentation_layout();
    const float scale_x = static_cast<float>(kDesignWidth) / std::max(1.0f, layout.dest_w);
    const float scale_y = static_cast<float>(kDesignHeight) / std::max(1.0f, layout.dest_h);
    return {
        -layout.offset_x * scale_x,
        (layout.reserved_top - layout.offset_y) * scale_y,
        layout.screen_w * scale_x,
        layout.screen_h * scale_y,
    };
}

float design_to_screen_scale() {
    const presentation_layout layout = make_presentation_layout();
    return layout.dest_w / static_cast<float>(kDesignWidth);
}

float current_render_scale() {
    return active_mode_ == render_mode::ui_highres ? static_cast<float>(kUiRenderScale) : 1.0f;
}

int current_render_width() {
    const RenderTexture2D& target = current_target(active_mode_ == render_mode::none ? present_mode_ : active_mode_);
    return target.texture.width;
}

int current_render_height() {
    const RenderTexture2D& target = current_target(active_mode_ == render_mode::none ? present_mode_ : active_mode_);
    return target.texture.height;
}

}  // namespace virtual_screen
