#pragma once

#include <string>

#include "data_models.h"
#include "mv/mv_storage.h"
#include "mv/mv_script_panel.h"
#include "scene.h"
#include "ui_text_input.h"

// MV(.rmv)スクリプトの編集画面。曲選択画面から遷移する。
class mv_editor_scene final : public scene {
public:
    mv_editor_scene(scene_manager& manager, song_data song);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    enum class tab {
        script,
        metadata,
    };

    void compile_script();
    void save_mv();

    song_data song_;
    mv::mv_package package_;
    mv_script_panel_state panel_state_;
    ui::text_input_state name_input_;
    ui::text_input_state author_input_;
    tab active_tab_ = tab::script;
    bool dirty_ = false;
    double last_change_time_ = 0.0;
    bool pending_compile_ = false;
};
