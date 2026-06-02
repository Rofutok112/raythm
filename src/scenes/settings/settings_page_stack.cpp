#include "settings/settings_page_stack.h"

settings_page_stack::settings_page_stack(game_settings& settings)
    : gameplay_page_(settings),
      audio_page_(settings, runtime_applier_),
      video_page_(settings, runtime_applier_),
      system_page_(settings, runtime_applier_),
      key_config_page_(settings) {
}

void settings_page_stack::reset() {
    current_page_ = settings::page_id::gameplay;
    reset_interactions();
    key_config_page_.reset();
}

void settings_page_stack::tick(float dt) {
    key_config_page_.tick(dt);
}

void settings_page_stack::prepare_current_page() {
    if (current_page_ == settings::page_id::gameplay) {
        gameplay_page_.prepare_frame();
    }
}

void settings_page_stack::update_current_page() {
    switch (current_page_) {
        case settings::page_id::gameplay:
            gameplay_page_.update();
            break;
        case settings::page_id::audio:
            audio_page_.update();
            break;
        case settings::page_id::video:
            video_page_.update();
            break;
        case settings::page_id::system:
            system_page_.update();
            break;
        case settings::page_id::key_config:
            key_config_page_.update();
            break;
    }
}

void settings_page_stack::draw_current_page() const {
    switch (current_page_) {
        case settings::page_id::gameplay:
            gameplay_page_.draw();
            break;
        case settings::page_id::audio:
            audio_page_.draw();
            break;
        case settings::page_id::video:
            video_page_.draw();
            break;
        case settings::page_id::system:
            system_page_.draw();
            break;
        case settings::page_id::key_config:
            key_config_page_.draw();
            break;
    }
}

void settings_page_stack::change_page(settings::page_id next_page) {
    reset_interactions();
    if (current_page_ == settings::page_id::key_config && next_page != settings::page_id::key_config) {
        key_config_page_.clear_selection();
    }
    current_page_ = next_page;
}

void settings_page_stack::clear_key_config_selection() {
    key_config_page_.clear_selection();
}

settings::page_id settings_page_stack::current_page() const {
    return current_page_;
}

bool settings_page_stack::current_page_blocks_navigation() const {
    return current_page_ == settings::page_id::key_config && key_config_page_.blocks_navigation();
}

void settings_page_stack::reset_interactions() {
    gameplay_page_.reset_interaction();
    audio_page_.reset_interaction();
    video_page_.reset_interaction();
    system_page_.reset_interaction();
}
