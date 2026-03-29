#pragma once

#include <string>

#include "data_models.h"
#include "scene.h"
#include "score_system.h"

// リザルト画面。プレイ終了後のスコア・ランク・判定内訳を表示する。
class result_scene final : public scene {
public:
    result_scene(scene_manager& manager, result_data result, bool ranking_enabled,
                 song_data song, std::string chart_path, chart_meta chart, int key_count);

    void update(float dt) override;
    void draw() override;

private:
    result_data result_;
    bool ranking_enabled_ = true;
    song_data song_;
    std::string chart_path_;
    chart_meta chart_;
    int key_count_ = 4;
    float fade_in_timer_ = 1.0f;
};
