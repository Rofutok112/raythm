#pragma once

#include "scene.h"

// 設定画面。ノート速度・カメラ角度・レーン幅を調整する。
class settings_scene final : public scene {
public:
    explicit settings_scene(scene_manager& manager);

    void update(float dt) override;
    void draw() override;
};
