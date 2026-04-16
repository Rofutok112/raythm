#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "data_models.h"
#include "ranking_service.h"
#include "scene.h"
#include "score_system.h"
#include "shared/scene_fade.h"

// リザルト画面。プレイ終了後のスコア・ランク・判定内訳を表示する。
class result_scene final : public scene {
public:
    result_scene(scene_manager& manager, result_data result, bool ranking_enabled,
                 song_data song, std::string chart_path, chart_meta chart, int key_count);

    void on_enter() override;
    void update(float dt) override;
    void draw() override;

private:
    struct online_submit_task_state {
        std::atomic<bool> done = false;
        std::mutex mutex;
        ranking_service::online_submit_result result;
    };

    void return_to_song_select() const;
    void poll_online_submit();

    result_data result_;
    bool ranking_enabled_ = true;
    song_data song_;
    std::string chart_path_;
    chart_meta chart_;
    int key_count_ = 4;
    std::shared_ptr<online_submit_task_state> online_submit_task_;
    bool online_submit_consumed_ = false;
    std::string online_submit_status_message_;
    bool online_submit_status_is_error_ = false;
    scene_fade fade_in_{scene_fade::direction::in, 1.0f, 0.65f};
};
