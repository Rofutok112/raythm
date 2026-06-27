#include "title/title_settings_overlay.h"

#include <algorithm>
#include <optional>

#include "raylib.h"
#include "rlgl.h"
#include "scene_common.h"
#include "settings_io.h"
#include "settings/settings_shell_view.h"
#include "localization/localization.h"
#include "theme.h"
#include "tween.h"
#include "ui_draw.h"

namespace {

constexpr float kViewAnimSpeed = 7.5f;
constexpr float kViewSlideOffsetY = 36.0f;
constexpr unsigned char kPanelAlpha = 205;
constexpr unsigned char kSectionAlpha = 190;
constexpr unsigned char kRowAlpha = 178;
constexpr unsigned char kRowHoverAlpha = 212;
constexpr unsigned char kRowSelectedAlpha = 218;
constexpr unsigned char kBorderAlpha = 210;

class scoped_theme_override {
public:
    explicit scoped_theme_override(const ui_theme& theme)
        : previous_(g_theme) {
        g_theme = &theme;
    }

    ~scoped_theme_override() {
        g_theme = previous_;
    }

    scoped_theme_override(const scoped_theme_override&) = delete;
    scoped_theme_override& operator=(const scoped_theme_override&) = delete;

private:
    const ui_theme* previous_ = nullptr;
};

Color fade_alpha(Color color, float fade) {
    const float alpha = static_cast<float>(color.a) * std::clamp(fade, 0.0f, 1.0f);
    return with_alpha(color, static_cast<unsigned char>(std::clamp(alpha, 0.0f, 255.0f)));
}

Color cap_alpha(Color color, unsigned char alpha, float fade) {
    const float capped = static_cast<float>(alpha) * std::clamp(fade, 0.0f, 1.0f);
    return with_alpha(color, static_cast<unsigned char>(std::clamp(capped, 0.0f, 255.0f)));
}

ui_theme make_overlay_theme(const ui_theme& source, float fade) {
    ui_theme theme = source;
    theme.panel = cap_alpha(source.panel, kPanelAlpha, fade);
    theme.section = cap_alpha(source.section, kSectionAlpha, fade);
    theme.row = cap_alpha(source.row, kRowAlpha, fade);
    theme.row_hover = cap_alpha(source.row_hover, kRowHoverAlpha, fade);
    theme.row_selected = cap_alpha(source.row_selected, kRowSelectedAlpha, fade);
    theme.row_selected_hover = cap_alpha(source.row_selected_hover, kRowSelectedAlpha, fade);
    theme.row_active = cap_alpha(source.row_active, kRowSelectedAlpha, fade);
    theme.row_list_hover = cap_alpha(source.row_list_hover, kRowHoverAlpha, fade);
    theme.row_soft = cap_alpha(source.row_soft, source.row_soft_alpha, fade);
    theme.row_soft_hover = cap_alpha(source.row_soft_hover, source.row_soft_hover_alpha, fade);
    theme.row_soft_selected = cap_alpha(source.row_soft_selected, source.row_soft_selected_alpha, fade);
    theme.row_soft_selected_hover = cap_alpha(source.row_soft_selected_hover, source.row_soft_selected_hover_alpha, fade);
    theme.border = cap_alpha(source.border, kBorderAlpha, fade);
    theme.border_light = cap_alpha(source.border_light, kBorderAlpha, fade);
    theme.border_active = fade_alpha(source.border_active, fade);
    theme.text = fade_alpha(source.text, fade);
    theme.text_secondary = fade_alpha(source.text_secondary, fade);
    theme.text_muted = fade_alpha(source.text_muted, fade);
    theme.text_dim = fade_alpha(source.text_dim, fade);
    theme.text_hint = fade_alpha(source.text_hint, fade);
    theme.slider_track = cap_alpha(source.slider_track, kRowAlpha, fade);
    theme.slider_fill = fade_alpha(source.slider_fill, fade);
    theme.slider_knob = fade_alpha(source.slider_knob, fade);
    theme.scrollbar_track = cap_alpha(source.scrollbar_track, kRowAlpha, fade);
    theme.scrollbar_thumb = fade_alpha(source.scrollbar_thumb, fade);
    return theme;
}

float overlay_slide_y(float animation) {
    const float eased = tween::ease_out_cubic(animation);
    return (1.0f - eased) * kViewSlideOffsetY;
}

}  // namespace

title_settings_overlay::title_settings_overlay(game_settings& settings)
    : pages_(settings) {
}

void title_settings_overlay::open() {
    pages_.reset();
    animation_ = 0.0f;
    closing_ = false;
}

void title_settings_overlay::save() {
    save_settings(g_settings);
}

void title_settings_overlay::request_close() {
    if (closing_) {
        return;
    }

    save();
    pages_.clear_key_config_selection();
    closing_ = true;
}

void title_settings_overlay::prepare_current_page() {
    ui_theme overlay_theme = make_overlay_theme(*g_theme, tween::ease_out_cubic(animation_));
    const scoped_theme_override theme_override(overlay_theme);
    pages_.prepare_current_page();
}

void title_settings_overlay::update_animation(bool active, float dt) {
    if (!active) {
        animation_ = 0.0f;
        closing_ = false;
        return;
    }

    if (closing_) {
        animation_ = tween::retreat(animation_, dt, kViewAnimSpeed);
    } else {
        animation_ = tween::advance(animation_, dt, kViewAnimSpeed);
    }
}

void title_settings_overlay::update(float dt) {
    pages_.tick(dt);

    if (closing_ || animation_ < 0.95f) {
        return;
    }

    if (pages_.current_page_blocks_navigation()) {
        const settings_page_update_result page_result = pages_.update_current_page();
        pages_.apply_update_result(page_result);
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        request_close();
        return;
    }

    const settings_page_update_result page_result = pages_.update_current_page();
    pages_.apply_update_result(page_result);
}

settings::shell_draw_result title_settings_overlay::draw() const {
    const float eased = tween::ease_out_cubic(animation_);
    const float slide_y = overlay_slide_y(animation_);
    ui_theme overlay_theme = make_overlay_theme(*g_theme, eased);
    const scoped_theme_override theme_override(overlay_theme);
    settings::shell_draw_result result;

    rlPushMatrix();
    rlTranslatef(0.0f, slide_y, 0.0f);

    result = settings::draw_shell(pages_.current_page());
    pages_.draw_current_page();

    rlPopMatrix();
    return result;
}

void title_settings_overlay::apply_draw_result(const settings::shell_draw_result& result) {
    if (closing_ || animation_ < 0.95f || pages_.current_page_blocks_navigation()) {
        return;
    }
    if (result.back_requested) {
        request_close();
        return;
    }
    if (result.selected_page.has_value()) {
        pages_.change_page(*result.selected_page);
    }
}

bool title_settings_overlay::closing() const {
    return closing_;
}

bool title_settings_overlay::closed() const {
    return closing_ && animation_ <= 0.0f;
}
