#include "mv_editor_scene.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>

#include "audio_manager.h"
#include "managed_content_storage.h"
#include "mv/composition/mv_composition_evaluator.h"
#include "mv/composition/mv_component_registry.h"
#include "mv/composition/mv_lua_runtime.h"
#include "mv/composition/mv_composition_serializer.h"
#include "file_dialog.h"
#include "path_utils.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_select_navigation.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "ui_frame.h"
#include "ui_hit.h"
#include "ui_inspector.h"
#include "ui_layout.h"
#include "ui_scroll.h"
#include "ui_text.h"
#include "ui_text_input.h"
#include "virtual_screen.h"

namespace {

constexpr float kHeaderHeight = 58.0f;
constexpr float kPadding = 16.0f;
constexpr float kPanelGap = 10.0f;
constexpr float kBackButtonWidth = 112.0f;
constexpr float kHeaderButtonHeight = 34.0f;
constexpr float kHeaderButtonWidth = 108.0f;
constexpr float kHistoryButtonWidth = 118.0f;
constexpr float kMetadataButtonWidth = 170.0f;
constexpr float kProjectPanelWidth = 300.0f;
constexpr float kInspectorWidth = 350.0f;
constexpr float kTimelineHeight = 314.0f;
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
constexpr double kKeyframeHitToleranceMs = 24.0;
constexpr double kDefaultFallbackDurationMs = 8000.0;
constexpr float kInspectorWheelStep = 48.0f;

struct mv_editor_header_layout {
    Rectangle header = {};
    Rectangle metadata_button = {};
    Rectangle title = {};
    Rectangle subtitle = {};
    Rectangle play = {};
    Rectangle undo = {};
    Rectangle redo = {};
    Rectangle save = {};
    Rectangle back = {};
};

struct mv_editor_frame_layout {
    Rectangle content = {};
    Rectangle work_area = {};
    Rectangle bottom_panel = {};
    Rectangle bottom_tab_bar = {};
    Rectangle bottom_content_panel = {};
    Rectangle timeline_panel = {};
    Rectangle project_panel = {};
    Rectangle layers_panel = {};
    Rectangle preview_panel = {};
    Rectangle inspector_panel = {};
};

struct scroll_panel_layout {
    Rectangle viewport = {};
    Rectangle scrollbar = {};
};

struct bottom_tabs_layout {
    Rectangle timeline = {};
    Rectangle project = {};
};

struct metadata_modal_layout {
    Rectangle backdrop = {};
    Rectangle modal = {};
    Rectangle title = {};
    Rectangle subtitle = {};
    Rectangle body = {};
    Rectangle name = {};
    Rectangle author = {};
};

metadata_modal_layout metadata_modal_layout_for(float open_anim);

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
}

std::string format_float_input(float value, int decimals) {
    char buffer[64];
    if (decimals <= 0) {
        std::snprintf(buffer, sizeof(buffer), "%.0f", value);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    }
    return buffer;
}

Rectangle metadata_button_rect() {
    return {
        kPadding,
        (kHeaderHeight - kHeaderButtonHeight) * 0.5f,
        kMetadataButtonWidth,
        kHeaderButtonHeight
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

mv_editor_header_layout mv_editor_header_layout_for() {
    mv_editor_header_layout layout;
    layout.header = {0.0f, 0.0f, static_cast<float>(kScreenWidth), kHeaderHeight};
    layout.metadata_button = metadata_button_rect();

    const float header_right = static_cast<float>(kScreenWidth) - kPadding;
    layout.back = {
        header_right - kBackButtonWidth,
        (kHeaderHeight - kHeaderButtonHeight) * 0.5f,
        kBackButtonWidth,
        kHeaderButtonHeight
    };
    layout.save = {
        layout.back.x - kHeaderButtonWidth - 12.0f,
        layout.back.y,
        kHeaderButtonWidth,
        kHeaderButtonHeight
    };
    layout.redo = {
        layout.save.x - kHistoryButtonWidth - 12.0f,
        layout.back.y,
        kHistoryButtonWidth,
        kHeaderButtonHeight
    };
    layout.undo = {
        layout.redo.x - kHistoryButtonWidth - 8.0f,
        layout.back.y,
        kHistoryButtonWidth,
        kHeaderButtonHeight
    };
    layout.play = {
        layout.undo.x - kHeaderButtonWidth - 12.0f,
        layout.back.y,
        kHeaderButtonWidth,
        kHeaderButtonHeight
    };
    layout.title = {
        layout.metadata_button.x + layout.metadata_button.width + 18.0f,
        8.0f,
        layout.play.x - layout.metadata_button.x - layout.metadata_button.width - 36.0f,
        24.0f
    };
    layout.subtitle = {
        layout.title.x,
        32.0f,
        layout.title.width,
        18.0f
    };
    return layout;
}

mv_editor_header_action draw_mv_editor_header(const mv_editor_header_layout& layout,
                                              bool metadata_modal_open,
                                              bool preview_playing,
                                              bool can_undo,
                                              bool can_redo,
                                              bool dirty,
                                              const std::string& title,
                                              const std::string& subtitle) {
    ui::surface(layout.header, g_theme->section, g_theme->border_light, 1.5f);
    mv_editor_header_action action = mv_editor_header_action::none;

    if (ui::button(layout.metadata_button, "Metadata", {
            .font_size = 12,
            .bg = metadata_modal_open ? g_theme->row_selected : g_theme->row,
            .bg_hover = metadata_modal_open ? g_theme->row_active : g_theme->row_hover,
            .text_color = g_theme->text,
            .custom_colors = true,
        }).clicked) {
        action = mv_editor_header_action::open_metadata;
    }
    if (ui::button(layout.play, preview_playing ? "Pause" : "Play", {.font_size = 12}).clicked) {
        action = mv_editor_header_action::toggle_preview;
    }
    if (ui::button(layout.undo, "Undo", {
            .font_size = 11,
            .border_width = 1.5f,
            .bg = can_undo ? g_theme->row : with_alpha(g_theme->row, 110),
            .bg_hover = g_theme->row_hover,
            .text_color = g_theme->text,
            .custom_colors = true,
        }).clicked) {
        action = mv_editor_header_action::undo;
    }
    if (ui::button(layout.redo, "Redo", {
            .font_size = 11,
            .border_width = 1.5f,
            .bg = can_redo ? g_theme->row : with_alpha(g_theme->row, 110),
            .bg_hover = g_theme->row_hover,
            .text_color = g_theme->text,
            .custom_colors = true,
        }).clicked) {
        action = mv_editor_header_action::redo;
    }
    if (ui::button(layout.save, dirty ? "Save *" : "Saved", {.font_size = 12}).clicked) {
        action = mv_editor_header_action::save;
    }
    if (ui::button(layout.back, "Back", {.font_size = 12}).clicked) {
        action = mv_editor_header_action::back;
    }

    ui::draw_text_in_rect(title.c_str(), 18, layout.title, g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect(subtitle.c_str(), 11, layout.subtitle, g_theme->text_muted, ui::text_align::left);
    return action;
}

mv_editor_bottom_tab_action draw_mv_editor_bottom_tabs(const bottom_tabs_layout& layout,
                                                       bool timeline_selected) {
    mv_editor_bottom_tab_action action = mv_editor_bottom_tab_action::none;
    if (ui::button(layout.timeline, "Timeline", {
            .font_size = 11,
            .border_width = 1.5f,
            .bg = timeline_selected
                ? g_theme->row_selected
                : with_alpha(g_theme->row, 130),
            .bg_hover = g_theme->row_hover,
            .text_color = g_theme->text,
            .custom_colors = true,
        }).clicked) {
        action = mv_editor_bottom_tab_action::show_timeline;
    }
    if (ui::button(layout.project, "Project", {
            .font_size = 11,
            .border_width = 1.5f,
            .bg = !timeline_selected
                ? g_theme->row_selected
                : with_alpha(g_theme->row, 130),
            .bg_hover = g_theme->row_hover,
            .text_color = g_theme->text,
            .custom_colors = true,
        }).clicked) {
        action = mv_editor_bottom_tab_action::show_project;
    }
    return action;
}

mv_editor_metadata_modal_result draw_mv_metadata_modal(float open_anim,
                                                       ui::text_input_state& name_input,
                                                       ui::text_input_state& author_input) {
    const float anim_t = tween::ease_out_cubic(open_anim);
    const metadata_modal_layout modal_layout = metadata_modal_layout_for(open_anim);
    ui::backdrop(modal_layout.backdrop,
                 with_alpha(g_theme->pause_overlay, static_cast<unsigned char>(180.0f + anim_t * 40.0f)));
    ui::panel(modal_layout.modal);
    ui::draw_text_in_rect("MV Metadata", 28, modal_layout.title, g_theme->text, ui::text_align::left);
    ui::draw_text_in_rect("Update the MV title and author.", 14,
                          modal_layout.subtitle,
                          g_theme->text_secondary, ui::text_align::left);

    const auto name_result = ui::text_input(modal_layout.name, name_input, "MV Name", "Untitled MV", {
        .layer = ui::draw_layer::modal,
        .font_size = 16,
        .max_length = 128,
        .filter = wide_text_filter,
        .label_width = kMetadataInputLabelWidth,
    });
    const auto author_result = ui::text_input(modal_layout.author, author_input, "Author", "Author name", {
        .layer = ui::draw_layer::modal,
        .font_size = 16,
        .max_length = 128,
        .filter = wide_text_filter,
        .label_width = kMetadataInputLabelWidth,
    });
    return {
        .metadata_changed = name_result.changed || author_result.changed,
    };
}

mv_editor_frame_layout mv_editor_frame_layout_for() {
    mv_editor_frame_layout layout;
    layout.content = {
        kPadding,
        kHeaderHeight + kPanelGap,
        static_cast<float>(kScreenWidth) - kPadding * 2.0f,
        static_cast<float>(kScreenHeight) - kHeaderHeight - kPanelGap - kPadding
    };
    const ui::rect_pair main_rows =
        ui::split_rows(layout.content, layout.content.height - kTimelineHeight - kPanelGap, kPanelGap);
    layout.work_area = main_rows.first;
    layout.bottom_panel = main_rows.second;
    layout.bottom_tab_bar = {layout.bottom_panel.x, layout.bottom_panel.y, layout.bottom_panel.width, 38.0f};
    layout.bottom_content_panel = {
        layout.bottom_panel.x,
        layout.bottom_panel.y + 42.0f,
        layout.bottom_panel.width,
        layout.bottom_panel.height - 42.0f
    };
    layout.timeline_panel = layout.bottom_content_panel;
    layout.project_panel = layout.bottom_content_panel;

    const ui::rect_pair left_split = ui::split_columns(layout.work_area, kProjectPanelWidth, kPanelGap);
    const ui::rect_pair right_split = ui::split_columns(left_split.second,
                                                       left_split.second.width - kInspectorWidth - kPanelGap,
                                                       kPanelGap);
    layout.layers_panel = left_split.first;
    layout.preview_panel = right_split.first;
    layout.inspector_panel = right_split.second;
    return layout;
}

scroll_panel_layout panel_scroll_layout_for(Rectangle panel) {
    const Rectangle viewport = {
        panel.x + 18.0f,
        panel.y + 58.0f,
        panel.width - 50.0f,
        panel.height - 76.0f
    };
    return {
        .viewport = viewport,
        .scrollbar = {viewport.x + viewport.width + 8.0f, viewport.y, 8.0f, viewport.height}
    };
}

Rectangle preview_outer_rect_for(Rectangle panel) {
    return {
        panel.x + 18.0f,
        panel.y + 58.0f,
        panel.width - 36.0f,
        panel.height - 76.0f
    };
}

Rectangle preview_canvas_rect_for(Rectangle outer, int canvas_width, int canvas_height) {
    const float canvas_aspect = static_cast<float>(std::max(1, canvas_width)) /
                                static_cast<float>(std::max(1, canvas_height));
    Rectangle preview = outer;
    if (preview.width / preview.height > canvas_aspect) {
        preview.width = preview.height * canvas_aspect;
        preview.x = outer.x + (outer.width - preview.width) * 0.5f;
    } else {
        preview.height = preview.width / canvas_aspect;
        preview.y = outer.y + (outer.height - preview.height) * 0.5f;
    }
    return preview;
}

bottom_tabs_layout bottom_tabs_layout_for(Rectangle tab_bar) {
    const Rectangle timeline = {tab_bar.x, tab_bar.y, 128.0f, 32.0f};
    return {
        .timeline = timeline,
        .project = {timeline.x + timeline.width + 8.0f, timeline.y, 128.0f, timeline.height}
    };
}

Rectangle diagnostics_rect_for(Rectangle inspector_panel) {
    return {
        inspector_panel.x + 18.0f,
        inspector_panel.y + inspector_panel.height - 58.0f,
        inspector_panel.width - 36.0f,
        40.0f
    };
}

metadata_modal_layout metadata_modal_layout_for(float open_anim) {
    const Rectangle modal = metadata_modal_rect(open_anim);
    const Rectangle body = {
        modal.x + kMetadataModalPaddingX,
        modal.y + kMetadataBodyTop,
        modal.width - kMetadataModalPaddingX * 2.0f,
        modal.height - kMetadataBodyTop - kMetadataModalPaddingX
    };
    return {
        .backdrop = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)},
        .modal = modal,
        .title = {modal.x + kMetadataModalPaddingX, modal.y + kMetadataHeaderTop,
                  modal.width - kMetadataModalPaddingX * 2.0f, kMetadataTitleHeight},
        .subtitle = {modal.x + kMetadataModalPaddingX,
                     modal.y + kMetadataHeaderTop + kMetadataTitleHeight + kMetadataHeaderGap,
                     modal.width - kMetadataModalPaddingX * 2.0f, kMetadataSubtitleHeight},
        .body = body,
        .name = {body.x, body.y, body.width, kMetadataRowHeight},
        .author = {body.x, body.y + kMetadataRowHeight + kMetadataRowGap, body.width, kMetadataRowHeight}
    };
}

std::string ms_label(double value_ms) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.2fs", value_ms / 1000.0);
    return buffer;
}

