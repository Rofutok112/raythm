#pragma once

#include <string>

#include "data_models.h"
#include "mv/mv_script_panel.h"
#include "scene.h"

// MV(.rmv)スクリプトの編集画面。曲選択画面から遷移する。
class mv_editor_scene final : public scene {
public:
    mv_editor_scene(scene_manager& manager, song_data song);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    void compile_script();
    void save_script();

    song_data song_;
    mv_script_panel_state panel_state_;
    bool dirty_ = false;
};
