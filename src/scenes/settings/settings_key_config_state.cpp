#include "settings/settings_key_config_state.h"

#include <algorithm>
#include <span>

#include "key_names.h"
#include "localization/localization.h"
#include "raylib.h"
#include "settings/settings_layout.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_hit.h"

namespace {

struct key_slot_row {
    Rectangle rect{};
    int index = 0;
    const char* key_label = "";
    bool selected = false;
    bool listening = false;
};

int key_count_for_mode(int mode) {
    return mode == 0 ? 4 : 6;
}

const char* key_mode_label(int mode) {
    return mode == 0 ? "4K" : "6K";
}

std::span<KeyboardKey> key_span_for_mode(game_settings& settings, int mode) {
    return mode == 0
        ? std::span<KeyboardKey>(settings.keys.keys_4)
        : std::span<KeyboardKey>(settings.keys.keys_6);
}

std::span<const KeyboardKey> key_span_for_mode(const game_settings& settings, int mode) {
    return mode == 0
        ? std::span<const KeyboardKey>(settings.keys.keys_4)
        : std::span<const KeyboardKey>(settings.keys.keys_6);
}

key_slot_row key_slot_row_for(int index, const KeyboardKey* keys, int selected_slot, bool listening) {
    const bool selected = selected_slot == index;
    const bool is_listening = selected && listening;
    return {
        .rect = settings::key_slot_rect(index),
        .index = index,
        .key_label = is_listening ? localization::tr(localization::text_key::press_a_key)
                                  : get_key_name(keys[index]),
        .selected = selected,
        .listening = is_listening,
    };
}

void draw_key_slot_row(const key_slot_row& row) {
    const auto& theme = *g_theme;
    const ui::row_state row_state = ui::row(row.rect, {
        .layer = settings::kLayer,
        .border_width = 2.0f,
        .bg = row.selected ? theme.row_selected : theme.row,
        .bg_hover = row.selected ? theme.row_active : theme.row_hover,
        .border_color = row.selected ? theme.border_active : theme.border,
        .custom_colors = true,
    });
    const settings::key_slot_layout layout = settings::key_slot_layout_for(row_state.visual);
    ui::draw_text_in_rect(TextFormat("%s %d", localization::tr(localization::text_key::lane), row.index + 1),
                          24, layout.label_rect, theme.text, ui::text_align::left);
    ui::draw_text_in_rect(row.key_label, 24, layout.value_rect,
                          row.listening ? theme.error : theme.text_dim, ui::text_align::right);
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

int clicked_key_slot(std::span<const KeyboardKey> keys, int selected_slot, bool listening) {
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        const key_slot_row row = key_slot_row_for(i, keys.data(), selected_slot, listening);
        if (ui::is_clicked(row.rect, settings::kLayer)) {
            return row.index;
        }
    }
    return -1;
}

void draw_key_slot_rows(std::span<const KeyboardKey> keys, int selected_slot, bool listening) {
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        draw_key_slot_row(key_slot_row_for(i, keys.data(), selected_slot, listening));
    }
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
    if (ui::is_escape_pressed()) {
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

    std::span<KeyboardKey> keys = key_span_for_mode(settings, mode_);
    const int count = static_cast<int>(keys.size());
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

    const std::span<const KeyboardKey> keys = key_span_for_mode(settings, mode_);
    const int clicked_slot = clicked_key_slot(keys, slot_, listening_);
    if (clicked_slot >= 0) {
        if (slot_ == clicked_slot) {
            listening_ = true;
            error_.clear();
        } else {
            slot_ = clicked_slot;
        }
    }
}

void settings_key_config_state::draw(const game_settings& settings) const {
    ui::value_selector(settings::kKeyModeRect, localization::tr(localization::text_key::mode),
                       key_mode_label(mode_), settings::value_selector_options());

    const std::span<const KeyboardKey> keys = key_span_for_mode(settings, mode_);
    draw_key_slot_rows(keys, slot_, listening_);
    const int count = key_count_for_mode(mode_);
    draw_key_config_error(error_, error_timer_, count);
}
