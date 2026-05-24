#pragma once

#include <vector>
#include <string>
#include <future>
#include <optional>

#include "editor/editor_scene_types.h"
#include "multiplayer/multiplayer_client.h"
#include "play/play_mv_controller.h"
#include "play/play_note_draw_queue.h"
#include "play/play_session_types.h"
#include "raylib.h"
#include "scene.h"

// プレイ画面。譜面を読み込み、ノートの描画・入力判定・スコア計算を行うメインのゲームシーン。
class play_scene final : public scene {
public:
    explicit play_scene(scene_manager& manager, int key_count);
    play_scene(scene_manager& manager, song_data song, std::string chart_path, int key_count,
               float chart_level = 0.0f);
    play_scene(scene_manager& manager, song_data song, std::string chart_path, int key_count,
               float chart_level, std::string multiplayer_room_id, std::string multiplayer_match_id);
    play_scene(scene_manager& manager, song_data song, chart_data chart, int start_tick,
               editor_resume_state editor_resume);
    ~play_scene() override;

    void on_enter() override;
    void on_exit() override;
    void on_app_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    Camera3D make_play_camera() const;
    bool get_lane_view_bounds(const Camera3D& camera, float& lane_start_z, float& judgement_z, float& lane_end_z) const;
    void load_jacket_texture();
    void unload_jacket_texture();
    void rebuild_hit_regions() const;
    double get_visual_ms() const;
    void apply_navigation(play_navigation_request navigation);
    void update_start_gate(float dt);
    void sync_multiplayer_score(float dt);

    play_start_request request_;
    play_session_state state_;
    play_note_draw_queue draw_queue_;
    play_mv_controller mv_controller_;
    Texture2D jacket_texture_{};
    bool jacket_texture_loaded_ = false;
    bool start_gate_active_ = false;
    bool multiplayer_loaded_sent_ = false;
    bool multiplayer_countdown_started_ = false;
    float start_gate_timer_ = 0.0f;
    float match_loaded_poll_t_ = 0.0f;
    float multiplayer_score_sync_t_ = 0.0f;
    std::optional<std::future<multiplayer::room_operation_result>> multiplayer_loaded_future_;
    std::optional<std::future<multiplayer::room_operation_result>> multiplayer_score_future_;
    std::optional<std::future<multiplayer::room_operation_result>> multiplayer_room_future_;
    std::unique_ptr<multiplayer::client::realtime_client> multiplayer_realtime_;
};
