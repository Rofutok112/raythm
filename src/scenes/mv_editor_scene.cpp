#include "mv_editor_scene.h"

#include "mv/api/mv_builtins.h"
#include "mv/lang/mv_sandbox.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_select_navigation.h"
#include "theme.h"
#include "tween.h"
#include "ui_draw.h"
#include "ui_text_input.h"
#include "ui_text_editor.h"
#include "virtual_screen.h"

namespace {

constexpr float kHeaderHeight = 72.0f;
constexpr float kPadding = 24.0f;
constexpr float kBackButtonWidth = 150.0f;
constexpr float kBackButtonHeight = 45.0f;
constexpr float kMetadataButtonWidth = 228.0f;
constexpr float kMetadataModalWidth = 540.0f;
constexpr float kMetadataModalHeight = 312.0f;
constexpr float kMetadataModalOffsetY = 27.0f;
constexpr float kMetadataModalPaddingX = 27.0f;
constexpr float kMetadataHeaderTop = 27.0f;
constexpr float kMetadataTitleHeight = 39.0f;
constexpr float kMetadataSubtitleHeight = 27.0f;
constexpr float kMetadataHeaderGap = 9.0f;
constexpr float kMetadataBodyTop = 117.0f;
constexpr float kMetadataRowHeight = 54.0f;
constexpr float kMetadataRowGap = 12.0f;
constexpr float kMetadataModalScreenMargin = 18.0f;
constexpr float kMetadataModalOpenOffsetY = 27.0f;
constexpr float kMetadataInputLabelWidth = 180.0f;

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
}

std::string default_script_source() {
    return "def draw(ctx):\n  DrawBackground(fill=\"#0a0a1a\")\n";
}

Rectangle metadata_button_rect() {
    return {
        kPadding,
        (kHeaderHeight - kBackButtonHeight) * 0.5f,
        kMetadataButtonWidth,
        kBackButtonHeight
    };
}

Rectangle metadata_modal_rect(float open_anim = 1.0f) {
    Rectangle rect = {
        metadata_button_rect().x,
        metadata_button_rect().y + metadata_button_rect().height + kMetadataModalOffsetY,
        kMetadataModalWidth,
        kMetadataModalHeight
    };
    rect.x = std::clamp(rect.x, kMetadataModalScreenMargin,
                        static_cast<float>(kScreenWidth) - rect.width - kMetadataModalScreenMargin);
    rect.y = std::clamp(rect.y, kMetadataModalScreenMargin,
                        static_cast<float>(kScreenHeight) - rect.height - kMetadataModalScreenMargin);
    const float anim_t = tween::ease_out_cubic(open_anim);
    rect.y -= (1.0f - anim_t) * kMetadataModalOpenOffsetY;
    return rect;
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

    if (metadata_modal_open_) {
        metadata_modal_open_anim_ = tween::advance(metadata_modal_open_anim_, dt, 8.0f);
    } else {
        metadata_modal_open_anim_ = 0.0f;
    }

    const Rectangle modal_rect = metadata_modal_rect(metadata_modal_open_anim_);
    if (metadata_modal_open_) {
        ui::register_hit_region({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                                ui::draw_layer::overlay);
        ui::register_hit_region(modal_rect, ui::draw_layer::modal);
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (metadata_modal_open_) {
            metadata_modal_open_ = false;
            name_input_.active = false;
            author_input_.active = false;
            return;
        }
        manager_.change_scene(song_select::make_seamless_create_scene(manager_, song_.meta.song_id));
        return;
    }

    if (metadata_modal_open_ &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), modal_rect)) {
        metadata_modal_open_ = false;
        name_input_.active = false;
        author_input_.active = false;
        return;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) {
        save_mv();
        return;
    }
}

