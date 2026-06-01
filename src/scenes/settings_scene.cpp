#include "settings_scene.h"

#include <memory>
#include <optional>
#include <utility>

#include "editor_scene.h"
#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "settings_io.h"
#include "settings/settings_layout.h"
#include "settings/settings_shell_view.h"
#include "song_select/song_select_navigation.h"
#include "localization/localization.h"
#include "theme.h"
#include "ui_draw.h"
#include "virtual_screen.h"

settings_scene::settings_scene(scene_manager& manager, return_target target)
    : scene(manager),
      return_target_(target),
      pages_(g_settings) {
}

settings_scene::settings_scene(scene_manager& manager, song_data editor_song, editor_resume_state editor_resume)
    : settings_scene(manager, return_target::editor) {
    editor_song_ = std::move(editor_song);
    editor_resume_ = std::make_shared<editor_resume_state>(std::move(editor_resume));
}

void settings_scene::on_enter() {
    pages_.reset();
}

void settings_scene::update(float dt) {
    ui::begin_hit_regions();
    pages_.tick(dt);

    if (pages_.current_page_blocks_navigation()) {
        pages_.update_current_page();
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

    if (const std::optional<settings::page_id> next_page = settings::clicked_tab_page()) {
        pages_.change_page(*next_page);
    }

    pages_.update_current_page();
}

void settings_scene::draw() {
    const auto& t = *g_theme;
    pages_.prepare_current_page();

    virtual_screen::begin_ui();
    draw_scene_background(t);
    ui::draw_panel(settings::kSidebarRect);
    ui::draw_panel(settings::kContentRect);

    ui::draw_header_block(settings::kSidebarHeaderRect, localization::tr(localization::text_key::settings),
                          localization::tr(localization::text_key::saved_on_exit));

    settings::draw_tab_buttons(pages_.current_page());

    draw_marquee_text(localization::tr(localization::text_key::settings_hint_tabs), settings::kSidebarHintRect.x, settings::kSidebarHintRect.y,
                      20, t.text_muted, settings::kSidebarHintRect.width, GetTime());
    ui::draw_button(settings::kBackRect, localization::tr(localization::text_key::back), 22);

    settings::draw_content_header(pages_.current_page());
    pages_.draw_current_page();

    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
