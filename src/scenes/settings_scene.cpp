#include "settings_scene.h"

#include <memory>
#include <utility>

#include "editor_scene.h"
#include "raylib.h"
#include "scene_manager.h"
#include "settings_io.h"
#include "settings/settings_layout.h"
#include "song_select/song_select_navigation.h"
#include "theme.h"
#include "ui_draw.h"
#include "virtual_screen.h"

settings_scene::settings_scene(scene_manager& manager, return_target target)
    : scene(manager),
      return_target_(target),
      gameplay_page_(g_settings),
      audio_page_(g_settings, runtime_applier_),
      video_page_(g_settings, runtime_applier_),
      key_config_page_(g_settings) {
}

settings_scene::settings_scene(scene_manager& manager, song_data editor_song, editor_resume_state editor_resume)
    : settings_scene(manager, return_target::editor) {
    editor_song_ = std::move(editor_song);
    editor_resume_ = std::make_shared<editor_resume_state>(std::move(editor_resume));
}

void settings_scene::on_enter() {
    current_page_ = settings::page_id::gameplay;
    gameplay_page_.reset_interaction();
    audio_page_.reset_interaction();
    video_page_.reset_interaction();
    key_config_page_.reset();
}

void settings_scene::update(float dt) {
    ui::begin_hit_regions();
    key_config_page_.tick(dt);

    if (current_page_blocks_navigation()) {
        update_current_page();
        return;
    }

    if (ui::is_clicked(settings::kBackRect, settings::kLayer)) {
        save_settings(g_settings);
        if (return_target_ == return_target::song_select) {
            manager_.change_scene(song_select::make_seamless_song_select_scene(manager_));
        } else if (return_target_ == return_target::editor && editor_song_.has_value() && editor_resume_) {
            manager_.change_scene(std::make_unique<editor_scene>(
                manager_, std::move(*editor_song_), std::move(*editor_resume_)));
        } else {
            manager_.change_scene(song_select::make_title_scene(manager_));
        }
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

void settings_scene::draw() {
    const auto& t = *g_theme;
    virtual_screen::begin();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);
    ui::draw_panel(settings::kSidebarRect);
    ui::draw_panel(settings::kContentRect);

    ui::draw_header_block(settings::kSidebarHeaderRect, "SETTINGS", "Saved on exit");

    Rectangle tabs[settings::kPageCount];
    settings::build_tab_rects(tabs);
    for (int i = 0; i < settings::kPageCount; ++i) {
        const settings::page_descriptor& descriptor = settings::page_descriptor_for(static_cast<settings::page_id>(i));
        if (static_cast<int>(current_page_) == i) {
            ui::draw_button_colored(tabs[i], descriptor.navigation_label, 22,
                                    t.row_selected, t.row_active, t.text);
        } else {
            ui::draw_button_colored(tabs[i], descriptor.navigation_label, 22,
                                    t.row, t.row_hover, t.text_secondary);
        }
    }

    draw_marquee_text("Click tabs to switch pages", settings::kSidebarHintRect.x, settings::kSidebarHintRect.y,
                      20, t.text_muted, settings::kSidebarHintRect.width, GetTime());
    ui::draw_button(settings::kBackRect, "BACK", 22);

    const settings::page_descriptor& descriptor = settings::page_descriptor_for(current_page_);
    ui::draw_header_block(settings::kContentHeaderRect, descriptor.title, descriptor.subtitle);

    draw_current_page();

    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void settings_scene::update_current_page() {
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
        case settings::page_id::key_config:
            key_config_page_.update();
            break;
    }
}

void settings_scene::draw_current_page() const {
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
        case settings::page_id::key_config:
            key_config_page_.draw();
            break;
    }
}

void settings_scene::change_page(settings::page_id next_page) {
    gameplay_page_.reset_interaction();
    audio_page_.reset_interaction();
    video_page_.reset_interaction();
    if (current_page_ == settings::page_id::key_config && next_page != settings::page_id::key_config) {
        key_config_page_.clear_selection();
    }
    current_page_ = next_page;
}

bool settings_scene::current_page_blocks_navigation() const {
    return current_page_ == settings::page_id::key_config && key_config_page_.blocks_navigation();
}
