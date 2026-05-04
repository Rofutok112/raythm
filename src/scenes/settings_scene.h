#pragma once

#include <memory>
#include <optional>

#include "scene.h"
#include "settings/settings_layout.h"
#include "settings/settings_pages.h"
#include "song_loader.h"

struct editor_resume_state;

// 設定画面。Gameplay / Audio / Video / Network / Key Config の5ページ構成。
class settings_scene final : public scene {
public:
    enum class return_target { title, song_select, editor };

    explicit settings_scene(scene_manager& manager, return_target target = return_target::title);
    settings_scene(scene_manager& manager, song_data editor_song, editor_resume_state editor_resume);

    void on_enter() override;
    void update(float dt) override;
    void draw() override;

private:
    void update_current_page();
    void draw_current_page() const;
    void change_page(settings::page_id next_page);
    [[nodiscard]] bool current_page_blocks_navigation() const;

    settings::page_id current_page_ = settings::page_id::gameplay;
    return_target return_target_ = return_target::title;
    std::optional<song_data> editor_song_;
    std::shared_ptr<editor_resume_state> editor_resume_;
    settings_runtime_applier runtime_applier_;
    settings_gameplay_page gameplay_page_;
    settings_audio_page audio_page_;
    settings_video_page video_page_;
    settings_network_page network_page_;
    settings_key_config_page key_config_page_;
};