std::string layer_type_label(const mv::composition::layer& layer) {
    const mv::composition::component* renderer = mv::composition::renderable_component(layer);
    if (renderer == nullptr) {
        return "Empty";
    }
    if (renderer->type == "ShapeRenderer" && !renderer->shape.empty()) {
        return renderer->type + "/" + renderer->shape;
    }
    return renderer->type.empty() ? "unknown" : renderer->type;
}

std::string next_layer_id(const mv::composition::mv_composition& composition, const std::string& prefix) {
    for (int index = static_cast<int>(composition.objects.size()) + 1; index < 10000; ++index) {
        const std::string id = prefix + "-" + std::to_string(index);
        const auto it = std::find_if(composition.objects.begin(), composition.objects.end(), [&](const auto& layer) {
            return layer.id == id;
        });
        if (it == composition.objects.end()) {
            return id;
        }
    }
    return prefix + "-fallback";
}

std::string next_effect_id(const mv::composition::mv_composition& composition, const std::string& prefix) {
    for (int index = 1; index < 10000; ++index) {
        const std::string id = prefix + "-" + std::to_string(index);
        bool exists = false;
        for (const mv::composition::layer& layer : composition.objects) {
            for (const mv::composition::component* effect : mv::composition::effect_components(layer)) {
                exists = effect->id == id;
                if (exists) {
                    break;
                }
            }
            if (exists) {
                break;
            }
        }
        if (!exists) {
            return id;
        }
    }
    return prefix + "-fallback";
}

std::string next_component_id(const mv::composition::mv_composition& composition, const std::string& prefix) {
    for (int index = 1; index < 10000; ++index) {
        const std::string id = prefix + "-" + std::to_string(index);
        bool exists = false;
        for (const mv::composition::layer& layer : composition.objects) {
            exists = std::any_of(layer.components.begin(), layer.components.end(), [&](const auto& component) {
                return component.id == id;
            });
            if (exists) {
                break;
            }
        }
        if (!exists) {
            return id;
        }
    }
    return prefix + "-fallback";
}

mv::composition::component* find_effect_component(mv::composition::layer& layer, const std::string& type) {
    for (mv::composition::component& component : layer.components) {
        if ((component.type == "Fade" || component.type == "Pulse" ||
             component.type == "Flash" || component.type == "Shake" ||
             component.type == "BeatReactive") && component.type == type) {
            return &component;
        }
    }
    return nullptr;
}

const mv::composition::component* renderer_or_null(const mv::composition::layer& layer) {
    return mv::composition::renderable_component(layer);
}

mv::composition::component* renderer_or_null(mv::composition::layer& layer) {
    return mv::composition::renderable_component(layer);
}

mv::composition::component* ensure_transform(mv::composition::layer& layer) {
    if (mv::composition::component* transform = mv::composition::transform_component(layer)) {
        return transform;
    }
    layer.components.insert(layer.components.begin(), mv::composition::make_transform_component());
    return mv::composition::transform_component(layer);
}

void apply_evaluated_transform(mv::composition::layer& layer, const mv::composition::transform& transform) {
    if (mv::composition::component* component = ensure_transform(layer)) {
        mv::composition::apply_transform_to_component(*component, transform);
    }
    layer.transform_data = transform;
}

void evaluate_preview_behaviours(mv::composition::layer& layer, double playhead_ms) {
    const mv::composition::lua_update_result result =
        mv::composition::apply_lua_behaviours(layer, {.song_time_ms = playhead_ms, .delta_ms = 0.0});
    (void)result;
}

mv::composition::component make_effect_component(std::string id,
                                                 std::string type,
                                                 std::string target,
                                                 float amount) {
    mv::composition::component effect;
    effect.id = std::move(id);
    effect.type = std::move(type);
    effect.target = std::move(target);
    effect.amount = amount;
    return effect;
}

const char* image_extension_for_asset(const mv::composition::asset_ref& asset) {
    std::string extension = std::filesystem::path(asset.path).extension().generic_string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (extension == ".jpg" || extension == ".jpeg") {
        return ".jpg";
    }
    return ".png";
}

