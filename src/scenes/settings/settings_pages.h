#pragma once

#include "game_settings.h"
#include "settings/settings_key_config_state.h"
#include "settings/settings_runtime_applier.h"

class settings_gameplay_page {
public:
    explicit settings_gameplay_page(game_settings& settings);

    void reset_interaction();
    void update();
    void draw() const;

private:
    enum class slider { none, note_speed, camera_angle, lane_width, note_height };

    game_settings& settings_;
    slider active_slider_ = slider::none;
};

class settings_audio_page {
public:
    settings_audio_page(game_settings& settings, const settings_runtime_applier& runtime_applier);

    void reset_interaction();
    void update();
    void draw() const;

private:
    enum class slider { none, bgm_volume, se_volume };

    game_settings& settings_;
    const settings_runtime_applier& runtime_applier_;
    slider active_slider_ = slider::none;
};

class settings_video_page {
public:
    settings_video_page(game_settings& settings, const settings_runtime_applier& runtime_applier);

    void reset_interaction();
    void update();
    void draw() const;

private:
    game_settings& settings_;
    const settings_runtime_applier& runtime_applier_;
    bool dragging_frame_rate_ = false;
};

class settings_network_page {
public:
    explicit settings_network_page(game_settings& settings);

    void reset_interaction();
    void update();
    void draw() const;

private:
    game_settings& settings_;
};

class settings_key_config_page {
public:
    explicit settings_key_config_page(game_settings& settings);

    void reset();
    void clear_selection();
    void tick(float dt);
    void update();
    void draw() const;

    [[nodiscard]] bool blocks_navigation() const;

private:
    game_settings& settings_;
    settings_key_config_state state_;
};
