#include "virtual_screen.h"

namespace {
RenderTexture2D render_target_{};
bool initialized_ = false;
}  // namespace

namespace virtual_screen {

void init() {
    render_target_ = LoadRenderTexture(kDesignWidth, kDesignHeight);
    initialized_ = true;
}

void cleanup() {
    if (initialized_) {
        UnloadRenderTexture(render_target_);
        initialized_ = false;
    }
}

void begin() {
    BeginTextureMode(render_target_);
}

void end() {
    EndTextureMode();
}

void draw_to_screen(bool use_alpha) {
    const float screen_w = static_cast<float>(GetScreenWidth());
    const float screen_h = static_cast<float>(GetScreenHeight());

    // 16:9 を前提としてウィンドウ全体にフィットさせる
    const float scale_x = screen_w / static_cast<float>(kDesignWidth);
    const float scale_y = screen_h / static_cast<float>(kDesignHeight);
    const float scale = (scale_x < scale_y) ? scale_x : scale_y;

    const float dest_w = static_cast<float>(kDesignWidth) * scale;
    const float dest_h = static_cast<float>(kDesignHeight) * scale;
    const float offset_x = (screen_w - dest_w) * 0.5f;
    const float offset_y = (screen_h - dest_h) * 0.5f;

    // RenderTexture は OpenGL 座標系で上下反転しているため source.height を負にする
    const Rectangle source = {0.0f, 0.0f,
                               static_cast<float>(kDesignWidth),
                               -static_cast<float>(kDesignHeight)};
    const Rectangle dest = {offset_x, offset_y, dest_w, dest_h};

    const Color tint = use_alpha ? WHITE : WHITE;
    DrawTexturePro(render_target_.texture, source, dest, {0.0f, 0.0f}, 0.0f, tint);
}

Vector2 get_virtual_mouse() {
    const Vector2 physical = GetMousePosition();
    const float screen_w = static_cast<float>(GetScreenWidth());
    const float screen_h = static_cast<float>(GetScreenHeight());

    const float scale_x = screen_w / static_cast<float>(kDesignWidth);
    const float scale_y = screen_h / static_cast<float>(kDesignHeight);
    const float scale = (scale_x < scale_y) ? scale_x : scale_y;

    const float dest_w = static_cast<float>(kDesignWidth) * scale;
    const float dest_h = static_cast<float>(kDesignHeight) * scale;
    const float offset_x = (screen_w - dest_w) * 0.5f;
    const float offset_y = (screen_h - dest_h) * 0.5f;

    return {
        (physical.x - offset_x) / scale,
        (physical.y - offset_y) / scale,
    };
}

}  // namespace virtual_screen
