#pragma once

#include "scene.h"

// タイトル画面。曲選択画面・設定画面への遷移を提供する。
class title_scene final : public scene {
public:
    explicit title_scene(scene_manager& manager);

    void update(float dt) override;
    void draw() override;
};
