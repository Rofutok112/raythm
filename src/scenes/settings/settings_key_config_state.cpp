#include "settings/settings_key_config_state.h"

#include <algorithm>
#include <span>

#include "key_names.h"
#include "localization/localization.h"
#include "raylib.h"
#include "settings/settings_layout.h"
#include "theme.h"
#include "ui_draw.h"

namespace {

void draw_key_slot_row(Rectangle row_rect, int index, const char* key_label, bool selected, bool listening) {
    const auto& theme = *g_theme;
    const ui::row_state row_state = ui::row(row_rect, {
        .layer = settings::kLayer,
        .border_width = 2.0f,
        .bg = selected ? theme.row_selected : theme.row,
        .bg_hover = selected ? theme.row_active : theme.row_hover,
        .border_color = selected ? theme.border_active : theme.border,
        .custom_colors = true,
    });
    const settings::key_slot_layout layout = settings::key_slot_layout_for(row_state.visual);
    ui::draw_text_in_rect(TextFormat("%s %d", localization::tr(localization::text_key::lane), index + 1),
                          24, layout.label_rect, theme.text, ui::text_align::left);
    ui::draw_text_in_rect(key_label, 24, layout.value_rect,
                          listening ? theme.error : theme.text_dim, ui::text_align::right);
}

void draw_key_config_error(const std::string& message, float timer, int visible_key_count) {
    if (timer <= 0.0f || message.empty()) {
        return;
    }
    const auto& theme = *g_theme;
    const unsigned char alpha = static_cast<unsigned char>(std::min(timer / 0.3f, 1.0f) * 255.0f);
    ui::draw_text_in_rect(message.c_str(), 22,
                          settings::key_config_error_rect(visible_key_count),
                          with_alpha(theme.error, alpha), ui::text_align::left);
}

}  // namespace

bool settings_key_config_state::is_valid_play_key(int key) {
    return key != KEY_ESCAPE && key != KEY_ENTER && key != KEY_UP && key != KEY_DOWN &&
           key != KEY_LEFT && key != KEY_RIGHT && key != KEY_BACKSPACE && key != KEY_NULL;
}

void settings_key_config_state::reset() {
    listening_ = false;
    slot_ = -1;
    error_.clear();
    error_timer_ = 0.0f;
}

void settings_key_config_state::clear_selection() {
    listening_ = false;
    slot_ = -1;
}

void settings_key_config_state::tick(float dt) {
    error_timer_ = std::max(0.0f, error_timer_ - dt);
}

bool settings_key_config_state::blocks_navigation() const {
    return listening_;
}

void settings_key_config_state::show_error(const std::string& message) {
    error_ = message;
    error_timer_ = 2.0f;
}

void settings_key_config_state::handle_listening(game_settings& settings) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        listening_ = false;
        return;
    }

    const int pressed = GetKeyPressed();
    if (pressed == KEY_NULL || slot_ < 0) {
        return;
    }

    if (!is_valid_play_key(pressed)) {
        show_error(localization::tr(localization::text_key::key_cannot_be_assigned));
        return;
    }

    std::span<KeyboardKey> keys = mode_ == 0
        ? std::span<KeyboardKey>(settings.keys.keys_4)
        : std::span<KeyboardKey>(settings.keys.keys_6);
    const int count = mode_ == 0 ? 4 : 6;
    for (int i = 0; i < count; ++i) {
        if (i != slot_ && keys[static_cast<std::size_t>(i)] == static_cast<KeyboardKey>(pressed)) {
            show_error(TextFormat("%s: %s (%s %d)",
                                  localization::tr(localization::text_key::key_already_assigned),
                                  get_key_name(pressed),
                                  localization::tr(localization::text_key::lane),
                                  i + 1));
            return;
        }
    }

    keys[static_cast<std::size_t>(slot_)] = static_cast<KeyboardKey>(pressed);
    listening_ = false;
    error_.clear();
}

void settings_key_config_state::update(game_settings& settings) {
    if (listening_) {
        handle_listening(settings);
        return;
    }

    const Rectangle mode_left = settings::arrow_left_rect(settings::kKeyModeRect);
    const Rectangle mode_right = settings::arrow_right_rect(settings::kKeyModeRect);
    if (ui::is_clicked(mode_left, settings::kLayer) || ui::is_clicked(mode_right, settings::kLayer)) {
        mode_ = 1 - mode_;
        slot_ = -1;
        return;
    }

    const int max_keys = mode_ == 0 ? 4 : 6;
    for (int i = 0; i < max_keys; ++i) {
        const Rectangle row_rect = settings::key_slot_rect(i);
        if (ui::is_clicked(row_rect, settings::kLayer)) {
            if (slot_ == i) {
                listening_ = true;
                error_.clear();
            } else {
                slot_ = i;
            }
            return;
        }
    }
}

void settings_key_config_state::draw(const game_settings& settings) const {
    ui::value_selector(settings::kKeyModeRect, localization::tr(localization::text_key::mode),
                       mode_ == 0 ? "4K" : "6K", settings::value_selector_options());

    const std::span<const KeyboardKey> keys = mode_ == 0
        ? std::span<const KeyboardKey>(settings.keys.keys_4)
        : std::span<const KeyboardKey>(settings.keys.keys_6);
    const int count = mode_ == 0 ? 4 : 6;
    for (int i = 0; i < count; ++i) {
        const bool selected = slot_ == i;
        const bool is_listening = selected && listening_;
        const Rectangle row_rect = settings::key_slot_rect(i);
        const char* key_label = is_listening ? localization::tr(localization::text_key::press_a_key)
                                             : get_key_name(keys[static_cast<std::size_t>(i)]);
        draw_key_slot_row(row_rect, i, key_label, selected, is_listening);
    }
    draw_key_config_error(error_, error_timer_, count);
}