void mv_editor_scene::draw() {
    virtual_screen::begin_ui();
    draw_scene_background(*g_theme);
    ui::begin_draw_queue();

    if (metadata_modal_open_) {
        panel_state_.editor.active = false;
    }

    // Header
    Rectangle header = {0, 0, static_cast<float>(kScreenWidth), kHeaderHeight};
    ui::draw_rect_f(header, g_theme->section);
    ui::draw_rect_lines(header, 1.5f, g_theme->border_light);

    Rectangle back_btn = {
        static_cast<float>(kScreenWidth) - kBackButtonWidth - kPadding,
        (kHeaderHeight - kBackButtonHeight) * 0.5f,
        kBackButtonWidth, kBackButtonHeight
    };
    auto back_result = ui::draw_button(back_btn, "Back", 14);
    if (back_result.clicked) {
        manager_.change_scene(song_select::make_seamless_create_scene(manager_, song_.meta.song_id));
        ui::flush_draw_queue();
        virtual_screen::end();
        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    if (ui::draw_button_colored(metadata_button_rect(), "Metadata", 14,
                                metadata_modal_open_ ? g_theme->row_selected : g_theme->row,
                                metadata_modal_open_ ? g_theme->row_active : g_theme->row_hover,
                                g_theme->text).clicked) {
        metadata_modal_open_ = true;
        metadata_modal_open_anim_ = 0.0f;
        panel_state_.editor.active = false;
        name_input_.active = false;
        author_input_.active = false;
    }

    Rectangle content = {
        kPadding, kHeaderHeight + kPadding,
        static_cast<float>(kScreenWidth) - kPadding * 2.0f,
        static_cast<float>(kScreenHeight) - kHeaderHeight - kPadding * 2.0f
    };

    const auto result = mv_script_panel::draw({content, virtual_screen::get_virtual_mouse()}, panel_state_);
    if (result.text_changed) {
        dirty_ = true;
        last_change_time_ = GetTime();
        pending_compile_ = true;
    }

    if (metadata_modal_open_) {
        const float anim_t = tween::ease_out_cubic(metadata_modal_open_anim_);
        const Rectangle modal = metadata_modal_rect(metadata_modal_open_anim_);
        const Rectangle body = {
            modal.x + kMetadataModalPaddingX,
            modal.y + kMetadataBodyTop,
            modal.width - kMetadataModalPaddingX * 2.0f,
            modal.height - kMetadataBodyTop - kMetadataModalPaddingX
        };
        const Rectangle name_rect = {body.x, body.y, body.width, kMetadataRowHeight};
        const Rectangle author_rect = {body.x, body.y + kMetadataRowHeight + kMetadataRowGap,
                                       body.width, kMetadataRowHeight};
        ui::draw_rect_f({0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
                        with_alpha(g_theme->pause_overlay, static_cast<unsigned char>(180.0f + anim_t * 40.0f)));
        ui::draw_panel(modal);
        ui::draw_text_in_rect("MV Metadata", 28,
                              {modal.x + kMetadataModalPaddingX, modal.y + kMetadataHeaderTop,
                               modal.width - kMetadataModalPaddingX * 2.0f, kMetadataTitleHeight},
                              g_theme->text, ui::text_align::left);
        ui::draw_text_in_rect("Update the MV title and author.", 14,
                              {modal.x + kMetadataModalPaddingX,
                               modal.y + kMetadataHeaderTop + kMetadataTitleHeight + kMetadataHeaderGap,
                               modal.width - kMetadataModalPaddingX * 2.0f, kMetadataSubtitleHeight},
                              g_theme->text_secondary, ui::text_align::left);

        const auto name_result = ui::draw_text_input(name_rect, name_input_, "MV Name", "Untitled MV",
                                                     nullptr, ui::draw_layer::modal, 16, 128,
                                                     wide_text_filter, kMetadataInputLabelWidth);
        const auto author_result = ui::draw_text_input(author_rect, author_input_, "Author", "Author name",
                                                       nullptr, ui::draw_layer::modal, 16, 128,
                                                       wide_text_filter, kMetadataInputLabelWidth);
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
    if (mv::write_mv_json(package_.meta, package_.directory) &&
        mv::save_script(package_, ui::text_editor_get_text(panel_state_.editor))) {
        dirty_ = false;
    }
}
