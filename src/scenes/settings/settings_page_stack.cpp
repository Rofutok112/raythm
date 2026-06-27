#include "settings/settings_page_stack.h"

settings_page_stack::settings_page_stack(game_settings& settings)
    : settings_(settings),
      gameplay_page_(settings),
      audio_page_(settings),
      video_page_(settings),
      system_page_(settings),
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

settings_page_update_result settings_page_stack::update_current_page() {
    switch (current_page_) {
        case settings::page_id::gameplay:
            return gameplay_page_.update();
        case settings::page_id::audio:
            return audio_page_.update();
        case settings::page_id::video:
            return video_page_.update();
        case settings::page_id::system:
            return system_page_.update();
        case settings::page_id::key_config:
            key_config_page_.update();
            break;
    }
    return {};
}

void settings_page_stack::apply_update_result(const settings_page_update_result& result) {
    for (const settings_float_change& change : result.float_changes) {
        if (change.value == nullptr) {
            continue;
        }
        settings_.*(change.value) = change.next_value;
        if (change.value == &game_settings::bgm_volume) {
            runtime_applier_.apply_bgm_volume(settings_.bgm_volume);
        } else if (change.value == &game_settings::se_volume) {
            runtime_applier_.apply_se_volume(settings_.se_volume);
        }
    }
    for (const settings_int_change& change : result.int_changes) {
        if (change.value != nullptr) {
            settings_.*(change.value) = change.next_value;
        }
    }
    for (const settings_bool_change& change : result.bool_changes) {
        if (change.value == nullptr) {
            continue;
        }
        settings_.*(change.value) = change.next_value;
        if (change.value == &game_settings::loudness_normalization_enabled) {
            runtime_applier_.apply_loudness_normalization(settings_.loudness_normalization_enabled);
        } else if (change.value == &game_settings::fullscreen) {
            runtime_applier_.apply_fullscreen(settings_.fullscreen);
        } else if (change.value == &game_settings::dark_mode) {
            runtime_applier_.apply_theme(settings_.dark_mode);
        }
    }
    if (result.locale.has_value()) {
        settings_.ui_locale = *result.locale;
        runtime_applier_.apply_locale(settings_.ui_locale);
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
