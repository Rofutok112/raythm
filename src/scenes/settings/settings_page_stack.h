#pragma once

#include "game_settings.h"
#include "settings/settings_layout.h"
#include "settings/settings_pages.h"

class settings_page_stack {
public:
    explicit settings_page_stack(game_settings& settings);

    void reset();
    void tick(float dt);
    void update_current_page();
    void draw_current_page() const;
    void change_page(settings::page_id next_page);
    void clear_key_config_selection();

    [[nodiscard]] settings::page_id current_page() const;
    [[nodiscard]] bool current_page_blocks_navigation() const;

private:
    void reset_interactions();

    settings_runtime_applier runtime_applier_;
    settings_gameplay_page gameplay_page_;
    settings_audio_page audio_page_;
    settings_video_page video_page_;
    settings_system_page system_page_;
    settings_key_config_page key_config_page_;
    settings::page_id current_page_ = settings::page_id::gameplay;
};