Texture2D load_texture_from_asset_bytes(const mv::mv_package& package,
                                        const mv::composition::asset_ref& asset,
                                        std::vector<std::string>* errors) {
    const std::optional<std::vector<unsigned char>> bytes = mv::read_asset_bytes(package, asset, errors);
    if (!bytes.has_value() || bytes->empty()) {
        return {};
    }
    Image image = LoadImageFromMemory(image_extension_for_asset(asset),
                                      bytes->data(),
                                      static_cast<int>(bytes->size()));
    if (image.data == nullptr) {
        if (errors != nullptr) {
            errors->push_back("Failed to decode MV image asset.");
        }
        return {};
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

std::size_t layer_index_by_id(const mv::composition::mv_composition& composition,
                              const std::string& layer_id) {
    const auto it = std::find_if(composition.objects.begin(), composition.objects.end(), [&](const auto& layer) {
        return layer.id == layer_id;
    });
    return it == composition.objects.end()
        ? static_cast<std::size_t>(-1)
        : static_cast<std::size_t>(std::distance(composition.objects.begin(), it));
}

bool layer_active_at(const mv::composition::layer& layer, double time_ms) {
    if (!layer.visible || time_ms < layer.start_ms) {
        return false;
    }
    return layer.duration_ms <= 0.0 || time_ms <= layer.start_ms + layer.duration_ms;
}

double song_duration_ms_for(const song_data& song) {
    return song.meta.duration_seconds > 0.0f
        ? static_cast<double>(song.meta.duration_seconds) * 1000.0
        : 0.0;
}

bool repair_missing_composition_duration(mv::composition::mv_composition& composition,
                                         double song_duration_ms) {
    if (song_duration_ms <= 0.0 || composition.duration_ms > 0.0) {
        return false;
    }
    composition.duration_ms = song_duration_ms;
    for (mv::composition::layer& layer : composition.objects) {
        if (layer.start_ms == 0.0 &&
            (layer.duration_ms <= 0.0 ||
             layer.id == "layer-background" ||
             (layer.id == "layer-title" && layer.duration_ms == kDefaultFallbackDurationMs))) {
            layer.duration_ms = song_duration_ms;
        }
    }
    return true;
}

void draw_section_title(Rectangle rect, const char* title, const char* subtitle = nullptr) {
    ui::draw_text_in_rect(title, 16, {rect.x + 18.0f, rect.y, rect.width - 36.0f, 30.0f},
                          g_theme->text, ui::text_align::left);
    if (subtitle != nullptr) {
        ui::draw_text_in_rect(subtitle, 12, {rect.x + 18.0f, rect.y + 26.0f, rect.width - 36.0f, 24.0f},
                              g_theme->text_muted, ui::text_align::left);
    }
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
    package_.song_duration_ms = song_duration_ms_for(song_);

    name_input_.value = package_.meta.name;
    author_input_.value = package_.meta.author;

    bool repaired_duration = false;
    std::vector<std::string> load_errors;
    if (const auto loaded = mv::load_composition(package_, &load_errors); loaded.has_value()) {
        composition_ = *loaded;
        repaired_duration = repair_missing_composition_duration(composition_, package_.song_duration_ms);
    } else {
        composition_ = mv::make_default_composition_for_song(package_);
        diagnostics_ = load_errors;
    }
    if (!composition_.objects.empty()) {
        selected_layer_id_ = composition_.objects.back().id;
    }
    normalize_layer_z_order();
    playhead_ms_ = 0.0;
    preview_audio_loaded_ = load_preview_audio();
    history_.reset(composition_, selected_layer_id_);
    metadata_dirty_ = false;
    dirty_ = repaired_duration;
    validate_composition();
}

void mv_editor_scene::on_exit() {
    if (dirty_) {
        save_mv();
    }
    stop_preview_audio();
    unload_asset_textures();
}

void mv_editor_scene::update(float dt) {
    ui::begin_input_frame();

    if (metadata_modal_open_) {
        metadata_modal_open_anim_ = tween::advance(metadata_modal_open_anim_, dt, 8.0f);
    } else {
        metadata_modal_open_anim_ = 0.0f;
    }

    if (preview_playing_) {
        if (preview_audio_loaded_) {
            if (!audio_manager::instance().is_preview_playing()) {
                preview_playing_ = false;
            }
            playhead_ms_ = audio_manager::instance().get_preview_position_seconds() * 1000.0;
        } else {
            playhead_ms_ += static_cast<double>(dt) * 1000.0;
        }
        if (playhead_ms_ > composition_duration_ms()) {
            playhead_ms_ = 0.0;
            if (preview_audio_loaded_) {
                audio_manager::instance().seek_preview(0.0);
                audio_manager::instance().play_preview(false);
            }
        }
    }

    const metadata_modal_layout modal_layout = metadata_modal_layout_for(metadata_modal_open_anim_);
    if (metadata_modal_open_) {
        ui::register_hit_region(modal_layout.backdrop, ui::draw_layer::overlay);
        ui::register_hit_region(modal_layout.modal, ui::draw_layer::modal);
    }

    if (ui::is_escape_pressed()) {
        if (metadata_modal_open_) {
            close_metadata_modal();
            return;
        }
        manager_.change_scene(song_select::make_seamless_create_scene(manager_, song_.meta.song_id));
        return;
    }

    if (metadata_modal_open_ &&
        ui::is_mouse_button_pressed_outside(modal_layout.modal, virtual_screen::get_virtual_mouse())) {
        close_metadata_modal();
        return;
    }

    if (!inspector_text_input_active() && !name_input_.active && !author_input_.active) {
        const bool ctrl_down = ui::is_key_down(KEY_LEFT_CONTROL) || ui::is_key_down(KEY_RIGHT_CONTROL);
        const bool shift_down = ui::is_shift_down();
        if (ctrl_down && ui::is_key_pressed(KEY_S)) {
            save_mv();
        } else if (ctrl_down && !shift_down && ui::is_key_pressed(KEY_Z)) {
            undo_edit();
        } else if (ctrl_down && (ui::is_key_pressed(KEY_Y) || (shift_down && ui::is_key_pressed(KEY_Z)))) {
            redo_edit();
        }
    }
}

bool mv_editor_scene::apply_header_action(mv_editor_header_action action) {
    switch (action) {
    case mv_editor_header_action::none:
        break;
    case mv_editor_header_action::open_metadata:
        metadata_modal_open_ = true;
        metadata_modal_open_anim_ = 0.0f;
        name_input_.active = false;
        author_input_.active = false;
        break;
    case mv_editor_header_action::toggle_preview:
        set_preview_playing(!preview_playing_);
        break;
    case mv_editor_header_action::undo:
        undo_edit();
        break;
    case mv_editor_header_action::redo:
        redo_edit();
        break;
    case mv_editor_header_action::save:
        save_mv();
        break;
    case mv_editor_header_action::back:
        manager_.change_scene(song_select::make_seamless_create_scene(manager_, song_.meta.song_id));
        return true;
    }
    return false;
}

void mv_editor_scene::apply_bottom_tab_action(mv_editor_bottom_tab_action action) {
    switch (action) {
    case mv_editor_bottom_tab_action::none:
        break;
    case mv_editor_bottom_tab_action::show_timeline:
        bottom_panel_tab_ = bottom_panel_tab::timeline;
        break;
    case mv_editor_bottom_tab_action::show_project:
        bottom_panel_tab_ = bottom_panel_tab::project;
        break;
    }
}

void mv_editor_scene::apply_metadata_modal_result(const mv_editor_metadata_modal_result& result) {
    if (result.metadata_changed) {
        metadata_dirty_ = true;
        dirty_ = true;
    }
}

void mv_editor_scene::apply_hierarchy_result(const mv_editor_hierarchy_result& result) {
    hierarchy_scroll_offset_ = result.scroll_offset;
    hierarchy_scrollbar_dragging_ = result.scrollbar_dragging;
    hierarchy_scrollbar_drag_offset_ = result.scrollbar_drag_offset;

    if (!result.selected_layer_id.empty()) {
        const auto it = std::find_if(composition_.objects.begin(), composition_.objects.end(), [&](const auto& layer) {
            return layer.id == result.selected_layer_id;
        });
        if (it != composition_.objects.end()) {
            selected_layer_id_ = it->id;
            sync_inspector_inputs(*it);
        }
    }
    if (result.move_direction != 0) {
        move_selected_layer(result.move_direction);
    }
}

void mv_editor_scene::apply_context_menu_result(const mv_editor_context_menu_result& result) {
    switch (result.action) {
    case mv_editor_context_menu_action::none:
        return;
    case mv_editor_context_menu_action::close:
        break;
    case mv_editor_context_menu_action::add_empty_layer:
        add_empty_layer();
        break;
    case mv_editor_context_menu_action::add_text_layer:
        add_text_layer();
        break;
    case mv_editor_context_menu_action::add_rect_layer:
        add_rect_layer();
        break;
    case mv_editor_context_menu_action::add_image_layer:
        add_image_layer();
        break;
    case mv_editor_context_menu_action::add_beat_grid_layer:
        add_beat_grid_layer();
        break;
    case mv_editor_context_menu_action::add_waveform_layer:
        add_waveform_layer();
        break;
    case mv_editor_context_menu_action::add_spectrum_layer:
        add_spectrum_layer();
        break;
    case mv_editor_context_menu_action::import_image_asset:
        import_image_asset_to_project();
        break;
    case mv_editor_context_menu_action::create_script_asset:
        create_script_asset();
        break;
    case mv_editor_context_menu_action::add_component:
        if (!result.component_type.empty()) {
            add_component_to_selected_layer(result.component_type);
        }
        break;
    case mv_editor_context_menu_action::clear_effects:
        clear_selected_layer_effects();
        break;
    case mv_editor_context_menu_action::delete_layer:
        delete_selected_layer();
        break;
    }
    context_menu_open_ = false;
}

void mv_editor_scene::apply_project_panel_result(const mv_editor_project_panel_result& result) {
    project_scroll_offset_ = result.scroll_offset;
    project_scrollbar_dragging_ = result.scrollbar_dragging;
    project_scrollbar_drag_offset_ = result.scrollbar_drag_offset;

    if (!result.selected_asset_id.empty()) {
        const auto selected = std::find_if(composition_.assets.begin(), composition_.assets.end(), [&](const auto& asset) {
            return asset.id == result.selected_asset_id;
        });
        if (selected != composition_.assets.end()) {
            selected_project_asset_id_ = selected->id;
        }
    }

    if (!result.assign_asset_id.empty()) {
        const auto assigned = std::find_if(composition_.assets.begin(), composition_.assets.end(), [&](const auto& asset) {
            return asset.id == result.assign_asset_id;
        });
        if (assigned != composition_.assets.end() && assign_asset_to_selected_component(*assigned)) {
            commit_history(assigned->type == "script" ? "Assign Script Asset" : "Assign Image Asset");
        }
    }
}

void mv_editor_scene::apply_timeline_layer_row_result(const mv_editor_timeline_layer_row_result& result) {
    if (result.action == mv_editor_timeline_layer_row_action::none || result.layer_id.empty()) {
        return;
    }

    const auto layer_it = std::find_if(composition_.objects.begin(), composition_.objects.end(), [&](const auto& layer) {
        return layer.id == result.layer_id;
    });
    if (layer_it == composition_.objects.end()) {
        return;
    }

    switch (result.action) {
    case mv_editor_timeline_layer_row_action::none:
        break;
    case mv_editor_timeline_layer_row_action::select:
        selected_layer_id_ = layer_it->id;
        sync_inspector_inputs(*layer_it);
        break;
    case mv_editor_timeline_layer_row_action::toggle_visibility:
        selected_layer_id_ = layer_it->id;
        layer_it->visible = !layer_it->visible;
        validate_composition();
        commit_history("Toggle Visibility");
        break;
    case mv_editor_timeline_layer_row_action::toggle_lock:
        selected_layer_id_ = layer_it->id;
        layer_it->locked = !layer_it->locked;
        validate_composition();
        commit_history("Toggle Lock");
        break;
    case mv_editor_timeline_layer_row_action::delete_layer:
        selected_layer_id_ = layer_it->id;
        delete_selected_layer();
        break;
    }
}

void mv_editor_scene::apply_timeline_scrub_result(const mv_editor_timeline_scrub_result& result) {
    if (!result.requested) {
        return;
    }
    playhead_ms_ = result.playhead_ms;
    set_preview_playing(false);
    seek_preview_audio_to_playhead();
}

void mv_editor_scene::apply_timeline_drag_start_result(const mv_editor_timeline_drag_start_result& result) {
    if (!result.started || result.layer_id.empty()) {
        return;
    }

    timeline_drag_mode next_mode = timeline_drag_mode::none;
    switch (result.mode) {
    case mv_editor_timeline_drag_update_mode::none:
        return;
    case mv_editor_timeline_drag_update_mode::move:
        next_mode = timeline_drag_mode::move;
        break;
    case mv_editor_timeline_drag_update_mode::trim_start:
        next_mode = timeline_drag_mode::trim_start;
        break;
    case mv_editor_timeline_drag_update_mode::trim_end:
        next_mode = timeline_drag_mode::trim_end;
        break;
    }

    selected_layer_id_ = result.layer_id;
    timeline_drag_mode_ = next_mode;
    timeline_drag_layer_id_ = result.layer_id;
    timeline_drag_origin_mouse_x_ = result.origin_mouse_x;
    timeline_drag_origin_start_ms_ = result.origin_start_ms;
    timeline_drag_origin_duration_ms_ = result.origin_duration_ms;
    set_preview_playing(false);
}

void mv_editor_scene::apply_timeline_drag_update_result(const mv_editor_timeline_drag_update_result& result) {
    if (!result.active || result.layer_id.empty()) {
        return;
    }

    auto layer_it = std::find_if(composition_.objects.begin(), composition_.objects.end(), [&](const auto& layer) {
        return layer.id == result.layer_id;
    });
    if (layer_it == composition_.objects.end()) {
        return;
    }

    layer_it->start_ms = result.start_ms;
    layer_it->duration_ms = result.duration_ms;
    playhead_ms_ = result.playhead_ms;
    dirty_ = true;
}

void mv_editor_scene::apply_timeline_drag_end_result(const mv_editor_timeline_drag_end_result& result) {
    if (!result.ended) {
        return;
    }

    commit_history("Edit Timing");
    timeline_drag_mode_ = timeline_drag_mode::none;
    timeline_drag_layer_id_.clear();
    seek_preview_audio_to_playhead();
    validate_composition();
}

void mv_editor_scene::apply_preview_drag_start_result(const mv_editor_preview_drag_start_result& result) {
    if (!result.selected_layer_id.empty()) {
        const auto layer_it = std::find_if(composition_.objects.begin(), composition_.objects.end(), [&](const auto& layer) {
            return layer.id == result.selected_layer_id;
        });
        if (layer_it != composition_.objects.end()) {
            selected_layer_id_ = layer_it->id;
            if (result.sync_inspector) {
                sync_inspector_inputs(*layer_it);
            }
        }
    }

    if (!result.drag_started ||
        result.drag_layer_id.empty() ||
        result.mode == mv_preview_drag_mode::none) {
        return;
    }

    preview_drag_mode_ = result.mode;
    preview_drag_layer_id_ = result.drag_layer_id;
    preview_drag_origin_mouse_ = result.origin_mouse;
    preview_drag_origin_rect_ = result.origin_rect;
    preview_drag_origin_transform_ = result.origin_transform;
    set_preview_playing(false);
}

void mv_editor_scene::apply_preview_drag_update_result(const mv_editor_preview_drag_update_result& result,
                                                       Rectangle preview) {
    if (!result.active || result.layer_id.empty()) {
        return;
    }

    auto layer_it = std::find_if(composition_.objects.begin(), composition_.objects.end(), [&](const auto& layer) {
        return layer.id == result.layer_id;
    });
    if (layer_it == composition_.objects.end() || layer_it->locked) {
        return;
    }

    const Texture2D* texture = nullptr;
    const mv::composition::component* renderer = renderer_or_null(*layer_it);
    if (renderer != nullptr && renderer->type == "ImageRenderer") {
        if (const mv::composition::asset_ref* asset = find_asset(renderer->asset_id); asset != nullptr) {
            texture = texture_for_asset(*asset);
        }
    }

    mv::composition::transform next_transform = preview_drag_origin_transform_;
    apply_preview_rect_to_transform(preview, composition_, *layer_it, texture, result.next_bounds, next_transform);
    if (mv::composition::component* transform = ensure_transform(*layer_it)) {
        mv::composition::apply_transform_to_component(*transform, next_transform);
    }
    dirty_ = true;
}

void mv_editor_scene::apply_preview_drag_end_result(const mv_editor_preview_drag_end_result& result) {
    if (result.cancelled) {
        preview_drag_mode_ = mv_preview_drag_mode::none;
        preview_drag_layer_id_.clear();
        return;
    }
    if (!result.ended) {
        return;
    }

    commit_history("Edit Transform");
    validate_composition();
    preview_drag_mode_ = mv_preview_drag_mode::none;
    preview_drag_layer_id_.clear();
}

bool mv_editor_scene::apply_inspector_layer_name_result(const mv_editor_inspector_layer_name_result& result,
                                                        mv::composition::layer& layer) {
    bool changed = false;
    if (result.changed) {
        layer.name = result.name.empty() ? "Object" : result.name;
        dirty_ = true;
        inspector_edit_pending_ = true;
        changed = true;
    }
    if (result.commit_requested && inspector_edit_pending_) {
        commit_history("Edit Object Name");
    }
    return changed;
}

bool mv_editor_scene::apply_inspector_transform_input_result(
    const mv_editor_inspector_transform_input_result& result,
    mv::composition::component& transform) {
    if (result.field == mv_editor_inspector_transform_field::none) {
        return false;
    }

    auto apply_value = [&](float value) {
        switch (result.field) {
        case mv_editor_inspector_transform_field::none:
            break;
        case mv_editor_inspector_transform_field::position_x:
            transform.position_x = value;
            break;
        case mv_editor_inspector_transform_field::position_y:
            transform.position_y = value;
            break;
        case mv_editor_inspector_transform_field::scale:
            transform.scale_x = std::clamp(value, 0.05f, 8.0f);
            transform.scale_y = transform.scale_x;
            break;
        }
    };

    bool changed = false;
    if (result.changed) {
        apply_value(result.value);
        dirty_ = true;
        inspector_edit_pending_ = true;
        changed = true;
    }
    if (result.finalized) {
        apply_value(result.value);
        const std::string formatted = format_float_input(result.value, result.decimals);
        switch (result.field) {
        case mv_editor_inspector_transform_field::none:
            break;
        case mv_editor_inspector_transform_field::position_x:
            transform_x_input_.value = formatted;
            break;
        case mv_editor_inspector_transform_field::position_y:
            transform_y_input_.value = formatted;
            break;
        case mv_editor_inspector_transform_field::scale:
            transform_scale_input_.value = formatted;
            break;
        }
        if (inspector_edit_pending_) {
            commit_history("Edit Transform");
        }
    }
    return changed;
}

bool mv_editor_scene::apply_inspector_transform_opacity_result(
    const mv_editor_inspector_transform_opacity_result& result,
    mv::composition::component& transform) {
    if (!result.changed) {
        return false;
    }
    transform.opacity = result.opacity;
    dirty_ = true;
    return true;
}

bool mv_editor_scene::apply_inspector_transform_card_result(
    const mv_editor_inspector_transform_card_result& result,
    mv::composition::component& transform) {
    bool changed = false;
    changed |= apply_inspector_transform_input_result(result.position_x, transform);
    changed |= apply_inspector_transform_input_result(result.position_y, transform);
    changed |= apply_inspector_transform_input_result(result.scale, transform);
    changed |= apply_inspector_transform_opacity_result(result.opacity, transform);
    return changed;
}

bool mv_editor_scene::apply_inspector_component_result(const mv_editor_inspector_component_result& result,
                                                       mv::composition::component& component) {
    if (result.component_id.empty() || component.id != result.component_id) {
        return false;
    }

    bool changed = false;
    if (result.text_changed) {
        component.text = result.text;
        changed = true;
    }
    if (result.fill_changed) {
        component.fill = result.fill;
        changed = true;
    }
    if (result.script_entry_changed) {
        component.script_entry = result.script_entry.empty() ? "update" : result.script_entry;
        changed = true;
    }
    if (result.script_source_changed) {
        component.script_source = result.script_source;
        changed = true;
    }

    if (changed) {
        dirty_ = true;
        if (result.edit_pending) {
            inspector_edit_pending_ = true;
        }
    }
    if (result.commit_requested && inspector_edit_pending_) {
        commit_history(result.commit_label.empty() ? "Edit Component" : result.commit_label);
    }
    return changed;
}

bool mv_editor_scene::apply_inspector_component_amount_result(
    const mv_editor_inspector_component_amount_result& result,
    mv::composition::component& component) {
    if (!result.changed || result.component_id.empty() || component.id != result.component_id) {
        return false;
    }
    component.amount = result.amount;
    dirty_ = true;
    return true;
}

bool mv_editor_scene::apply_inspector_component_remove_result(
    const mv_editor_inspector_component_remove_result& result,
    mv::composition::layer& layer) {
    if (!result.requested || result.component_id.empty()) {
        return false;
    }
    layer.components.erase(
        std::remove_if(layer.components.begin(), layer.components.end(), [&](const auto& component) {
            return component.type != "transform" && component.id == result.component_id;
        }),
        layer.components.end());
    dirty_ = true;
    commit_history("Remove Component");
    return true;
}

bool mv_editor_scene::apply_inspector_add_component_result(
    const mv_editor_inspector_add_component_result& result) {
    if (!result.requested) {
        return false;
    }
    context_menu_open_ = true;
    context_menu_target_ = context_menu_target::components;
    context_menu_position_ = result.menu_position;
    return true;
}

void mv_editor_scene::close_metadata_modal() {
    metadata_modal_open_ = false;
    name_input_.active = false;
    author_input_.active = false;
}

void mv_editor_scene::draw() {
    virtual_screen::begin_ui();
    draw_scene_background(*g_theme);
    ui::begin_render_frame();

    const mv_editor_header_layout header_layout = mv_editor_header_layout_for();
    const mv_editor_frame_layout frame_layout = mv_editor_frame_layout_for();

    const std::string title = package_.meta.name.empty() ? song_.meta.title + " MV" : package_.meta.name;
    const std::string subtitle = song_.meta.title + " / " + song_.meta.artist + "   " + ms_label(playhead_ms_);
    const mv_editor_header_action header_action = draw_mv_editor_header(
        header_layout,
        metadata_modal_open_,
        preview_playing_,
        history_.can_undo(),
        history_.can_redo(),
        dirty_,
        title,
        subtitle);
    if (apply_header_action(header_action)) {
        ui::end_render_frame();
        virtual_screen::end();
        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    const Rectangle bottom_panel = frame_layout.bottom_panel;
    const Rectangle bottom_tab_bar = frame_layout.bottom_tab_bar;
    const Rectangle bottom_content_panel = frame_layout.bottom_content_panel;
    const Rectangle timeline_panel = frame_layout.timeline_panel;
    const Rectangle project_panel = frame_layout.project_panel;
    const Rectangle layers_panel = frame_layout.layers_panel;
    const Rectangle preview_panel = frame_layout.preview_panel;
    const Rectangle inspector_panel = frame_layout.inspector_panel;
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const float wheel = ui::mouse_wheel_move();
    const bool shift_down = ui::is_shift_down();
    const bool ctrl_down = ui::is_key_down(KEY_LEFT_CONTROL) || ui::is_key_down(KEY_RIGHT_CONTROL);
    bool context_menu_opened_this_frame = false;
    if (ui::is_mouse_button_pressed(MOUSE_BUTTON_RIGHT)) {
        context_menu_open_ = true;
        context_menu_opened_this_frame = true;
        context_menu_position_ = mouse;
        if (ui::contains_point(bottom_content_panel, mouse)) {
            context_menu_target_ = bottom_panel_tab_ == bottom_panel_tab::project
                ? context_menu_target::project_assets
                : context_menu_target::timeline;
        } else if (ui::contains_point(layers_panel, mouse)) {
            context_menu_target_ = context_menu_target::hierarchy;
        } else {
            context_menu_open_ = false;
            context_menu_target_ = context_menu_target::none;
        }
    }

    apply_hierarchy_result(draw_mv_hierarchy_panel(
        layers_panel,
        composition_,
        selected_layer_id_,
        hierarchy_scroll_offset_,
        hierarchy_scrollbar_dragging_,
        hierarchy_scrollbar_drag_offset_,
        mouse,
        wheel,
        shift_down,
        ctrl_down));

    ui::panel(preview_panel);
    draw_section_title(preview_panel, "Composition", "Active camera preview");
    const Rectangle preview_outer = preview_outer_rect_for(preview_panel);
    const Rectangle preview = preview_canvas_rect_for(preview_outer,
                                                      composition_.canvas_data.width,
                                                      composition_.canvas_data.height);
    ui::surface(preview_outer, with_alpha(g_theme->bg_alt, 180), g_theme->border_light, 1.0f);
    {
        ui::scoped_clip_rect clip(preview);
        draw_mv_preview_background(preview, composition_);
        std::array<float, 128> spectrum = {};
        std::array<float, 256> waveform_samples = {};
        const bool has_spectrum = preview_audio_loaded_ &&
            audio_manager::instance().get_preview_fft256(spectrum);
        const bool has_waveform_samples = preview_audio_loaded_ &&
            audio_manager::instance().get_preview_oscilloscope256(waveform_samples);
        std::vector<const mv::composition::layer*> draw_layers;
        for (const auto& layer : composition_.objects) {
            if (layer_active_at(layer, playhead_ms_)) {
                draw_layers.push_back(&layer);
            }
        }
        std::sort(draw_layers.begin(), draw_layers.end(), [](const auto* left, const auto* right) {
            return left->order < right->order;
        });
        for (const auto* layer : draw_layers) {
            const Texture2D* texture = nullptr;
            const mv::composition::component* renderer = renderer_or_null(*layer);
            if (renderer != nullptr && renderer->type == "ImageRenderer") {
                if (const mv::composition::asset_ref* asset = find_asset(renderer->asset_id);
                    asset != nullptr) {
                    texture = texture_for_asset(*asset);
                }
            }
            mv::composition::layer evaluated_layer = *layer;
            apply_evaluated_transform(evaluated_layer, mv::composition::evaluate_transform(*layer, playhead_ms_));
            hydrate_lua_script_sources(evaluated_layer);
            evaluate_preview_behaviours(evaluated_layer, playhead_ms_);
            draw_preview_layer(preview, composition_, evaluated_layer, layer->id == selected_layer_id_,
                               playhead_ms_,
                               texture,
                               has_waveform_samples ? &waveform_samples : nullptr,
                               has_spectrum ? &spectrum : nullptr);
        }

        auto texture_for_layer = [&](const mv::composition::layer& layer) -> const Texture2D* {
            const mv::composition::component* renderer = renderer_or_null(layer);
            if (renderer == nullptr || renderer->type != "ImageRenderer") {
                return nullptr;
            }
            const mv::composition::asset_ref* asset = find_asset(renderer->asset_id);
            return asset == nullptr ? nullptr : texture_for_asset(*asset);
        };
        auto evaluated_bounds_for_layer = [&](const mv::composition::layer& layer) {
            mv::composition::layer evaluated_layer = layer;
            apply_evaluated_transform(evaluated_layer, mv::composition::evaluate_transform(layer, playhead_ms_));
            evaluate_preview_behaviours(evaluated_layer, playhead_ms_);
            return layer_preview_bounds(preview, composition_, evaluated_layer, texture_for_layer(layer));
        };

        if (preview_drag_mode_ != mv_preview_drag_mode::none) {
            const std::string drag_layer_id = preview_drag_layer_id_.empty()
                ? selected_layer_id_
                : preview_drag_layer_id_;
            const auto drag_layer_it =
                std::find_if(composition_.objects.begin(), composition_.objects.end(), [&](const auto& layer) {
                    return layer.id == drag_layer_id;
                });
            const bool editable_drag_layer =
                drag_layer_it != composition_.objects.end() && !drag_layer_it->locked;
            apply_preview_drag_update_result(
                preview_drag_update_result_for(
                    drag_layer_id,
                    preview_drag_mode_,
                    mouse,
                    preview_drag_origin_mouse_,
                    preview_drag_origin_rect_),
                preview);
            apply_preview_drag_end_result(preview_drag_end_result_for(
                preview_drag_mode_ != mv_preview_drag_mode::none,
                editable_drag_layer));
        }

        if (ui::contains_point(preview, mouse) &&
            ui::is_mouse_button_pressed() &&
            preview_drag_mode_ == mv_preview_drag_mode::none &&
            !context_menu_open_) {
            mv_editor_preview_drag_start_result preview_drag_start_result;
            if (mv::composition::layer* selected = selected_layer(); selected != nullptr) {
                const Rectangle selected_bounds = evaluated_bounds_for_layer(*selected);
                preview_drag_start_result =
                    preview_drag_start_result_for_selected_handles(*selected, selected_bounds, mouse);
            }
            if (!preview_drag_start_result.drag_started) {
                for (auto it = draw_layers.rbegin(); it != draw_layers.rend(); ++it) {
                    const mv::composition::layer& candidate = **it;
                    const Rectangle bounds = evaluated_bounds_for_layer(candidate);
                    preview_drag_start_result =
                        preview_drag_start_result_for_layer_body(candidate, bounds, mouse);
                    if (!preview_drag_start_result.selected_layer_id.empty() ||
                        preview_drag_start_result.drag_started) {
                        break;
                    }
                }
            }
            apply_preview_drag_start_result(preview_drag_start_result);
        }

        if (const mv::composition::layer* selected = selected_layer();
            selected != nullptr && preview_transformable(*selected)) {
            draw_preview_transform_overlay(evaluated_bounds_for_layer(*selected), selected->locked);
        }
    }
    ui::surface(preview, Color{0, 0, 0, 0}, g_theme->border_active, 1.5f);

    const bottom_tabs_layout tabs_layout = bottom_tabs_layout_for(bottom_tab_bar);
    apply_bottom_tab_action(draw_mv_editor_bottom_tabs(
        tabs_layout,
        bottom_panel_tab_ == bottom_panel_tab::timeline));

    if (bottom_panel_tab_ == bottom_panel_tab::timeline) {
        mv_editor_timeline_drag_update_mode drag_update_mode = mv_editor_timeline_drag_update_mode::none;
        switch (timeline_drag_mode_) {
        case timeline_drag_mode::none:
            break;
        case timeline_drag_mode::move:
            drag_update_mode = mv_editor_timeline_drag_update_mode::move;
            break;
        case timeline_drag_mode::trim_start:
            drag_update_mode = mv_editor_timeline_drag_update_mode::trim_start;
            break;
        case timeline_drag_mode::trim_end:
            drag_update_mode = mv_editor_timeline_drag_update_mode::trim_end;
            break;
        }
        const mv_editor_timeline_view_result timeline_result = draw_mv_timeline_view(
            timeline_panel,
            composition_,
            selected_layer_id_,
            playhead_ms_,
            composition_duration_ms(),
            {
                .vertical_scroll_offset = timeline_vertical_scroll_offset_,
                .horizontal_scroll_ms = timeline_horizontal_scroll_ms_,
                .zoom = timeline_zoom_,
                .drag = {
                    .active = timeline_drag_mode_ != timeline_drag_mode::none,
                    .layer_id = timeline_drag_layer_id_,
                    .mode = drag_update_mode,
                    .origin_mouse_x = timeline_drag_origin_mouse_x_,
                    .origin_start_ms = timeline_drag_origin_start_ms_,
                    .origin_duration_ms = timeline_drag_origin_duration_ms_,
                },
            },
            mouse,
            wheel,
            shift_down,
            ctrl_down);

        timeline_vertical_scroll_offset_ = timeline_result.vertical_scroll_offset;
        timeline_horizontal_scroll_ms_ = timeline_result.horizontal_scroll_ms;
        timeline_zoom_ = timeline_result.zoom;
        apply_timeline_layer_row_result(timeline_result.layer_row);
        apply_timeline_layer_row_result(timeline_result.delete_layer);
        apply_timeline_drag_start_result(timeline_result.drag_start);
        apply_timeline_drag_update_result(timeline_result.drag_update);
        apply_timeline_drag_end_result(timeline_result.drag_end);
        apply_timeline_scrub_result(timeline_result.scrub);
    } else {
        apply_project_panel_result(draw_mv_project_panel(
            project_panel,
            composition_,
            selected_project_asset_id_,
            project_scroll_offset_,
            project_scrollbar_dragging_,
            project_scrollbar_drag_offset_,
            mouse,
            wheel,
            shift_down,
            ctrl_down));
    }

    ui::panel(inspector_panel);
    draw_section_title(inspector_panel, "Inspector", "Selected object");
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        ui::draw_text_in_rect("No object selected", 16,
                              {inspector_panel.x + 18.0f, inspector_panel.y + 70.0f,
                               inspector_panel.width - 36.0f, 36.0f},
                              g_theme->text_muted, ui::text_align::left);
    } else {
        sync_inspector_inputs(*layer);
        const scroll_panel_layout inspector_layout = panel_scroll_layout_for(inspector_panel);
        const Rectangle inspector_view = inspector_layout.viewport;
        const Rectangle inspector_scrollbar = inspector_layout.scrollbar;
        mv::composition::component* transform = ensure_transform(*layer);
        const auto color_picker_for = [&](const mv::composition::component& component)
            -> const ui::inspector::color_picker_state* {
            const auto picker_it = component_color_pickers_.find(component.id);
            return picker_it == component_color_pickers_.end() ? nullptr : &picker_it->second;
        };
        float inspector_content_height = 58.0f + 38.0f + 10.0f + 28.0f + 18.0f;
        for (const mv::composition::component& component : layer->components) {
            inspector_content_height += component_inspector_card_height(component, color_picker_for(component)) + 10.0f;
        }
        const float inspector_max_scroll = ui::max_scroll_offset(inspector_content_height, inspector_view);
        inspector_scroll_offset_ = ui::clamp_scroll_offset(inspector_scroll_offset_, inspector_max_scroll);
        if (!shift_down && !ctrl_down) {
            inspector_scroll_offset_ = ui::wheel_scrolled_offset(
                inspector_view, mouse, wheel, inspector_scroll_offset_, inspector_max_scroll, kInspectorWheelStep);
        }
        const ui::scrollbar_interaction inspector_scrollbar_result =
            ui::vertical_scrollbar(inspector_scrollbar,
                                   inspector_content_height,
                                   inspector_scroll_offset_,
                                   inspector_scrollbar_dragging_,
                                   inspector_scrollbar_drag_offset_, {
                                       .min_thumb_height = 30.0f,
                                       .drag_blocked_by_layer = false,
                                   });
        inspector_scroll_offset_ = inspector_scrollbar_result.scroll_offset;
        inspector_scrollbar_dragging_ = inspector_scrollbar_result.dragging;
        Rectangle body = {inspector_view.x, inspector_view.y - inspector_scroll_offset_,
                          inspector_view.width, inspector_content_height};
        {
            ui::scoped_clip_rect inspector_clip(inspector_view);
        apply_inspector_layer_name_result(
            draw_mv_inspector_layer_header(body, layer_name_input_, layer_type_label(*layer)),
            *layer);
        if (layer != nullptr) {
            bool inspector_changed = false;
            float detail_y = body.y + 58.0f;
            const float transform_card_h = component_inspector_card_height(*transform);
            inspector_changed |= apply_inspector_transform_card_result(
                draw_mv_inspector_transform_card(
                    {body.x, detail_y, body.width, transform_card_h},
                    *transform,
                    transform_x_input_,
                    transform_y_input_,
                    transform_scale_input_),
                *transform);
            detail_y += transform_card_h + 10.0f;
            if (layer != nullptr) {
                mv_editor_inspector_component_remove_result remove_component_result;
                for (mv::composition::component& component : layer->components) {
                    if (component.type == "transform") {
                        continue;
                    }
                    const float card_top = detail_y;
                    const float card_h = component_inspector_card_height(component, color_picker_for(component));
                    const mv_editor_inspector_component_card_result component_card_result =
                        draw_mv_inspector_component_card(
                            {body.x, card_top, body.width, card_h},
                            component,
                            component_text_inputs_,
                            component_fill_inputs_,
                            component_script_entry_inputs_,
                            component_script_inputs_,
                            component_color_pickers_);
                    if (component_card_result.remove.requested) {
                        remove_component_result = component_card_result.remove;
                    }
                    apply_inspector_component_result(component_card_result.component, component);
                    inspector_changed |= apply_inspector_component_amount_result(
                        component_card_result.amount,
                        component);
                    detail_y = card_top + card_h + 10.0f;
                }
                inspector_changed |= apply_inspector_component_remove_result(remove_component_result, *layer);
                const Rectangle add_component_btn = {body.x, detail_y, body.width, 28.0f};
                if (apply_inspector_add_component_result(
                        draw_mv_inspector_add_component_button(add_component_btn))) {
                    context_menu_opened_this_frame = true;
                }
                detail_y += 36.0f;
            }
            if (inspector_changed) {
                validate_composition();
                inspector_edit_pending_ = true;
            }
            if (inspector_edit_pending_ && ui::is_mouse_button_released()) {
                commit_history("Edit Layer");
            }
        }
        }
        ui::scrollbar(inspector_scrollbar, inspector_content_height, inspector_scroll_offset_, {
            .track_color = with_alpha(g_theme->row, 120),
            .thumb_color = g_theme->slider_fill,
            .min_thumb_height = 30.0f,
            .custom_colors = true,
        });
    }

    if (!diagnostics_.empty()) {
        const Rectangle diag = diagnostics_rect_for(inspector_panel);
        ui::surface(diag, with_alpha(g_theme->error, 50), with_alpha(g_theme->error, 160), 1.0f);
        ui::draw_text_in_rect(diagnostics_.front().c_str(), 11, ui::inset(diag, 8.0f),
                              g_theme->text, ui::text_align::left);
    }

    if (metadata_modal_open_) {
        context_menu_open_ = false;
    }
    if (context_menu_open_) {
        const mv::composition::layer* context_selected_layer = selected_layer();
        const bool has_layer = context_selected_layer != nullptr;
        const bool has_effects =
            has_layer && !mv::composition::effect_components(*context_selected_layer).empty();
        apply_context_menu_result(draw_mv_context_menu(
            context_menu_target_,
            context_menu_position_,
            has_layer,
            has_effects,
            context_menu_opened_this_frame,
            mouse));
    }

    if (metadata_modal_open_) {
        apply_metadata_modal_result(draw_mv_metadata_modal(metadata_modal_open_anim_, name_input_, author_input_));
    }

    ui::end_render_frame();
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}

void mv_editor_scene::validate_composition() {
    diagnostics_.clear();
    const mv::composition::parse_result parsed = mv::composition::parse(mv::composition::serialize(composition_));
    if (!parsed.success) {
        diagnostics_ = parsed.errors;
    }
}

bool mv_editor_scene::sync_script_assets_from_components() {
    std::vector<std::string> errors;
    for (mv::composition::layer& layer : composition_.objects) {
        for (mv::composition::component& component : layer.components) {
            if (component.type != "LuaBehaviour" ||
                component.script_asset_id.empty() ||
                component.script_source.empty()) {
                continue;
            }
            auto asset_it = std::find_if(composition_.assets.begin(), composition_.assets.end(), [&](const auto& asset) {
                return asset.id == component.script_asset_id;
            });
            if (asset_it == composition_.assets.end() || asset_it->type != "script") {
                continue;
            }
            if (!mv::update_script_asset_source(package_, *asset_it, component.script_source, &errors)) {
                diagnostics_ = errors.empty()
                    ? std::vector<std::string>{"Failed to update MV script asset."}
                    : errors;
                return false;
            }
        }
    }
    return true;
}

void mv_editor_scene::save_mv() {
    if (inspector_edit_pending_) {
        commit_history("Edit Layer");
    }
    package_.meta.song_id = song_.meta.song_id;
    package_.meta.name = name_input_.value.empty() ? (song_.meta.title + " MV") : name_input_.value;
    package_.meta.author = author_input_.value;
    package_.meta.composition_file = "composition.rmvcomp";
    package_.meta.format_version = 2;
    if (!sync_script_assets_from_components()) {
        return;
    }
    validate_composition();
    if (!diagnostics_.empty()) {
        return;
    }
    std::vector<std::string> save_errors;
    if (mv::save_composition(package_, composition_) &&
        mv::save_metadata(package_, &save_errors)) {
        metadata_dirty_ = false;
        history_.mark_clean(composition_);
        update_dirty_from_history();
    } else if (!save_errors.empty()) {
        diagnostics_ = save_errors;
    } else {
        diagnostics_ = {"Failed to save MV composition."};
    }
}

void mv_editor_scene::commit_history(const std::string& label) {
    history_.commit(composition_, selected_layer_id_, label);
    inspector_edit_pending_ = false;
    update_dirty_from_history();
}

void mv_editor_scene::update_dirty_from_history() {
    dirty_ = metadata_dirty_ || !history_.is_clean(composition_);
}

void mv_editor_scene::apply_history_snapshot(const mv::composition::edit_snapshot& snapshot) {
    composition_ = snapshot.composition;
    selected_layer_id_ = snapshot.selected_layer_id;
    if (selected_layer() == nullptr && !composition_.objects.empty()) {
        selected_layer_id_ = composition_.objects.back().id;
    }
    reset_inspector_inputs();
    playhead_ms_ = std::clamp(playhead_ms_, 0.0, composition_duration_ms());
    seek_preview_audio_to_playhead();
    update_dirty_from_history();
    validate_composition();
}

bool mv_editor_scene::undo_edit() {
    if (inspector_edit_pending_) {
        commit_history("Edit Layer");
    }
    mv::composition::edit_snapshot snapshot;
    if (!history_.undo(snapshot)) {
        return false;
    }
    set_preview_playing(false);
    inspector_edit_pending_ = false;
    apply_history_snapshot(snapshot);
    return true;
}

bool mv_editor_scene::redo_edit() {
    mv::composition::edit_snapshot snapshot;
    if (!history_.redo(snapshot)) {
        return false;
    }
    set_preview_playing(false);
    inspector_edit_pending_ = false;
    apply_history_snapshot(snapshot);
    return true;
}

void mv_editor_scene::reset_inspector_inputs() {
    layer_name_input_ = {};
    layer_text_input_ = {};
    layer_fill_input_ = {};
    transform_x_input_ = {};
    transform_y_input_ = {};
    transform_scale_input_ = {};
    component_text_inputs_.clear();
    component_fill_inputs_.clear();
    component_script_entry_inputs_.clear();
    component_script_inputs_.clear();
    component_color_pickers_.clear();
    inspector_input_layer_id_.clear();
}

void mv_editor_scene::sync_inspector_inputs(const mv::composition::layer& layer) {
    if (inspector_input_layer_id_ == layer.id) {
        return;
    }
    inspector_input_layer_id_ = layer.id;
    inspector_scroll_offset_ = 0.0f;
    inspector_scrollbar_dragging_ = false;
    inspector_scrollbar_drag_offset_ = 0.0f;
    layer_name_input_ = {};
    layer_text_input_ = {};
    layer_fill_input_ = {};
    transform_x_input_ = {};
    transform_y_input_ = {};
    transform_scale_input_ = {};
    component_text_inputs_.clear();
    component_fill_inputs_.clear();
    component_script_entry_inputs_.clear();
    component_script_inputs_.clear();
    component_color_pickers_.clear();
    layer_name_input_.value = layer.name;
    for (const mv::composition::component& component : layer.components) {
        if (!component.text.empty() || component.type == "TextRenderer") {
            component_text_inputs_[component.id].value = component.text;
        }
        if (!component.fill.empty() ||
            component.type == "TextRenderer" ||
            component.type == "ShapeRenderer" ||
            component.type == "BackgroundRenderer" ||
            component.type == "BeatGridRenderer" ||
            component.type == "WaveformRenderer" ||
            component.type == "SpectrumRenderer") {
            component_fill_inputs_[component.id].value =
                component.fill.empty() ? "#ffffff" : component.fill;
        }
        if (component.type == "LuaBehaviour") {
            component_script_entry_inputs_[component.id].value =
                component.script_entry.empty() ? "update" : component.script_entry;
            component_script_inputs_[component.id].value =
                component.script_source.empty() ? "function update(self, ctx) end" : component.script_source;
        }
    }
    if (const mv::composition::component* transform = mv::composition::transform_component(layer)) {
        transform_x_input_.value = format_float_input(transform->position_x, 0);
        transform_y_input_.value = format_float_input(transform->position_y, 0);
        transform_scale_input_.value = format_float_input(transform->scale_x, 2);
    }
}

bool mv_editor_scene::inspector_text_input_active() const {
    if (layer_name_input_.active || layer_text_input_.active || layer_fill_input_.active ||
        transform_x_input_.active || transform_y_input_.active || transform_scale_input_.active) {
        return true;
    }
    for (const auto& [_, state] : component_text_inputs_) {
        if (state.active) {
            return true;
        }
    }
    for (const auto& [_, state] : component_fill_inputs_) {
        if (state.active) {
            return true;
        }
    }
    for (const auto& [_, state] : component_script_entry_inputs_) {
        if (state.active) {
            return true;
        }
    }
    for (const auto& [_, state] : component_script_inputs_) {
        if (state.active) {
            return true;
        }
    }
    return false;
}

void mv_editor_scene::add_empty_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.objects.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-object");
    layer.name = "Object " + std::to_string(index);
    layer.order = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = std::max(8000.0, composition_duration_ms() - playhead_ms_);
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    layer.components.push_back(mv::composition::make_transform_component(transform));
    composition_.objects.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Empty Object");
}

void mv_editor_scene::add_text_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.objects.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-text");
    layer.name = "Text " + std::to_string(index);
    layer.order = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = 8000.0;
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    mv::composition::component renderer = mv::composition::make_component("TextRenderer");
    renderer.id = "renderer-text";
    renderer.text = "Text";
    renderer.fill = "#d8d4ff";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
    composition_.objects.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Text");
}

