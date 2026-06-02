#pragma once

#include "game_settings.h"
#include "play/play_note_draw_queue.h"
#include "play/play_session_types.h"
#include "raylib.h"

class settings_gameplay_preview {
public:
    explicit settings_gameplay_preview(game_settings& settings);
    ~settings_gameplay_preview();

    void prepare_frame();
    void draw(Rectangle frame) const;

private:
    bool ensure_textures();
    void unload_textures();
    void rebuild_preview_state(double preview_time_ms);

    game_settings& settings_;
    RenderTexture2D scene_texture_{};
    RenderTexture2D lane_texture_{};
    bool scene_texture_loaded_ = false;
    bool lane_texture_loaded_ = false;
    play_session_state state_;
    play_note_draw_queue draw_queue_;
};
