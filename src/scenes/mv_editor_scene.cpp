#include "mv_editor_scene.h"

#include "core/app_paths.h"
#include "mv/api/mv_builtins.h"
#include "mv/lang/mv_sandbox.h"
#include "path_utils.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_text_input.h"
#include "ui_text_editor.h"
#include "virtual_screen.h"

namespace {

constexpr float kHeaderHeight = 48.0f;
constexpr float kPadding = 16.0f;
constexpr float kBackButtonWidth = 100.0f;
constexpr float kBackButtonHeight = 30.0f;

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
}

std::string default_script_source() {
    return "def draw(ctx):\n  return Scene([])\n";
}

Rectangle tab_button_rect(int index) {
    return {
        kPadding + static_cast<float>(index) * 164.0f,
        (kHeaderHeight - kBackButtonHeight) * 0.5f,
        152.0f,
        30.0f
    };
}

}  // namespace

mv_editor_scene::mv_editor_scene(scene_manager& manager, song_data song)
    : scene(manager), song_(std::move(song)) {
}

void mv_editor_scene::on_enter() {
    if (const auto existing = mv::find_first_package_for_song(song_.meta.song_id); existing.has_value()) {
        package_ = *existing;
    } else {
        package_ = mv::make_default_package_for_song(song_.meta);
    }

    name_input_.value = package_.meta.name;
    author_input_.value = package_.meta.author;

    const std::string content = mv::load_script(package_);
    if (!content.empty()) {
        ui::text_editor_set_text(panel_state_.editor, content);
    } else {
        ui::text_editor_set_text(panel_state_.editor, default_script_source());
    }
    dirty_ = false;
    compile_script();
}

void mv_editor_scene::on_exit() {
    if (dirty_) {
        save_mv();
    }
}

void mv_editor_scene::update(float dt) {
    (void)dt;
    ui::begin_hit_regions();

    if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_, song_.meta.song_id));
        return;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) {
        save_mv();
        return;
    }
}

void mv_editor_scene::draw() {
    virtual_screen::begin();
    ClearBackground(g_theme->bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, g_theme->bg, g_theme->bg_alt);
    ui::begin_draw_queue();

    // Header
    Rectangle header = {0, 0, static_cast<float>(kScreenWidth), kHeaderHeight};
    DrawRectangleRec(header, g_theme->section);
    DrawRectangleLinesEx(header, 1.0f, g_theme->border_light);

    Rectangle back_btn = {
        static_cast<float>(kScreenWidth) - kBackButtonWidth - kPadding,
        (kHeaderHeight - kBackButtonHeight) * 0.5f,
        kBackButtonWidth, kBackButtonHeight
    };
    auto back_result = ui::draw_button(back_btn, "Back", 14);
    if (back_result.clicked) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_, song_.meta.song_id));
        ui::flush_draw_queue();
        virtual_screen::end();
        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    if (ui::draw_button_colored(tab_button_rect(0), "Script", 14,
                                active_tab_ == tab::script ? g_theme->row_selected : g_theme->row,
                                active_tab_ == tab::script ? g_theme->row_active : g_theme->row_hover,
                                g_theme->text).clicked) {
        active_tab_ = tab::script;
    }
    if (ui::draw_button_colored(tab_button_rect(1), "Metadata", 14,
                                active_tab_ == tab::metadata ? g_theme->row_selected : g_theme->row,
                                active_tab_ == tab::metadata ? g_theme->row_active : g_theme->row_hover,
                                g_theme->text).clicked) {
        active_tab_ = tab::metadata;
    }

    Rectangle content = {
        kPadding, kHeaderHeight + kPadding,
        static_cast<float>(kScreenWidth) - kPadding * 2.0f,
        static_cast<float>(kScreenHeight) - kHeaderHeight - kPadding * 2.0f
    };

    if (active_tab_ == tab::script) {
        const auto result = mv_script_panel::draw({content, virtual_screen::get_virtual_mouse()}, panel_state_);
        if (result.text_changed) {
            dirty_ = true;
            last_change_time_ = GetTime();
            pending_compile_ = true;
        }
    } else {
        ui::draw_panel(content);

        const Rectangle body = {
            content.x + 20.0f,
            content.y + 20.0f,
            content.width - 40.0f,
            content.height - 40.0f
        };

        const Rectangle name_rect = {body.x, body.y, body.width, 42.0f};
        const Rectangle author_rect = {body.x, body.y + 56.0f, body.width, 42.0f};

        const auto name_result = ui::draw_text_input(name_rect, name_input_, "MV Name", "Untitled MV",
                                                     nullptr, ui::draw_layer::base, 16, 128,
                                                     wide_text_filter, 120.0f);
        const auto author_result = ui::draw_text_input(author_rect, author_input_, "Author", "Author name",
                                                       nullptr, ui::draw_layer::base, 16, 128,
                                                       wide_text_filter, 120.0f);
        if (name_result.changed || author_result.changed) {
            dirty_ = true;
        }
    }

    if (pending_compile_ && (GetTime() - last_change_time_) >= 0.3) {
        compile_script();
        pending_compile_ = false;
    }

    ui::flush_draw_queue();
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void mv_editor_scene::compile_script() {
    try {
        mv::sandbox sandbox;
        mv::register_builtins_to_sandbox(sandbox);
        std::string source = ui::text_editor_get_text(panel_state_.editor);
        sandbox.compile(source);
        panel_state_.errors = sandbox.last_errors();

        // Populate inline error markers for squiggly underlines
        panel_state_.editor.error_markers.clear();
        for (const auto& err : panel_state_.errors) {
            if (err.line > 0) {
                panel_state_.editor.error_markers.push_back({
                    err.line, std::max(err.column - 1, 0), std::max(err.column, 0), err.message
                });
            }
        }
    } catch (const std::exception& e) {
        panel_state_.errors = {{"internal", e.what(), 0, 0}};
        panel_state_.editor.error_markers.clear();
    } catch (...) {
        panel_state_.errors = {{"internal", "unexpected error during compile", 0, 0}};
        panel_state_.editor.error_markers.clear();
    }
}

void mv_editor_scene::save_mv() {
    package_.meta.song_id = song_.meta.song_id;
    package_.meta.name = name_input_.value.empty() ? (song_.meta.title + " MV") : name_input_.value;
    package_.meta.author = author_input_.value;
    package_.meta.script_file = "script.rmv";
    package_.directory = path_utils::to_utf8(app_paths::mv_dir(package_.meta.mv_id));
    if (mv::write_mv_json(package_.meta, package_.directory) &&
        mv::save_script(package_, ui::text_editor_get_text(panel_state_.editor))) {
        dirty_ = false;
    }
}