void mv_editor_scene::add_rect_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.objects.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-rect");
    layer.name = "Rectangle " + std::to_string(index);
    layer.order = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = 8000.0;
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    transform.opacity = 0.75f;
    mv::composition::component renderer = mv::composition::make_component("ShapeRenderer");
    renderer.id = "renderer-shape";
    renderer.shape = "rect";
    renderer.fill = "#6ee7b7";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
    composition_.objects.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Rectangle");
}

void mv_editor_scene::add_image_layer() {
    const std::string source_path = file_dialog::open_image_file();
    if (source_path.empty()) {
        return;
    }

    std::vector<std::string> errors;
    const std::optional<mv::composition::asset_ref> imported =
        mv::import_image_asset(package_, source_path, &errors);
    if (!imported.has_value()) {
        diagnostics_ = errors.empty() ? std::vector<std::string>{"Failed to import MV image asset."} : errors;
        return;
    }

    const auto existing = std::find_if(composition_.assets.begin(), composition_.assets.end(), [&](const auto& asset) {
        return asset.id == imported->id;
    });
    if (existing == composition_.assets.end()) {
        composition_.assets.push_back(*imported);
    }

    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.objects.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-image");
    layer.name = "Image " + std::to_string(index);
    layer.order = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = 8000.0;
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    mv::composition::component renderer = mv::composition::make_component("ImageRenderer");
    renderer.id = "renderer-image";
    renderer.asset_id = imported->id;
    renderer.fill = "#ffffff";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
    composition_.objects.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Image");
}

