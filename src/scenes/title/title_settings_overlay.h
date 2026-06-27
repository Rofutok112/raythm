#pragma once

#include "game_settings.h"
#include "settings/settings_page_stack.h"
#include "settings/settings_shell_view.h"

class title_settings_overlay {
public:
    explicit title_settings_overlay(game_settings& settings);

    void open();
    void save();
    void request_close();
    void prepare_current_page();
    void update_animation(bool active, float dt);
    void update(float dt);
    [[nodiscard]] settings::shell_draw_result draw() const;
    void apply_draw_result(const settings::shell_draw_result& result);

    [[nodiscard]] bool closing() const;
    [[nodiscard]] bool closed() const;

private:
    settings_page_stack pages_;
    float animation_ = 0.0f;
    bool closing_ = false;
};
