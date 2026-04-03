#pragma once

#include <string>

#include "editor_scene.h"
#include "play/play_note_draw_queue.h"
#include "play/play_session_types.h"
#include "raylib.h"
#include "scene.h"

// プレイ画面。譜面を読み込み、ノートの描画・入力判定・スコア計算を行うメインのゲームシーン。
class play_scene final : public scene {
public:
    explicit play_scene(scene_manager& manager, int key_count);
    play_scene(scene_manager& manager, song_data song, std::string chart_path, int key_count);
    play_scene(scene_manager& manager, song_data song, chart_data chart, int start_tick,
               editor_scene::resume_state editor_resume);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    Camera3D make_play_camera() const;
    bool get_lane_view_bounds(const Camera3D& camera, float& lane_start_z, float& judgement_z, float& lane_end_z) const;
    void rebuild_hit_regions() const;
    double get_visual_ms() const;
    void apply_navigation(play_navigation_request navigation);

    play_start_request request_;
    play_session_state state_;
    play_note_draw_queue draw_queue_;
};