void mv_editor_scene::import_image_asset_to_project() {
    const std::string source_path = file_dialog::open_image_file();
    if (source_path.empty()) {
        return;
    }

    std::vector<std::string> errors;
    const std::optional<mv::composition::asset_ref> imported =
        mv::import_image_asset(package_, source_path, &errors);
    if (!imported.has_value()) {
        diagnostics_ = errors.empty() ? std::vector<std::string>{"Failed to import MV image asset."} : errors;
        return;
    }

    const auto existing = std::find_if(composition_.assets.begin(), composition_.assets.end(), [&](const auto& asset) {
        return asset.id == imported->id;
    });
    if (existing == composition_.assets.end()) {
        composition_.assets.push_back(*imported);
        selected_project_asset_id_ = imported->id;
        validate_composition();
        commit_history("Import Image Asset");
    } else {
        selected_project_asset_id_ = existing->id;
    }
}

void mv_editor_scene::create_script_asset() {
    const int index = static_cast<int>(std::count_if(composition_.assets.begin(), composition_.assets.end(),
                                                     [](const auto& asset) {
                                                         return asset.type == "script";
                                                     })) + 1;
    const std::string name = "Script " + std::to_string(index);
    const std::string source =
        "function start(self, ctx)\n"
        "end\n"
        "\n"
        "function update(self, ctx)\n"
        "  -- self.transform.position.x = self.transform.position.x + math.sin(ctx.songTimeMs / 300) * 40\n"
        "end\n";

    std::vector<std::string> errors;
    const std::optional<mv::composition::asset_ref> created =
        mv::create_script_asset(package_, name, source, &errors);
    if (!created.has_value()) {
        diagnostics_ = errors.empty() ? std::vector<std::string>{"Failed to create MV script asset."} : errors;
        return;
    }

    const auto existing = std::find_if(composition_.assets.begin(), composition_.assets.end(), [&](const auto& asset) {
        return asset.id == created->id;
    });
    if (existing == composition_.assets.end()) {
        composition_.assets.push_back(*created);
    }
    selected_project_asset_id_ = created->id;
    validate_composition();
    commit_history("Create Script Asset");
}

