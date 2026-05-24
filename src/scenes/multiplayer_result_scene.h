#pragma once

#include <future>
#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "multiplayer/multiplayer_client.h"
#include "play/play_session_types.h"
#include "raylib.h"
#include "scene.h"

class multiplayer_result_scene final : public scene {
public:
    multiplayer_result_scene(scene_manager& manager,
                             result_data result,
                             song_data song,
                             chart_meta chart,
                             int key_count,
                             std::string room_id,
                             std::string match_id,
                             std::string self_user_id,
                             std::vector<play_multiplayer_score_row> scores);
    ~multiplayer_result_scene() override;

    void on_enter() override;
    void on_exit() override;
    void on_app_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    void request_return_to_room();
    void poll_room_refresh(float dt);
    void apply_live_scores(const std::vector<multiplayer::live_score>& scores);
    void upsert_self_score();
    void sort_scores();
    void load_jacket_texture();
    void unload_jacket_texture();

    result_data result_;
    song_data song_;
    chart_meta chart_;
    int key_count_ = 4;
    std::string room_id_;
    std::string match_id_;
    std::string self_user_id_;
    std::vector<play_multiplayer_score_row> scores_;
    Texture2D jacket_texture_{};
    bool jacket_texture_loaded_ = false;
    float reveal_t_ = 0.0f;
    float refresh_t_ = 0.0f;
    float scroll_y_ = 0.0f;
    bool scrollbar_dragging_ = false;
    float scrollbar_drag_offset_ = 0.0f;
    bool returning_ = false;
    std::string selected_score_key_;
    std::string status_message_;
    std::optional<std::future<multiplayer::room_operation_result>> room_future_;
    std::optional<std::future<multiplayer::room_operation_result>> complete_future_;
};
