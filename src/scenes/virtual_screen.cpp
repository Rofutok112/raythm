#include "virtual_screen.h"

namespace {
RenderTexture2D render_target_{};
RenderTexture2D ui_render_target_{};
bool initialized_ = false;

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
}  // namespace

namespace virtual_screen {

void init() {
    render_target_ = LoadRenderTexture(kDesignWidth, kDesignHeight);
    ui_render_target_ = LoadRenderTexture(kDesignWidth * kUiRenderScale, kDesignHeight * kUiRenderScale);
    SetTextureFilter(render_target_.texture, TEXTURE_FILTER_BILINEAR);
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
    const float screen_w = static_cast<float>(GetScreenWidth());
    const float screen_h = static_cast<float>(GetScreenHeight());
    const RenderTexture2D& target = current_target(present_mode_);
    const float source_w = static_cast<float>(target.texture.width);
    const float source_h = static_cast<float>(target.texture.height);

    // 16:9 を前提としてウィンドウ全体にフィットさせる
    const float scale_x = screen_w / source_w;
    const float scale_y = screen_h / source_h;
    const float scale = (scale_x < scale_y) ? scale_x : scale_y;

    const float dest_w = source_w * scale;
    const float dest_h = source_h * scale;
    const float offset_x = (screen_w - dest_w) * 0.5f;
    const float offset_y = (screen_h - dest_h) * 0.5f;

    // RenderTexture は OpenGL 座標系で上下反転しているため source.height を負にする
    const Rectangle source = {0.0f, 0.0f, source_w, -source_h};
    const Rectangle dest = {offset_x, offset_y, dest_w, dest_h};

    const Color tint = use_alpha ? WHITE : WHITE;
    DrawTexturePro(target.texture, source, dest, {0.0f, 0.0f}, 0.0f, tint);
}

Vector2 get_virtual_mouse() {
    const Vector2 physical = GetMousePosition();
    const float screen_w = static_cast<float>(GetScreenWidth());
    const float screen_h = static_cast<float>(GetScreenHeight());

    const float source_w = static_cast<float>(current_target(present_mode_).texture.width);
    const float source_h = static_cast<float>(current_target(present_mode_).texture.height);
    const float scale_x = screen_w / source_w;
    const float scale_y = screen_h / source_h;
    const float scale = (scale_x < scale_y) ? scale_x : scale_y;

    const float dest_w = source_w * scale;
    const float dest_h = source_h * scale;
    const float offset_x = (screen_w - dest_w) * 0.5f;
    const float offset_y = (screen_h - dest_h) * 0.5f;

    const float normalized_x = (physical.x - offset_x) / dest_w;
    const float normalized_y = (physical.y - offset_y) / dest_h;
    return {
        normalized_x * static_cast<float>(kDesignWidth),
        normalized_y * static_cast<float>(kDesignHeight),
    };
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