bool mv_editor_scene::assign_asset_to_selected_component(const mv::composition::asset_ref& asset) {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return false;
    }

    if (asset.type == "image") {
        if (mv::composition::component* image = mv::composition::find_component(*layer, "ImageRenderer")) {
            image->asset_id = asset.id;
            validate_composition();
            return true;
        }
        return false;
    }

    if (asset.type == "script") {
        if (mv::composition::component* lua = mv::composition::find_component(*layer, "LuaBehaviour")) {
            lua->script_asset_id = asset.id;
            if (const std::optional<std::string> source = mv::read_script_asset_source(package_, asset)) {
                lua->script_source = *source;
            }
            if (lua->script_entry.empty()) {
                lua->script_entry = "update";
            }
            validate_composition();
            reset_inspector_inputs();
            return true;
        }
    }

    return false;
}

void mv_editor_scene::add_beat_grid_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.objects.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-beat-grid");
    layer.name = "Beat Grid " + std::to_string(index);
    layer.order = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = std::max(8000.0, composition_duration_ms() - playhead_ms_);
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    transform.opacity = 0.8f;
    mv::composition::component renderer = mv::composition::make_component("BeatGridRenderer");
    renderer.id = "renderer-beat-grid";
    renderer.fill = "#8b7cf6";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
    composition_.objects.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Beat Grid");
}

