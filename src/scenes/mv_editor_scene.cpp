#include "mv_editor_scene.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/app_paths.h"
#include "mv/api/mv_builtins.h"
#include "mv/lang/mv_sandbox.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_layout.h"
#include "ui_text_editor.h"
#include "virtual_screen.h"

namespace {

constexpr float kHeaderHeight = 48.0f;
constexpr float kPadding = 16.0f;
constexpr float kBackButtonWidth = 100.0f;
constexpr float kBackButtonHeight = 30.0f;

} // namespace

mv_editor_scene::mv_editor_scene(scene_manager& manager, song_data song)
    : scene(manager), song_(std::move(song)) {
}

void mv_editor_scene::on_enter() {
    auto script_file = app_paths::script_path(song_.meta.song_id);
    if (std::filesystem::exists(script_file)) {
        std::ifstream ifs(script_file);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ui::text_editor_set_text(panel_state_.editor, content);
    } else {
        ui::text_editor_set_text(panel_state_.editor,
            "def draw(ctx):\n  return Scene([])\n");
    }
    panel_state_.show_compile_result = false;
    dirty_ = false;
}

void mv_editor_scene::on_exit() {
}

void mv_editor_scene::update(float dt) {
    ui::begin_hit_regions();

    if (IsKeyPressed(KEY_ESCAPE)) {
        manager_.change_scene(std::make_unique<song_select_scene>(manager_, song_.meta.song_id));
        return;
    }

    if (!panel_state_.editor.active) {
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) {
            save_script();
            return;
        }
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

    const char* title = TextFormat("MV Script - %s", song_.meta.title.c_str());
    DrawText(title, static_cast<int>(kPadding), 14, 20, g_theme->text);

    // Back button
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

    // MV script panel content area
    Rectangle content = {
        kPadding, kHeaderHeight + kPadding,
        static_cast<float>(kScreenWidth) - kPadding * 2.0f,
        static_cast<float>(kScreenHeight) - kHeaderHeight - kPadding * 2.0f
    };

    auto result = editor_mv_script_panel::draw({content, virtual_screen::get_virtual_mouse()}, panel_state_);

    if (result.compile_clicked) {
        compile_script();
    }
    if (result.save_clicked) {
        save_script();
    }

    ui::flush_draw_queue();
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void mv_editor_scene::compile_script() {
    mv::sandbox sandbox;
    mv::register_builtins_to_sandbox(sandbox);
    std::string source = ui::text_editor_get_text(panel_state_.editor);
    panel_state_.compile_success = sandbox.compile(source);
    panel_state_.errors = sandbox.last_errors();
    panel_state_.show_compile_result = true;
}

void mv_editor_scene::save_script() {
    auto path = app_paths::script_path(song_.meta.song_id);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path);
    ofs << ui::text_editor_get_text(panel_state_.editor);
    dirty_ = false;
}
