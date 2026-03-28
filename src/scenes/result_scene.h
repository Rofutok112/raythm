#pragma once

#include "scene.h"
#include "score_system.h"

// リザルト画面。プレイ終了後のスコア・ランク・判定内訳を表示する。
class result_scene final : public scene {
public:
    result_scene(scene_manager& manager, result_data result, bool ranking_enabled);

    void update(float dt) override;
    void draw() override;

private:
    result_data result_;
    bool ranking_enabled_ = true;
};
