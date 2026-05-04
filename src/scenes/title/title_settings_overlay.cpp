#include "title/title_settings_overlay.h"

#include <algorithm>

#include "raylib.h"
#include "rlgl.h"
#include "scene_common.h"
#include "settings_io.h"
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

}  // namespace

title_settings_overlay::title_settings_overlay(game_settings& settings)
    : gameplay_page_(settings),
      audio_page_(settings, runtime_applier_),
      video_page_(settings, runtime_applier_),
      network_page_(settings),
      key_config_page_(settings) {
}

void title_settings_overlay::open() {
    current_page_ = settings::page_id::gameplay;
    animation_ = 0.0f;
    closing_ = false;
    gameplay_page_.reset_interaction();
    audio_page_.reset_interaction();
    video_page_.reset_interaction();
    network_page_.reset_interaction();
    key_config_page_.reset();
}

void title_settings_overlay::save() {
    save_settings(g_settings);
}

void title_settings_overlay::request_close() {
    if (closing_) {
        return;
    }

    save();
    key_config_page_.clear_selection();
    closing_ = true;
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
    key_config_page_.tick(dt);

    if (closing_ || animation_ < 0.95f) {
        return;
    }

    if (current_page_blocks_navigation()) {
        update_current_page();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
        ui::is_clicked(settings::kBackRect, settings::kLayer)) {
        request_close();
        return;
    }

    Rectangle tabs[settings::kPageCount];
    settings::build_tab_rects(tabs);
    for (int i = 0; i < settings::kPageCount; ++i) {
        if (ui::is_clicked(tabs[i], settings::kLayer)) {
            change_page(static_cast<settings::page_id>(i));
            break;
        }
    }

    update_current_page();
}

void title_settings_overlay::draw() const {
    const float eased = tween::ease_out_cubic(animation_);
    const float slide_y = (1.0f - eased) * kViewSlideOffsetY;
    const ui_theme* previous_theme = g_theme;
    ui_theme overlay_theme = make_overlay_theme(*previous_theme, eased);
    g_theme = &overlay_theme;
    const auto& theme = *g_theme;

    rlPushMatrix();
    rlTranslatef(0.0f, slide_y, 0.0f);

    ui::draw_panel(settings::kSidebarRect);
    ui::draw_panel(settings::kContentRect);

    ui::draw_header_block(settings::kSidebarHeaderRect, "SETTINGS", "Saved on back");

    Rectangle tabs[settings::kPageCount];
    settings::build_tab_rects(tabs);
    for (int i = 0; i < settings::kPageCount; ++i) {
        const settings::page_descriptor& descriptor =
            settings::page_descriptor_for(static_cast<settings::page_id>(i));
        if (static_cast<int>(current_page_) == i) {
            ui::draw_button_colored(tabs[i], descriptor.navigation_label, 22,
                                    theme.row_selected, theme.row_active, theme.text);
        } else {
            ui::draw_button_colored(tabs[i], descriptor.navigation_label, 22,
                                    theme.row, theme.row_hover, theme.text_secondary);
        }
    }

    draw_marquee_text("ESC or right click goes back", settings::kSidebarHintRect.x,
                      settings::kSidebarHintRect.y, 20, theme.text_muted,
                      settings::kSidebarHintRect.width, GetTime());
    ui::draw_button(settings::kBackRect, "BACK", 22);

    const settings::page_descriptor& descriptor = settings::page_descriptor_for(current_page_);
    ui::draw_header_block(settings::kContentHeaderRect, descriptor.title, descriptor.subtitle);

    draw_current_page();

    rlPopMatrix();
    g_theme = previous_theme;
}

bool title_settings_overlay::closing() const {
    return closing_;
}

bool title_settings_overlay::closed() const {
    return closing_ && animation_ <= 0.0f;
}

void title_settings_overlay::update_current_page() {
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
        case settings::page_id::network:
            network_page_.update();
            break;
        case settings::page_id::key_config:
            key_config_page_.update();
            break;
    }
}

void title_settings_overlay::draw_current_page() const {
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
        case settings::page_id::network:
            network_page_.draw();
            break;
        case settings::page_id::key_config:
            key_config_page_.draw();
            break;
    }
}

void title_settings_overlay::change_page(settings::page_id next_page) {
    gameplay_page_.reset_interaction();
    audio_page_.reset_interaction();
    video_page_.reset_interaction();
    network_page_.reset_interaction();
    if (current_page_ == settings::page_id::key_config && next_page != settings::page_id::key_config) {
        key_config_page_.clear_selection();
    }
    current_page_ = next_page;
}

bool title_settings_overlay::current_page_blocks_navigation() const {
    return current_page_ == settings::page_id::key_config && key_config_page_.blocks_navigation();
}
