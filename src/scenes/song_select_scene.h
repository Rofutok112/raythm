#pragma once

#include "scene.h"

// 曲選択画面。難易度・キーモードを選んでプレイ画面に遷移する。
class song_select_scene final : public scene {
public:
    explicit song_select_scene(scene_manager& manager);

    void update(float dt) override;
    void draw() override;

private:
    int difficulty_index_ = 0;
    int key_mode_index_ = 0;
};
