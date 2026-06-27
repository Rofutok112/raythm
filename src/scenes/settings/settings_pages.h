#pragma once

#include <optional>
#include <vector>

#include "game_settings.h"
#include "localization/localization.h"
#include "settings/settings_gameplay_preview.h"
#include "settings/settings_key_config_state.h"
#include "settings/settings_runtime_applier.h"
#include "ui_hit.h"

struct settings_float_change {
    float game_settings::* value = nullptr;
    float next_value = 0.0f;
};

struct settings_int_change {
    int game_settings::* value = nullptr;
    int next_value = 0;
};

struct settings_bool_change {
    bool game_settings::* value = nullptr;
    bool next_value = false;
};

struct settings_page_update_result {
    std::vector<settings_float_change> float_changes;
    std::vector<settings_int_change> int_changes;
    std::vector<settings_bool_change> bool_changes;
    std::optional<localization::locale> locale;

    [[nodiscard]] bool changed() const {
        return !float_changes.empty() || !int_changes.empty() ||
               !bool_changes.empty() || locale.has_value();
    }
};

class settings_gameplay_page {
public:
    explicit settings_gameplay_page(game_settings& settings);

    void reset_interaction();
    void prepare_frame();
    [[nodiscard]] settings_page_update_result update();
    void draw() const;

private:
    game_settings& settings_;
    settings_gameplay_preview preview_;
    ui::indexed_drag_state slider_drag_;
};

class settings_audio_page {
public:
    explicit settings_audio_page(game_settings& settings);

    void reset_interaction();
    [[nodiscard]] settings_page_update_result update();
    void draw() const;

private:
    game_settings& settings_;
    ui::indexed_drag_state slider_drag_;
};

class settings_video_page {
public:
    explicit settings_video_page(game_settings& settings);

    void reset_interaction();
    [[nodiscard]] settings_page_update_result update();
    void draw() const;

private:
    game_settings& settings_;
    ui::indexed_drag_state frame_rate_drag_;
};

class settings_system_page {
public:
    explicit settings_system_page(game_settings& settings);

    void reset_interaction();
    [[nodiscard]] settings_page_update_result update();
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
