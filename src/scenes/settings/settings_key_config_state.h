#pragma once

#include <string>

#include "game_settings.h"

class settings_key_config_state {
public:
    void reset();
    void clear_selection();
    void tick(float dt);

    void update(game_settings& settings);
    void draw(const game_settings& settings) const;

    [[nodiscard]] bool blocks_navigation() const;

private:
    [[nodiscard]] static bool is_valid_play_key(int key);
    void show_error(const std::string& message);
    void handle_listening(game_settings& settings);

    int mode_ = 0;
    int slot_ = -1;
    bool listening_ = false;
    std::string error_;
    float error_timer_ = 0.0f;
};
