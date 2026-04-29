#pragma once

#include "game_settings.h"
#include "settings/settings_layout.h"
#include "settings/settings_pages.h"

class title_settings_overlay {
public:
    explicit title_settings_overlay(game_settings& settings);

    void open();
    void save();
    void request_close();
    void update_animation(bool active, float dt);
    void update(float dt);
    void draw() const;

    [[nodiscard]] bool closing() const;
    [[nodiscard]] bool closed() const;

private:
    void update_current_page();
    void draw_current_page() const;
    void change_page(settings::page_id next_page);
    [[nodiscard]] bool current_page_blocks_navigation() const;

    settings_runtime_applier runtime_applier_;
    settings_gameplay_page gameplay_page_;
    settings_audio_page audio_page_;
    settings_video_page video_page_;
    settings_key_config_page key_config_page_;
    settings::page_id current_page_ = settings::page_id::gameplay;
    float animation_ = 0.0f;
    bool closing_ = false;
};