void mv_editor_scene::add_waveform_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.objects.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-waveform");
    layer.name = "Waveform " + std::to_string(index);
    layer.order = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = std::max(8000.0, composition_duration_ms() - playhead_ms_);
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.72f;
    transform.opacity = 0.85f;
    mv::composition::component renderer = mv::composition::make_component("WaveformRenderer");
    renderer.id = "renderer-waveform";
    renderer.fill = "#6ee7b7";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
    composition_.objects.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Waveform");
}

void mv_editor_scene::add_spectrum_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.objects.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-spectrum");
    layer.name = "Spectrum " + std::to_string(index);
    layer.order = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = std::max(8000.0, composition_duration_ms() - playhead_ms_);
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) - 249.0f;
    transform.scale_x = 1.5f;
    transform.scale_y = 498.0f / 420.0f;
    mv::composition::component renderer = mv::composition::make_component("SpectrumRenderer");
    renderer.id = "renderer-spectrum";
    renderer.shape = "title";
    renderer.fill = "#a855f7";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
    composition_.objects.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Spectrum");
}

void mv_editor_scene::add_component_to_selected_layer(const std::string& type) {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr || type.empty()) {
        return;
    }
    if (type == "ImageRenderer") {
        const std::string source_path = file_dialog::open_image_file();
        if (source_path.empty()) {
            return;
        }
        std::vector<std::string> errors;
        const std::optional<mv::composition::asset_ref> imported =
            mv::import_image_asset(package_, source_path, &errors);
        if (!imported.has_value()) {
            diagnostics_ = errors.empty() ? std::vector<std::string>{"Failed to import MV image asset."} : errors;
            return;
        }
        const auto existing = std::find_if(composition_.assets.begin(), composition_.assets.end(), [&](const auto& asset) {
            return asset.id == imported->id;
        });
        if (existing == composition_.assets.end()) {
            composition_.assets.push_back(*imported);
        }
        mv::composition::component image = mv::composition::make_component("ImageRenderer");
        image.id = next_component_id(composition_, "ImageRenderer");
        image.asset_id = imported->id;
        image.fill = "#ffffff";
        layer->components.push_back(std::move(image));
        validate_composition();
        commit_history("Add Image Component");
        return;
    }
    mv::composition::component component = mv::composition::make_component(type);
    const mv::composition::component_category category = mv::composition::category_for_component_type(type);
    if (category == mv::composition::component_category::modifier) {
        component.id = next_effect_id(composition_, "fx-" + mv::composition::canonical_component_type(type));
    } else {
        component.id = next_component_id(composition_, mv::composition::canonical_component_type(type));
    }
    layer->components.push_back(std::move(component));
    validate_composition();
    reset_inspector_inputs();
    commit_history("Add Component");
}

