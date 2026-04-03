#include "settings/settings_key_config_state.h"

#include <algorithm>
#include <span>

#include "key_names.h"
#include "raylib.h"
#include "settings/settings_layout.h"
#include "theme.h"
#include "ui_draw.h"

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
        show_error("This key cannot be assigned");
        return;
    }

    std::span<KeyboardKey> keys = mode_ == 0
        ? std::span<KeyboardKey>(settings.keys.keys_4)
        : std::span<KeyboardKey>(settings.keys.keys_6);
    const int count = mode_ == 0 ? 4 : 6;
    for (int i = 0; i < count; ++i) {
        if (i != slot_ && keys[static_cast<std::size_t>(i)] == static_cast<KeyboardKey>(pressed)) {
            show_error(TextFormat("Key '%s' is already assigned to Lane %d", get_key_name(pressed), i + 1));
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
    const auto& theme = *g_theme;
    ui::draw_value_selector(settings::kKeyModeRect, "Mode", mode_ == 0 ? "4K" : "6K", settings::kLayer);

    const std::span<const KeyboardKey> keys = mode_ == 0
        ? std::span<const KeyboardKey>(settings.keys.keys_4)
        : std::span<const KeyboardKey>(settings.keys.keys_6);
    const int count = mode_ == 0 ? 4 : 6;
    for (int i = 0; i < count; ++i) {
        const bool selected = slot_ == i;
        const bool is_listening = selected && listening_;
        const Rectangle row_rect = settings::key_slot_rect(i);
        const ui::row_state row_state = ui::draw_row(row_rect,
                                                     selected ? theme.row_selected : theme.row,
                                                     selected ? theme.row_active : theme.row_hover,
                                                     selected ? theme.border_active : theme.border);
        const char* key_label = is_listening ? "Press a key..." : get_key_name(keys[static_cast<std::size_t>(i)]);
        const ui::rect_pair columns = ui::split_columns(ui::inset(row_state.visual, 18.0f), 160.0f);
        ui::draw_text_in_rect(TextFormat("Lane %d", i + 1), 24, columns.first, theme.text, ui::text_align::left);
        ui::draw_text_in_rect(key_label, 24, columns.second, is_listening ? theme.error : theme.text_dim, ui::text_align::right);
    }

    if (error_timer_ > 0.0f && !error_.empty()) {
        const unsigned char alpha = static_cast<unsigned char>(std::min(error_timer_ / 0.3f, 1.0f) * 255.0f);
        ui::draw_text_in_rect(error_.c_str(), 22,
                              ui::place(settings::kContentRect, 560.0f, 28.0f,
                                        ui::anchor::top_left, ui::anchor::top_left,
                                        {30.0f, 214.0f + static_cast<float>(count) * 62.0f + 8.0f}),
                              with_alpha(theme.error, alpha), ui::text_align::left);
    }
}
