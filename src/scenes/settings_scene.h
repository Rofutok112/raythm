#pragma once

#include <memory>
#include <optional>

#include "scene.h"
#include "settings/settings_page_stack.h"
#include "song_loader.h"

struct editor_resume_state;

// 設定画面。Gameplay / Audio / Video / Key Config の4ページ構成。
class settings_scene final : public scene {
public:
    enum class return_target { title, song_select, editor };

    explicit settings_scene(scene_manager& manager, return_target target = return_target::title);
    settings_scene(scene_manager& manager, song_data editor_song, editor_resume_state editor_resume);

    void on_enter() override;
    void update(float dt) override;
    void draw() override;

private:
    return_target return_target_ = return_target::title;
    std::optional<song_data> editor_song_;
    std::shared_ptr<editor_resume_state> editor_resume_;
    settings_page_stack pages_;
};