void mv_editor_scene::key_selected_transform() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    const mv::composition::component* transform = mv::composition::transform_component(*layer);
    if (transform == nullptr) {
        return;
    }
    const auto add_point = [&](const std::string& target, float value) {
        mv::composition::keyframe_track& track = mv::composition::ensure_keyframe_track(*layer, target);
        mv::composition::upsert_keyframe(track, {playhead_ms_, value, "linear"});
    };
    add_point("transform.position.x", transform->position_x);
    add_point("transform.position.y", transform->position_y);
    add_point("transform.scale.x", transform->scale_x);
    add_point("transform.scale.y", transform->scale_y);
    add_point("transform.rotationDeg", transform->rotation_deg);
    add_point("transform.opacity", transform->opacity);
    validate_composition();
    commit_history("Set Key");
}

void mv_editor_scene::delete_transform_keyframes_at_playhead() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }

    const int removed = mv::composition::erase_transform_keyframes_near(
        *layer, playhead_ms_, kKeyframeHitToleranceMs);
    if (removed <= 0) {
        return;
    }
    validate_composition();
    commit_history("Clear Key");
}

int mv_editor_scene::transform_keyframe_count_at_playhead() const {
    const mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return 0;
    }
    return mv::composition::count_transform_keyframes_near(
        *layer, playhead_ms_, kKeyframeHitToleranceMs);
}

void mv_editor_scene::add_fade_effect_to_selected_layer() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    if (mv::composition::component* effect = find_effect_component(*layer, "Fade"); effect == nullptr) {
        layer->components.push_back(
            make_effect_component(next_effect_id(composition_, "fx-fade"), "Fade", "transform.opacity", 650.0f));
    } else {
        effect->target = "transform.opacity";
        effect->amount = effect->amount <= 0.0f ? 650.0f : effect->amount;
    }
    validate_composition();
    commit_history("Add Fade");
}

void mv_editor_scene::add_pulse_effect_to_selected_layer() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    if (mv::composition::component* effect = find_effect_component(*layer, "Pulse"); effect == nullptr) {
        layer->components.push_back(
            make_effect_component(next_effect_id(composition_, "fx-pulse"), "Pulse", "transform.scale", 0.08f));
    } else {
        effect->target = "transform.scale";
        effect->amount = effect->amount <= 0.0f ? 0.08f : effect->amount;
    }
    validate_composition();
    commit_history("Add Pulse");
}

void mv_editor_scene::add_flash_effect_to_selected_layer() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    if (mv::composition::component* effect = find_effect_component(*layer, "Flash"); effect == nullptr) {
        layer->components.push_back(
            make_effect_component(next_effect_id(composition_, "fx-flash"), "Flash", "transform.opacity", 0.35f));
    } else {
        effect->target = "transform.opacity";
        effect->amount = effect->amount <= 0.0f ? 0.35f : effect->amount;
    }
    validate_composition();
    commit_history("Add Flash");
}

void mv_editor_scene::add_shake_effect_to_selected_layer() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    if (mv::composition::component* effect = find_effect_component(*layer, "Shake"); effect == nullptr) {
        layer->components.push_back(
            make_effect_component(next_effect_id(composition_, "fx-shake"), "Shake", "transform.position", 18.0f));
    } else {
        effect->target = "transform.position";
        effect->amount = effect->amount <= 0.0f ? 18.0f : effect->amount;
    }
    validate_composition();
    commit_history("Add Shake");
}

void mv_editor_scene::clear_selected_layer_effects() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr || mv::composition::effect_components(*layer).empty()) {
        return;
    }
    layer->components.erase(
        std::remove_if(layer->components.begin(), layer->components.end(), [](const auto& component) {
            return component.type == "Fade" ||
                   component.type == "Pulse" ||
                   component.type == "BeatReactive" ||
                   component.type == "Flash" ||
                   component.type == "Shake";
        }),
        layer->components.end());
    validate_composition();
    commit_history("Clear FX");
}

void mv_editor_scene::normalize_layer_z_order() {
    for (std::size_t i = 0; i < composition_.objects.size(); ++i) {
        composition_.objects[i].order = static_cast<int>((i + 1) * 10);
    }
}

bool mv_editor_scene::move_selected_layer(int direction) {
    if (selected_layer_id_.empty() || direction == 0 || composition_.objects.size() < 2) {
        return false;
    }
    const std::size_t index = layer_index_by_id(composition_, selected_layer_id_);
    if (index == static_cast<std::size_t>(-1)) {
        return false;
    }
    const int next_index_signed = static_cast<int>(index) + direction;
    if (next_index_signed < 0 || next_index_signed >= static_cast<int>(composition_.objects.size())) {
        return false;
    }
    std::swap(composition_.objects[index], composition_.objects[static_cast<std::size_t>(next_index_signed)]);
    normalize_layer_z_order();
    reset_inspector_inputs();
    validate_composition();
    commit_history(direction > 0 ? "Move Layer Up" : "Move Layer Down");
    return true;
}

void mv_editor_scene::delete_selected_layer() {
    if (selected_layer_id_.empty()) {
        return;
    }
    const auto it = std::find_if(composition_.objects.begin(), composition_.objects.end(), [&](const auto& layer) {
        return layer.id == selected_layer_id_;
    });
    if (it == composition_.objects.end()) {
        return;
    }
    composition_.objects.erase(it);
    selected_layer_id_.clear();
    if (!composition_.objects.empty()) {
        selected_layer_id_ = composition_.objects.back().id;
    }
    normalize_layer_z_order();
    validate_composition();
    commit_history("Delete Layer");
}

mv::composition::layer* mv_editor_scene::selected_layer() {
    const auto it = std::find_if(composition_.objects.begin(), composition_.objects.end(), [&](auto& layer) {
        return layer.id == selected_layer_id_;
    });
    return it == composition_.objects.end() ? nullptr : &*it;
}

const mv::composition::layer* mv_editor_scene::selected_layer() const {
    const auto it = std::find_if(composition_.objects.begin(), composition_.objects.end(), [&](const auto& layer) {
        return layer.id == selected_layer_id_;
    });
    return it == composition_.objects.end() ? nullptr : &*it;
}

const mv::composition::asset_ref* mv_editor_scene::find_asset(const std::string& asset_id) const {
    const auto it = std::find_if(composition_.assets.begin(), composition_.assets.end(), [&](const auto& asset) {
        return asset.id == asset_id;
    });
    return it == composition_.assets.end() ? nullptr : &*it;
}

void mv_editor_scene::hydrate_lua_script_sources(mv::composition::layer& layer) const {
    for (mv::composition::component& component : layer.components) {
        if (component.type != "LuaBehaviour" ||
            !component.script_source.empty() ||
            component.script_asset_id.empty()) {
            continue;
        }
        const mv::composition::asset_ref* asset = find_asset(component.script_asset_id);
        if (asset == nullptr || asset->type != "script") {
            continue;
        }
        if (const std::optional<std::string> source = mv::read_script_asset_source(package_, *asset)) {
            component.script_source = *source;
        }
    }
}

const Texture2D* mv_editor_scene::texture_for_asset(const mv::composition::asset_ref& asset) {
    const auto cached = asset_textures_.find(asset.id);
    if (cached != asset_textures_.end()) {
        return cached->second.id == 0 ? nullptr : &cached->second;
    }
    std::vector<std::string> errors;
    Texture2D texture = load_texture_from_asset_bytes(package_, asset, &errors);
    if (texture.id == 0) {
        TraceLog(LOG_WARNING, "MV editor: failed to load image asset texture %s", asset.path.c_str());
        if (!errors.empty()) {
            diagnostics_ = errors;
        }
        return nullptr;
    }
    const auto inserted = asset_textures_.emplace(asset.id, texture);
    return &inserted.first->second;
}

void mv_editor_scene::unload_asset_textures() {
    for (auto& [_, texture] : asset_textures_) {
        if (texture.id != 0) {
            UnloadTexture(texture);
        }
    }
    asset_textures_.clear();
}

bool mv_editor_scene::load_preview_audio() {
    audio_manager::instance().stop_preview();
    audio_manager::instance().unload_preview();
    if (song_.meta.audio_file.empty()) {
        diagnostics_.push_back("MV preview audio is unavailable: song audioFile is empty.");
        return false;
    }

    const std::filesystem::path audio_path = path_utils::join_utf8(song_.directory, song_.meta.audio_file);
    bool loaded = false;
    if (managed_content_storage::is_within_content_cache(audio_path)) {
        const managed_content_storage::managed_file_read_result read =
            managed_content_storage::read_managed_file(audio_path);
        if (read.managed) {
            loaded = read.success && audio_manager::instance().load_preview_from_memory(read.bytes);
            if (!loaded) {
                diagnostics_.push_back(read.error_message.empty()
                    ? "Failed to load decrypted MV preview audio."
                    : read.error_message);
            }
        } else {
            loaded = audio_manager::instance().load_preview(path_utils::to_utf8(audio_path));
        }
    } else {
        loaded = audio_manager::instance().load_preview(path_utils::to_utf8(audio_path));
    }

    if (!loaded) {
        diagnostics_.push_back("Failed to load MV preview audio.");
        return false;
    }
    audio_manager::instance().seek_preview(playhead_ms_ / 1000.0);
    return true;
}

void mv_editor_scene::stop_preview_audio() {
    preview_playing_ = false;
    audio_manager::instance().stop_preview();
    audio_manager::instance().unload_preview();
    preview_audio_loaded_ = false;
}

void mv_editor_scene::set_preview_playing(bool playing) {
    preview_playing_ = playing;
    if (!preview_audio_loaded_) {
        return;
    }
    if (playing) {
        seek_preview_audio_to_playhead();
        audio_manager::instance().play_preview(false);
    } else {
        audio_manager::instance().pause_preview();
    }
}

void mv_editor_scene::seek_preview_audio_to_playhead() {
    if (!preview_audio_loaded_) {
        return;
    }
    audio_manager::instance().seek_preview(std::clamp(playhead_ms_, 0.0, composition_duration_ms()) / 1000.0);
}

double mv_editor_scene::composition_duration_ms() const {
    double duration = std::max(1.0, composition_.duration_ms);
    for (const auto& layer : composition_.objects) {
        duration = std::max(duration, layer.duration_ms <= 0.0 ? 1.0 : layer.start_ms + layer.duration_ms);
    }
    return std::max(duration, 1000.0);
}
