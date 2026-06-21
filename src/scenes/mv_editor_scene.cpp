#include "mv_editor_scene.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>

#include "audio_manager.h"
#include "managed_content_storage.h"
#include "mv/composition/mv_composition_event_authoring.h"
#include "mv/composition/mv_composition_evaluator.h"
#include "mv/composition/mv_composition_presets.h"
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
#include "ui_layout.h"
#include "ui_text.h"
#include "ui_text_input.h"
#include "virtual_screen.h"

namespace {

constexpr float kHeaderHeight = 72.0f;
constexpr float kPadding = 24.0f;
constexpr float kPanelGap = 18.0f;
constexpr float kBackButtonWidth = 150.0f;
constexpr float kHeaderButtonHeight = 45.0f;
constexpr float kHeaderButtonWidth = 150.0f;
constexpr float kMetadataButtonWidth = 228.0f;
constexpr float kLayerPanelWidth = 330.0f;
constexpr float kInspectorWidth = 390.0f;
constexpr float kTimelineHeight = 190.0f;
constexpr float kWorkspaceTabHeight = 42.0f;
constexpr float kWorkspaceTabGap = 8.0f;
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

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
}

int hex_digit(char ch);

bool hex_color_filter(int codepoint, const std::string& current_value) {
    if (current_value.empty()) {
        return codepoint == '#';
    }
    if (current_value.size() >= 7) {
        return false;
    }
    return (codepoint >= '0' && codepoint <= '9') ||
           (codepoint >= 'a' && codepoint <= 'f') ||
           (codepoint >= 'A' && codepoint <= 'F');
}

bool is_valid_hex_color(const std::string& value) {
    if (value.size() != 7 || value[0] != '#') {
        return false;
    }
    for (std::size_t i = 1; i < value.size(); ++i) {
        if (hex_digit(value[i]) < 0) {
            return false;
        }
    }
    return true;
}

std::string normalize_hex_color(std::string value) {
    if (!is_valid_hex_color(value)) {
        return value;
    }
    value[0] = '#';
    for (std::size_t i = 1; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'F') {
            value[i] = static_cast<char>('a' + (value[i] - 'A'));
        }
    }
    return value;
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

std::string ms_label(double value_ms) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.2fs", value_ms / 1000.0);
    return buffer;
}

std::string layer_type_label(const mv::composition::layer& layer) {
    if (layer.source_data.type == "shape" && !layer.source_data.shape.empty()) {
        return layer.source_data.type + "/" + layer.source_data.shape;
    }
    return layer.source_data.type.empty() ? "unknown" : layer.source_data.type;
}

std::string next_layer_id(const mv::composition::mv_composition& composition, const std::string& prefix) {
    for (int index = static_cast<int>(composition.layers.size()) + 1; index < 10000; ++index) {
        const std::string id = prefix + "-" + std::to_string(index);
        const auto it = std::find_if(composition.layers.begin(), composition.layers.end(), [&](const auto& layer) {
            return layer.id == id;
        });
        if (it == composition.layers.end()) {
            return id;
        }
    }
    return prefix + "-fallback";
}

std::string next_effect_id(const mv::composition::mv_composition& composition, const std::string& prefix) {
    for (int index = 1; index < 10000; ++index) {
        const std::string id = prefix + "-" + std::to_string(index);
        bool exists = false;
        for (const mv::composition::layer& layer : composition.layers) {
            exists = std::any_of(layer.effects.begin(), layer.effects.end(), [&](const auto& effect) {
                return effect.id == id;
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
    const auto it = std::find_if(composition.layers.begin(), composition.layers.end(), [&](const auto& layer) {
        return layer.id == layer_id;
    });
    return it == composition.layers.end()
        ? static_cast<std::size_t>(-1)
        : static_cast<std::size_t>(std::distance(composition.layers.begin(), it));
}

int hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + ch - 'a';
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + ch - 'A';
    }
    return -1;
}

unsigned char hex_byte(const std::string& value, std::size_t index, unsigned char fallback) {
    if (index + 1 >= value.size()) {
        return fallback;
    }
    const int high = hex_digit(value[index]);
    const int low = hex_digit(value[index + 1]);
    if (high < 0 || low < 0) {
        return fallback;
    }
    return static_cast<unsigned char>((high << 4) | low);
}

Color parse_color(const std::string& value, float opacity = 1.0f) {
    Color result = g_theme->text;
    if (value.size() == 7 && value[0] == '#') {
        result.r = hex_byte(value, 1, result.r);
        result.g = hex_byte(value, 3, result.g);
        result.b = hex_byte(value, 5, result.b);
    }
    result.a = static_cast<unsigned char>(std::clamp(opacity, 0.0f, 1.0f) * 255.0f);
    return result;
}

Color with_opacity(Color color, float opacity) {
    color.a = static_cast<unsigned char>(std::clamp(opacity, 0.0f, 1.0f) * 255.0f);
    return color;
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
    for (mv::composition::layer& layer : composition.layers) {
        if (layer.start_ms == 0.0 &&
            (layer.duration_ms <= 0.0 ||
             layer.id == "layer-background" ||
             (layer.id == "layer-title" && layer.duration_ms == kDefaultFallbackDurationMs))) {
            layer.duration_ms = song_duration_ms;
        }
    }
    return true;
}

Vector2 preview_position(Rectangle preview, const mv::composition::mv_composition& composition,
                         const mv::composition::transform& transform) {
    const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
    const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
    return {
        preview.x + transform.position_x / canvas_w * preview.width,
        preview.y + transform.position_y / canvas_h * preview.height,
    };
}

void draw_preview_layer(Rectangle preview, const mv::composition::mv_composition& composition,
                        const mv::composition::layer& layer, bool selected, double visual_time_ms,
                        const Texture2D* texture = nullptr,
                        const std::array<float, 256>* waveform_samples = nullptr,
                        const std::array<float, 128>* spectrum = nullptr) {
    const auto& source = layer.source_data;
    const auto& transform = layer.transform_data;
    const Color fill = parse_color(source.fill.empty() ? composition.canvas_data.background : source.fill,
                                   transform.opacity);
    if (source.type == "background") {
        ui::draw_rect_f(preview, fill);
        return;
    }

    const Vector2 position = preview_position(preview, composition, transform);
    if (source.type == "text") {
        const std::string text = source.text.empty() ? "Text" : source.text;
        const float font_size = std::clamp(44.0f * transform.scale_y * (preview.height / 1080.0f), 10.0f, 72.0f);
        const Vector2 size = MeasureTextEx(GetFontDefault(), text.c_str(), font_size, 1.0f);
        const Vector2 origin = {size.x * transform.anchor_x, size.y * transform.anchor_y};
        DrawTextPro(GetFontDefault(), text.c_str(), position, origin, transform.rotation_deg, font_size, 1.0f, fill);
        if (selected) {
            DrawRectangleLinesEx({position.x - origin.x, position.y - origin.y, size.x, size.y}, 1.5f,
                                 g_theme->border_active);
        }
        return;
    }

    if (source.type == "shape" && (source.shape.empty() || source.shape == "rect")) {
        const float rect_w = 480.0f * transform.scale_x / std::max(1.0f, static_cast<float>(composition.canvas_data.width))
                             * preview.width;
        const float rect_h = 270.0f * transform.scale_y / std::max(1.0f, static_cast<float>(composition.canvas_data.height))
                             * preview.height;
        const Rectangle rect = {position.x, position.y, rect_w, rect_h};
        const Vector2 origin = {rect_w * transform.anchor_x, rect_h * transform.anchor_y};
        DrawRectanglePro(rect, origin, transform.rotation_deg, fill);
        if (selected) {
            DrawRectangleLinesEx({position.x - origin.x, position.y - origin.y, rect_w, rect_h}, 1.5f,
                                 g_theme->border_active);
        }
        return;
    }

    if (source.type == "image" && texture != nullptr && texture->id != 0) {
        const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
        const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
        const float rect_w = static_cast<float>(texture->width) * transform.scale_x / canvas_w * preview.width;
        const float rect_h = static_cast<float>(texture->height) * transform.scale_y / canvas_h * preview.height;
        const Rectangle src = {0.0f, 0.0f, static_cast<float>(texture->width), static_cast<float>(texture->height)};
        const Rectangle dest = {position.x, position.y, rect_w, rect_h};
        const Vector2 origin = {rect_w * transform.anchor_x, rect_h * transform.anchor_y};
        DrawTexturePro(*texture, src, dest, origin, transform.rotation_deg, fill);
        if (selected) {
            DrawRectangleLinesEx({position.x - origin.x, position.y - origin.y, rect_w, rect_h}, 1.5f,
                                 g_theme->border_active);
        }
        return;
    }

    if (source.type == "beatGrid") {
        const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
        const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
        const float rect_w = 1280.0f * transform.scale_x / canvas_w * preview.width;
        const float rect_h = 720.0f * transform.scale_y / canvas_h * preview.height;
        const Rectangle area = {position.x - rect_w * transform.anchor_x,
                                position.y - rect_h * transform.anchor_y,
                                rect_w, rect_h};
        const Color base = parse_color(source.fill.empty() ? "#8b7cf6" : source.fill);
        const Color minor = with_opacity(base, 0.18f * transform.opacity);
        const Color major = with_opacity(base, 0.38f * transform.opacity);
        const float phase_x = static_cast<float>(std::fmod(std::max(0.0, visual_time_ms - layer.start_ms), 500.0) / 500.0);
        const float phase_y = static_cast<float>(std::fmod(std::max(0.0, visual_time_ms - layer.start_ms), 1000.0) / 1000.0);
        const float cell_w = area.width / 16.0f;
        const float cell_h = area.height / 8.0f;
        for (int i = 0; i <= 16; ++i) {
            const float x = area.x + std::fmod((static_cast<float>(i) - phase_x) * cell_w + area.width, area.width);
            DrawLineEx({x, area.y}, {x, area.y + area.height}, i % 4 == 0 ? 2.0f : 1.0f,
                       i % 4 == 0 ? major : minor);
        }
        for (int i = 0; i <= 8; ++i) {
            const float y = area.y + std::fmod((static_cast<float>(i) - phase_y) * cell_h + area.height, area.height);
            DrawLineEx({area.x, y}, {area.x + area.width, y}, i % 4 == 0 ? 2.0f : 1.0f,
                       i % 4 == 0 ? major : minor);
        }
        if (selected) {
            DrawRectangleLinesEx(area, 1.5f, g_theme->border_active);
        }
        return;
    }

    if (source.type == "waveform") {
        const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
        const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
        const float rect_w = 1280.0f * transform.scale_x / canvas_w * preview.width;
        const float rect_h = 280.0f * transform.scale_y / canvas_h * preview.height;
        const Rectangle area = {position.x - rect_w * transform.anchor_x,
                                position.y - rect_h * transform.anchor_y,
                                rect_w, rect_h};
        const Color base = parse_color(source.fill.empty() ? "#6ee7b7" : source.fill);
        const Color line = with_opacity(base, 0.78f * transform.opacity);
        const Color shadow = with_opacity(base, 0.20f * transform.opacity);
        const float center_y = area.y + area.height * 0.5f;
        Vector2 previous = {area.x, center_y};
        constexpr int kSegments = 96;
        for (int i = 0; i <= kSegments; ++i) {
            const float ratio = static_cast<float>(i) / static_cast<float>(kSegments);
            const float x = area.x + ratio * area.width;
            float wave = 0.0f;
            float envelope = 1.0f;
            if (waveform_samples != nullptr) {
                const std::size_t sample_index = std::min(
                    waveform_samples->size() - 1,
                    static_cast<std::size_t>(ratio * static_cast<float>(waveform_samples->size() - 1)));
                wave = std::clamp((*waveform_samples)[sample_index], -1.0f, 1.0f);
            } else {
                const float phase = static_cast<float>((visual_time_ms - layer.start_ms) / 320.0);
                envelope = 0.25f + 0.75f * std::sin((ratio * 3.0f + 0.15f) * 3.1415926f);
                wave = std::sin(ratio * 42.0f + phase) * 0.55f +
                       std::sin(ratio * 19.0f - phase * 0.6f) * 0.35f;
            }
            const float y = center_y - wave * envelope * area.height * 0.38f;
            const Vector2 current = {x, y};
            if (i > 0) {
                DrawLineEx(previous, current, 5.0f, shadow);
                DrawLineEx(previous, current, 2.0f, line);
            }
            previous = current;
        }
        if (selected) {
            DrawRectangleLinesEx(area, 1.5f, g_theme->border_active);
        }
        return;
    }

    if (source.type == "spectrum") {
        const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
        const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
        const float rect_w = 1280.0f * transform.scale_x / canvas_w * preview.width;
        const float rect_h = 420.0f * transform.scale_y / canvas_h * preview.height;
        const Rectangle area = {position.x - rect_w * transform.anchor_x,
                                position.y - rect_h * transform.anchor_y,
                                rect_w, rect_h};
        const Color base = parse_color(source.fill.empty() ? "#38bdf8" : source.fill);
        const Color bar = with_opacity(base, 0.72f * transform.opacity);
        const Color peak = with_opacity(base, 0.95f * transform.opacity);
        constexpr int kBars = 32;
        const float gap = 3.0f;
        const float bar_w = std::max(2.0f, (area.width - gap * static_cast<float>(kBars - 1)) /
                                               static_cast<float>(kBars));
        for (int i = 0; i < kBars; ++i) {
            const float ratio = static_cast<float>(i) / static_cast<float>(kBars - 1);
            float normalized = 0.0f;
            if (spectrum != nullptr) {
                const std::size_t start = static_cast<std::size_t>(i) * spectrum->size() / kBars;
                const std::size_t end = std::max(start + 1, static_cast<std::size_t>(i + 1) * spectrum->size() / kBars);
                float sum = 0.0f;
                for (std::size_t j = start; j < end && j < spectrum->size(); ++j) {
                    sum += std::sqrt(std::max(0.0f, (*spectrum)[j])) * 8.0f;
                }
                normalized = std::clamp(sum / static_cast<float>(std::max<std::size_t>(1, end - start)), 0.04f, 1.0f);
            } else {
                const float phase = static_cast<float>((visual_time_ms - layer.start_ms) / 260.0);
                const float bass_bias = 1.0f - ratio * 0.45f;
                const float wave = 0.45f + 0.35f * std::sin(phase + ratio * 9.0f) +
                                   0.20f * std::sin(phase * 0.55f + ratio * 27.0f);
                normalized = std::clamp(wave * bass_bias, 0.08f, 1.0f);
            }
            const float height = normalized * area.height;
            const float x = area.x + static_cast<float>(i) * (bar_w + gap);
            const Rectangle rect = {x, area.y + area.height - height, bar_w, height};
            DrawRectangleRec(rect, bar);
            DrawRectangleRec({rect.x, rect.y, rect.width, std::max(2.0f, rect.height * 0.08f)}, peak);
        }
        if (selected) {
            DrawRectangleLinesEx(area, 1.5f, g_theme->border_active);
        }
    }
}

void draw_section_title(Rectangle rect, const char* title, const char* subtitle = nullptr) {
    ui::draw_text_in_rect(title, 16, {rect.x + 18.0f, rect.y, rect.width - 36.0f, 30.0f},
                          g_theme->text, ui::text_align::left);
    if (subtitle != nullptr) {
        ui::draw_text_in_rect(subtitle, 12, {rect.x + 18.0f, rect.y + 26.0f, rect.width - 36.0f, 24.0f},
                              g_theme->text_muted, ui::text_align::left);
    }
}

const char* workspace_label(mv_editor_scene::workspace value) {
    switch (value) {
        case mv_editor_scene::workspace::compose: return "Compose";
        case mv_editor_scene::workspace::timeline: return "Timeline";
        case mv_editor_scene::workspace::assets: return "Assets";
        case mv_editor_scene::workspace::effects: return "Effects";
        case mv_editor_scene::workspace::events: return "Events";
    }
    return "Compose";
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
    if (!composition_.layers.empty()) {
        selected_layer_id_ = composition_.layers.back().id;
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
    ui::begin_hit_regions();

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

    if (!inspector_text_input_active() && !name_input_.active && !author_input_.active) {
        const bool ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const bool shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (ctrl_down && IsKeyPressed(KEY_S)) {
            save_mv();
        } else if (ctrl_down && !shift_down && IsKeyPressed(KEY_Z)) {
            undo_edit();
        } else if (ctrl_down && (IsKeyPressed(KEY_Y) || (shift_down && IsKeyPressed(KEY_Z)))) {
            redo_edit();
        }
    }
}

void mv_editor_scene::draw() {
    virtual_screen::begin_ui();
    draw_scene_background(*g_theme);
    ui::begin_draw_queue();

    const Rectangle header = {0, 0, static_cast<float>(kScreenWidth), kHeaderHeight};
    ui::draw_rect_f(header, g_theme->section);
    ui::draw_rect_lines(header, 1.5f, g_theme->border_light);

    if (ui::draw_button_colored(metadata_button_rect(), "Metadata", 14,
                                metadata_modal_open_ ? g_theme->row_selected : g_theme->row,
                                metadata_modal_open_ ? g_theme->row_active : g_theme->row_hover,
                                g_theme->text).clicked) {
        metadata_modal_open_ = true;
        metadata_modal_open_anim_ = 0.0f;
        name_input_.active = false;
        author_input_.active = false;
    }

    const float header_right = static_cast<float>(kScreenWidth) - kPadding;
    Rectangle back_btn = {header_right - kBackButtonWidth, (kHeaderHeight - kHeaderButtonHeight) * 0.5f,
                          kBackButtonWidth, kHeaderButtonHeight};
    constexpr float kHistoryButtonWidth = 118.0f;
    Rectangle save_btn = {back_btn.x - kHeaderButtonWidth - 12.0f, back_btn.y, kHeaderButtonWidth, kHeaderButtonHeight};
    Rectangle redo_btn = {save_btn.x - kHistoryButtonWidth - 12.0f, back_btn.y, kHistoryButtonWidth, kHeaderButtonHeight};
    Rectangle undo_btn = {redo_btn.x - kHistoryButtonWidth - 8.0f, back_btn.y, kHistoryButtonWidth, kHeaderButtonHeight};
    Rectangle play_btn = {undo_btn.x - kHeaderButtonWidth - 12.0f, back_btn.y, kHeaderButtonWidth, kHeaderButtonHeight};

    if (ui::draw_button(play_btn, preview_playing_ ? "Pause" : "Play", 14).clicked) {
        set_preview_playing(!preview_playing_);
    }
    if (ui::draw_button_colored(undo_btn, "Undo", 13,
                                history_.can_undo() ? g_theme->row : with_alpha(g_theme->row, 110),
                                g_theme->row_hover, g_theme->text, 1.5f).clicked) {
        undo_edit();
    }
    if (ui::draw_button_colored(redo_btn, "Redo", 13,
                                history_.can_redo() ? g_theme->row : with_alpha(g_theme->row, 110),
                                g_theme->row_hover, g_theme->text, 1.5f).clicked) {
        redo_edit();
    }
    if (ui::draw_button(save_btn, dirty_ ? "Save *" : "Saved", 14).clicked) {
        save_mv();
    }
    if (ui::draw_button(back_btn, "Back", 14).clicked) {
        manager_.change_scene(song_select::make_seamless_create_scene(manager_, song_.meta.song_id));
        ui::flush_draw_queue();
        virtual_screen::end();
        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    const std::string title = package_.meta.name.empty() ? song_.meta.title + " MV" : package_.meta.name;
    ui::draw_text_in_rect(title.c_str(), 20,
                          {metadata_button_rect().x + metadata_button_rect().width + 24.0f, 12.0f,
                           play_btn.x - metadata_button_rect().x - metadata_button_rect().width - 48.0f, 28.0f},
                          g_theme->text, ui::text_align::left);
    const std::string subtitle = song_.meta.title + " / " + song_.meta.artist + "   " + ms_label(playhead_ms_);
    ui::draw_text_in_rect(subtitle.c_str(), 13,
                          {metadata_button_rect().x + metadata_button_rect().width + 24.0f, 38.0f,
                           play_btn.x - metadata_button_rect().x - metadata_button_rect().width - 48.0f, 22.0f},
                          g_theme->text_muted, ui::text_align::left);

    const Rectangle tab_strip = {
        kPadding,
        kHeaderHeight + 12.0f,
        static_cast<float>(kScreenWidth) - kPadding * 2.0f,
        kWorkspaceTabHeight
    };
    constexpr float kTabWidth = 138.0f;
    constexpr workspace kWorkspaces[] = {
        workspace::compose,
        workspace::timeline,
        workspace::assets,
        workspace::effects,
        workspace::events,
    };
    float tab_x = tab_strip.x;
    for (workspace item : kWorkspaces) {
        const Rectangle tab_rect = {tab_x, tab_strip.y, kTabWidth, tab_strip.height};
        const bool active = item == current_workspace_;
        if (ui::draw_button_colored(tab_rect,
                                    workspace_label(item),
                                    13,
                                    active ? g_theme->row_selected : g_theme->row,
                                    active ? g_theme->row_active : g_theme->row_hover,
                                    active ? g_theme->text : g_theme->text_muted,
                                    active ? 2.0f : 1.5f).clicked) {
            current_workspace_ = item;
        }
        tab_x += kTabWidth + kWorkspaceTabGap;
    }

    const Rectangle content = {
        kPadding, tab_strip.y + tab_strip.height + 12.0f,
        static_cast<float>(kScreenWidth) - kPadding * 2.0f,
        static_cast<float>(kScreenHeight) - (tab_strip.y + tab_strip.height + 12.0f) - kPadding
    };
    const auto left_split = ui::split_columns(content, kLayerPanelWidth, kPanelGap);
    const auto right_split = ui::split_columns(left_split.second,
                                              left_split.second.width - kInspectorWidth - kPanelGap,
                                              kPanelGap);
    const Rectangle layers_panel = left_split.first;
    const Rectangle center_area = right_split.first;
    const Rectangle inspector_panel = right_split.second;
    const float max_timeline_height = std::max(kTimelineHeight, center_area.height - 220.0f);
    const float workspace_timeline_height = current_workspace_ == workspace::timeline
        ? std::clamp(center_area.height * 0.68f, kTimelineHeight, max_timeline_height)
        : kTimelineHeight;
    const auto center_rows = ui::split_rows(center_area,
                                            center_area.height - workspace_timeline_height - kPanelGap,
                                            kPanelGap);
    const Rectangle preview_panel = center_rows.first;
    const Rectangle timeline_panel = center_rows.second;

    ui::draw_panel(layers_panel);
    if (current_workspace_ == workspace::assets) {
        draw_section_title(layers_panel, "Assets", "Package media used by this MV");
        const Rectangle import_btn = {layers_panel.x + 18.0f, layers_panel.y + 58.0f,
                                      layers_panel.width - 36.0f, 40.0f};
        if (ui::draw_button(import_btn, "+ Image Layer", 13).clicked) {
            add_image_layer();
        }
        const Rectangle asset_view = {layers_panel.x + 14.0f, layers_panel.y + 116.0f,
                                      layers_panel.width - 28.0f, layers_panel.height - 132.0f};
        ui::scoped_clip_rect clip(asset_view);
        float y = asset_view.y;
        if (composition_.assets.empty()) {
            ui::draw_text_in_rect("No assets", 14,
                                  {asset_view.x + 10.0f, y, asset_view.width - 20.0f, 32.0f},
                                  g_theme->text_muted, ui::text_align::left);
        }
        for (const mv::composition::asset_ref& asset : composition_.assets) {
            const Rectangle row = {asset_view.x, y, asset_view.width, 58.0f};
            ui::draw_rect_f(row, g_theme->section);
            ui::draw_rect_lines(row, 1.0f, g_theme->border_light);
            const int uses = static_cast<int>(std::count_if(
                composition_.layers.begin(), composition_.layers.end(), [&](const mv::composition::layer& layer) {
                    return layer.source_data.asset_id == asset.id;
                }));
            ui::draw_text_in_rect(asset.id.c_str(), 13,
                                  {row.x + 12.0f, row.y + 6.0f, row.width - 24.0f, 22.0f},
                                  g_theme->text, ui::text_align::left);
            const std::string meta = asset.type + "   uses " + std::to_string(uses);
            ui::draw_text_in_rect(meta.c_str(), 11,
                                  {row.x + 12.0f, row.y + 29.0f, row.width - 24.0f, 20.0f},
                                  g_theme->text_muted, ui::text_align::left);
            y += 66.0f;
        }
    } else if (current_workspace_ == workspace::effects) {
        draw_section_title(layers_panel, "Effects", "Selected layer effect chain");
        mv::composition::layer* layer = selected_layer();
        if (layer == nullptr) {
            ui::draw_text_in_rect("No layer selected", 14,
                                  {layers_panel.x + 18.0f, layers_panel.y + 62.0f,
                                   layers_panel.width - 36.0f, 32.0f},
                                  g_theme->text_muted, ui::text_align::left);
        } else {
            const float button_w = (layers_panel.width - 36.0f - 8.0f) * 0.5f;
            Rectangle fade_btn = {layers_panel.x + 18.0f, layers_panel.y + 58.0f, button_w, 36.0f};
            Rectangle pulse_btn = {fade_btn.x + fade_btn.width + 8.0f, fade_btn.y, button_w, 36.0f};
            Rectangle flash_btn = {fade_btn.x, fade_btn.y + 44.0f, button_w, 36.0f};
            Rectangle shake_btn = {pulse_btn.x, flash_btn.y, button_w, 36.0f};
            Rectangle clear_btn = {fade_btn.x, flash_btn.y + 44.0f, layers_panel.width - 36.0f, 34.0f};
            if (ui::draw_button(fade_btn, "+ Fade", 12).clicked) {
                add_fade_effect_to_selected_layer();
                layer = selected_layer();
            }
            if (ui::draw_button(pulse_btn, "+ Pulse", 12).clicked) {
                add_pulse_effect_to_selected_layer();
                layer = selected_layer();
            }
            if (ui::draw_button(flash_btn, "+ Flash", 12).clicked) {
                add_flash_effect_to_selected_layer();
                layer = selected_layer();
            }
            if (ui::draw_button(shake_btn, "+ Shake", 12).clicked) {
                add_shake_effect_to_selected_layer();
                layer = selected_layer();
            }
            if (ui::draw_button_colored(clear_btn, "Clear Effects", 12,
                                        layer != nullptr && !layer->effects.empty()
                                            ? g_theme->row
                                            : with_alpha(g_theme->row, 110),
                                        g_theme->row_hover, g_theme->text, 1.5f).clicked &&
                layer != nullptr && !layer->effects.empty()) {
                clear_selected_layer_effects();
                layer = selected_layer();
            }
            const Rectangle effect_view = {layers_panel.x + 14.0f, layers_panel.y + 186.0f,
                                           layers_panel.width - 28.0f, layers_panel.height - 202.0f};
            ui::scoped_clip_rect clip(effect_view);
            float y = effect_view.y;
            if (layer != nullptr && layer->effects.empty()) {
                ui::draw_text_in_rect("No effects", 14,
                                      {effect_view.x + 10.0f, y, effect_view.width - 20.0f, 32.0f},
                                      g_theme->text_muted, ui::text_align::left);
            }
            if (layer != nullptr) {
                for (const mv::composition::effect& effect : layer->effects) {
                    const Rectangle row = {effect_view.x, y, effect_view.width, 54.0f};
                    ui::draw_rect_f(row, g_theme->section);
                    ui::draw_rect_lines(row, 1.0f, g_theme->border_light);
                    ui::draw_text_in_rect(effect.type.c_str(), 14,
                                          {row.x + 12.0f, row.y + 6.0f, row.width - 24.0f, 22.0f},
                                          g_theme->text, ui::text_align::left);
                    const std::string meta = effect.target + "   " + std::to_string(effect.amount).substr(0, 5);
                    ui::draw_text_in_rect(meta.c_str(), 11,
                                          {row.x + 12.0f, row.y + 29.0f, row.width - 24.0f, 19.0f},
                                          g_theme->text_muted, ui::text_align::left);
                    y += 62.0f;
                }
            }
        }
    } else if (current_workspace_ == workspace::events) {
        draw_section_title(layers_panel, "Events", "Timeline cues for selected layer");
        const float button_w = (layers_panel.width - 36.0f - 8.0f) * 0.5f;
        Rectangle flash_btn = {layers_panel.x + 18.0f, layers_panel.y + 58.0f, button_w, 36.0f};
        Rectangle show_btn = {flash_btn.x + flash_btn.width + 8.0f, flash_btn.y, button_w, 36.0f};
        Rectangle text_btn = {flash_btn.x, flash_btn.y + 44.0f, button_w, 36.0f};
        Rectangle clear_btn = {show_btn.x, text_btn.y, button_w, 36.0f};
        if (ui::draw_button(flash_btn, "Cue Flash", 11).clicked) {
            add_flash_event_trigger_at_playhead();
        }
        if (ui::draw_button(show_btn, "Cue Show", 11).clicked) {
            add_show_event_trigger_at_playhead();
        }
        if (ui::draw_button(text_btn, "Cue Text", 11).clicked) {
            add_text_event_trigger_at_playhead();
        }
        const int cues_at_playhead = event_trigger_count_at_playhead();
        if (ui::draw_button_colored(clear_btn, "Clear Cue", 11,
                                    cues_at_playhead > 0 ? g_theme->row : with_alpha(g_theme->row, 110),
                                    g_theme->row_hover, g_theme->text, 1.5f).clicked &&
            cues_at_playhead > 0) {
            clear_event_triggers_at_playhead();
        }
        const Rectangle cue_view = {layers_panel.x + 14.0f, layers_panel.y + 156.0f,
                                    layers_panel.width - 28.0f, layers_panel.height - 172.0f};
        ui::scoped_clip_rect clip(cue_view);
        float y = cue_view.y;
        int cue_count = 0;
        for (const mv::composition::layer& cue_layer : composition_.layers) {
            for (const mv::composition::event_trigger& trigger : cue_layer.event_triggers) {
                const Rectangle row = {cue_view.x, y, cue_view.width, 58.0f};
                ui::draw_rect_f(row, cue_layer.id == selected_layer_id_ ? g_theme->row_selected : g_theme->section);
                ui::draw_rect_lines(row, 1.0f, g_theme->border_light);
                const std::string title = trigger.time_ms >= 0.0
                    ? ms_label(trigger.time_ms)
                    : (trigger.event.empty() ? "Event" : trigger.event);
                ui::draw_text_in_rect(title.c_str(), 13,
                                      {row.x + 12.0f, row.y + 6.0f, row.width - 24.0f, 22.0f},
                                      g_theme->text, ui::text_align::left);
                const std::string meta = cue_layer.name + "   actions " + std::to_string(trigger.actions.size());
                ui::draw_text_in_rect(meta.c_str(), 11,
                                      {row.x + 12.0f, row.y + 29.0f, row.width - 24.0f, 20.0f},
                                      g_theme->text_muted, ui::text_align::left);
                if (ui::is_hovered(row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    selected_layer_id_ = cue_layer.id;
                    if (trigger.time_ms >= 0.0) {
                        playhead_ms_ = std::clamp(trigger.time_ms, 0.0, composition_duration_ms());
                        set_preview_playing(false);
                        seek_preview_audio_to_playhead();
                    }
                }
                y += 66.0f;
                ++cue_count;
            }
        }
        if (cue_count == 0) {
            ui::draw_text_in_rect("No cues", 14,
                                  {cue_view.x + 10.0f, cue_view.y, cue_view.width - 20.0f, 32.0f},
                                  g_theme->text_muted, ui::text_align::left);
        }
    } else {
        draw_section_title(layers_panel, "Layers", "Source stack for this song MV");
        const float add_gap = 6.0f;
        const float add_button_width = (layers_panel.width - 36.0f - add_gap * 4.0f) / 5.0f;
        Rectangle add_text = {layers_panel.x + 18.0f, layers_panel.y + 56.0f, add_button_width, 38.0f};
        Rectangle add_rect = {add_text.x + add_text.width + add_gap, add_text.y, add_button_width, 38.0f};
        Rectangle add_image = {add_rect.x + add_rect.width + add_gap, add_text.y, add_button_width, 38.0f};
        Rectangle add_grid = {add_image.x + add_image.width + add_gap, add_text.y, add_button_width, 38.0f};
        Rectangle add_wave = {add_grid.x + add_grid.width + add_gap, add_text.y, add_button_width, 38.0f};
        if (ui::draw_button(add_text, "+ Txt", 12).clicked) {
            add_text_layer();
        }
        if (ui::draw_button(add_rect, "+ Rect", 12).clicked) {
            add_rect_layer();
        }
        if (ui::draw_button(add_image, "+ Img", 12).clicked) {
            add_image_layer();
        }
        if (ui::draw_button(add_grid, "+ Grid", 12).clicked) {
            add_beat_grid_layer();
        }
        if (ui::draw_button(add_wave, "+ Wave", 12).clicked) {
            add_waveform_layer();
        }
        const float preset_button_width = (layers_panel.width - 36.0f - add_gap * 2.0f) / 3.0f;
        Rectangle preset_flash = {layers_panel.x + 18.0f, layers_panel.y + 100.0f, preset_button_width, 34.0f};
        Rectangle preset_lyric = {preset_flash.x + preset_flash.width + add_gap, preset_flash.y,
                                  preset_button_width, preset_flash.height};
        Rectangle preset_bass = {preset_lyric.x + preset_lyric.width + add_gap, preset_flash.y,
                                 preset_button_width, preset_flash.height};
        if (ui::draw_button(preset_flash, "Flash", 11).clicked) {
            apply_builtin_preset("chorusFlash");
        }
        if (ui::draw_button(preset_lyric, "Lyric", 11).clicked) {
            apply_builtin_preset("lyricPop");
        }
        if (ui::draw_button(preset_bass, "Bass", 11).clicked) {
            apply_builtin_preset("bassPulse");
        }
        Rectangle add_spectrum = {preset_bass.x - preset_button_width - add_gap, preset_bass.y + 40.0f,
                                  preset_button_width, 30.0f};
        if (ui::draw_button(add_spectrum, "+ Spec", 10).clicked) {
            add_spectrum_layer();
        }
        Rectangle layer_view = {layers_panel.x + 14.0f, layers_panel.y + 178.0f,
                                layers_panel.width - 28.0f, layers_panel.height - 192.0f};
        {
            ui::scoped_clip_rect clip(layer_view);
            float y = layer_view.y;
            for (const mv::composition::layer& layer : composition_.layers) {
                const std::size_t layer_index = layer_index_by_id(composition_, layer.id);
                const Rectangle row = {layer_view.x, y, layer_view.width, 54.0f};
                const bool selected = layer.id == selected_layer_id_;
                const auto state = ui::draw_selectable_row(row, selected, 1.5f);
                if (state.clicked) {
                    selected_layer_id_ = layer.id;
                    sync_inspector_inputs(layer);
                }
                const float reorder_button_width = selected ? 34.0f : 0.0f;
                const float reorder_gap = selected ? 6.0f : 0.0f;
                const float text_width = row.width - 24.0f - reorder_button_width * 2.0f - reorder_gap * 2.0f;
                ui::draw_text_in_rect(layer.name.c_str(), 14,
                                      {row.x + 12.0f, row.y + 6.0f, text_width, 22.0f},
                                      g_theme->text, ui::text_align::left);
                const std::string meta = layer_type_label(layer) + "   z " + std::to_string(layer.z);
                ui::draw_text_in_rect(meta.c_str(), 11,
                                      {row.x + 12.0f, row.y + 28.0f, text_width, 20.0f},
                                      g_theme->text_muted, ui::text_align::left);
                if (selected) {
                    const Rectangle up_btn = {row.x + row.width - 78.0f, row.y + 10.0f, 34.0f, 34.0f};
                    const Rectangle down_btn = {up_btn.x + up_btn.width + 6.0f, up_btn.y, 34.0f, 34.0f};
                    const bool can_move_up = layer_index != static_cast<std::size_t>(-1) &&
                                             layer_index + 1 < composition_.layers.size();
                    const bool can_move_down = layer_index != static_cast<std::size_t>(-1) && layer_index > 0;
                    if (ui::draw_button_colored(up_btn, "Up", 10,
                                                can_move_up ? g_theme->row : with_alpha(g_theme->row, 110),
                                                g_theme->row_hover, g_theme->text, 1.5f).clicked &&
                        can_move_up) {
                        move_selected_layer(1);
                    }
                    if (ui::draw_button_colored(down_btn, "Dn", 10,
                                                can_move_down ? g_theme->row : with_alpha(g_theme->row, 110),
                                                g_theme->row_hover, g_theme->text, 1.5f).clicked &&
                        can_move_down) {
                        move_selected_layer(-1);
                    }
                }
                y += 62.0f;
            }
        }
    }

    ui::draw_panel(preview_panel);
    draw_section_title(preview_panel, "Preview", "Composition at the current song time");
    const Rectangle preview_outer = {preview_panel.x + 18.0f, preview_panel.y + 58.0f,
                                     preview_panel.width - 36.0f, preview_panel.height - 76.0f};
    const float canvas_aspect = static_cast<float>(std::max(1, composition_.canvas_data.width)) /
                                static_cast<float>(std::max(1, composition_.canvas_data.height));
    Rectangle preview = preview_outer;
    if (preview.width / preview.height > canvas_aspect) {
        preview.width = preview.height * canvas_aspect;
        preview.x = preview_outer.x + (preview_outer.width - preview.width) * 0.5f;
    } else {
        preview.height = preview.width / canvas_aspect;
        preview.y = preview_outer.y + (preview_outer.height - preview.height) * 0.5f;
    }
    ui::draw_rect_f(preview_outer, with_alpha(g_theme->bg_alt, 180));
    ui::draw_rect_lines(preview_outer, 1.0f, g_theme->border_light);
    {
        ui::scoped_clip_rect clip(preview);
        ui::draw_rect_f(preview, parse_color(composition_.canvas_data.background));
        std::array<float, 128> spectrum = {};
        std::array<float, 256> waveform_samples = {};
        const bool has_spectrum = preview_audio_loaded_ &&
            audio_manager::instance().get_preview_fft256(spectrum);
        const bool has_waveform_samples = preview_audio_loaded_ &&
            audio_manager::instance().get_preview_oscilloscope256(waveform_samples);
        std::vector<const mv::composition::layer*> draw_layers;
        for (const auto& layer : composition_.layers) {
            if (layer_active_at(layer, playhead_ms_)) {
                draw_layers.push_back(&layer);
            }
        }
        std::sort(draw_layers.begin(), draw_layers.end(), [](const auto* left, const auto* right) {
            return left->z < right->z;
        });
        for (const auto* layer : draw_layers) {
            const Texture2D* texture = nullptr;
            if (layer->source_data.type == "image") {
                if (const mv::composition::asset_ref* asset = find_asset(layer->source_data.asset_id);
                    asset != nullptr) {
                    texture = texture_for_asset(*asset);
                }
            }
            mv::composition::layer evaluated_layer = *layer;
            evaluated_layer.transform_data = mv::composition::evaluate_transform(*layer, playhead_ms_);
            draw_preview_layer(preview, composition_, evaluated_layer, layer->id == selected_layer_id_,
                               playhead_ms_,
                               texture,
                               has_waveform_samples ? &waveform_samples : nullptr,
                               has_spectrum ? &spectrum : nullptr);
        }
    }
    ui::draw_rect_lines(preview, 1.5f, g_theme->border_active);

    ui::draw_panel(timeline_panel);
    draw_section_title(timeline_panel, "Timeline", "Layer spans and playhead");
    const double duration = composition_duration_ms();
    const std::string duration_text = ms_label(playhead_ms_) + " / " + ms_label(duration);
    ui::draw_text_in_rect(duration_text.c_str(), 13,
                          {timeline_panel.x + timeline_panel.width - 220.0f, timeline_panel.y + 16.0f,
                           196.0f, 24.0f}, g_theme->text_muted, ui::text_align::right);
    const Rectangle track_area = {timeline_panel.x + 18.0f, timeline_panel.y + 58.0f,
                                  timeline_panel.width - 36.0f, timeline_panel.height - 78.0f};
    ui::draw_rect_f(track_area, g_theme->section);
    ui::draw_rect_lines(track_area, 1.0f, g_theme->border_light);
    for (int i = 0; i <= 8; ++i) {
        const float x = track_area.x + track_area.width * (static_cast<float>(i) / 8.0f);
        ui::draw_rect_f({x, track_area.y, 1.0f, track_area.height}, g_theme->editor_grid_minor);
    }
    const Vector2 timeline_mouse = virtual_screen::get_virtual_mouse();
    const double safe_duration = std::max(1.0, duration);
    const double ms_per_pixel = safe_duration / std::max(1.0f, track_area.width);
    bool timeline_span_hit = false;
    bool timeline_dragging = timeline_drag_mode_ != timeline_drag_mode::none &&
                             IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    float row_y = track_area.y + 10.0f;
    for (const auto& layer : composition_.layers) {
        const float start_x = track_area.x + static_cast<float>(layer.start_ms / safe_duration) * track_area.width;
        const double end_ms = layer.duration_ms <= 0.0 ? duration : std::min(duration, layer.start_ms + layer.duration_ms);
        const float end_x = track_area.x + static_cast<float>(end_ms / safe_duration) * track_area.width;
        const Rectangle span = {start_x, row_y, std::max(3.0f, end_x - start_x), 16.0f};
        const bool selected = layer.id == selected_layer_id_;
        const bool locked = layer.locked;
        const Color span_color = locked
            ? with_alpha(g_theme->slider_fill, 95)
            : selected ? g_theme->accent : g_theme->slider_fill;
        ui::draw_rect_f(span, span_color);
        const Rectangle left_handle = {span.x - 4.0f, span.y - 3.0f, 8.0f, span.height + 6.0f};
        const Rectangle right_handle = {span.x + span.width - 4.0f, span.y - 3.0f, 8.0f, span.height + 6.0f};
        if (selected && !locked) {
            ui::draw_rect_f(left_handle, with_alpha(g_theme->border_active, 190));
            ui::draw_rect_f(right_handle, with_alpha(g_theme->border_active, 190));
        }
        if (ui::is_hovered(span) || ui::is_hovered(left_handle) || ui::is_hovered(right_handle)) {
            timeline_span_hit = true;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !locked) {
            timeline_drag_mode mode = timeline_drag_mode::none;
            if (ui::is_hovered(left_handle)) {
                mode = timeline_drag_mode::trim_start;
            } else if (ui::is_hovered(right_handle)) {
                mode = timeline_drag_mode::trim_end;
            } else if (ui::is_hovered(span)) {
                mode = timeline_drag_mode::move;
            }
            if (mode != timeline_drag_mode::none) {
                selected_layer_id_ = layer.id;
                timeline_drag_mode_ = mode;
                timeline_drag_layer_id_ = layer.id;
                timeline_drag_origin_mouse_x_ = timeline_mouse.x;
                timeline_drag_origin_start_ms_ = layer.start_ms;
                timeline_drag_origin_duration_ms_ = layer.duration_ms <= 0.0
                    ? std::max(250.0, end_ms - layer.start_ms)
                    : layer.duration_ms;
                set_preview_playing(false);
                timeline_dragging = true;
            }
        }
        for (const mv::composition::keyframe_track& track : layer.keyframes) {
            if (!mv::composition::is_transform_keyframe_target(track.target)) {
                continue;
            }
            for (const mv::composition::keyframe& point : track.points) {
                if (point.time_ms < 0.0 || point.time_ms > duration) {
                    continue;
                }
                const float marker_x = track_area.x +
                    static_cast<float>(point.time_ms / safe_duration) * track_area.width;
                const Rectangle marker = {marker_x - 2.5f, row_y - 3.0f, 5.0f, 22.0f};
                const Color marker_color = layer.id == selected_layer_id_
                    ? g_theme->border_active
                    : with_alpha(g_theme->text_muted, 170);
                ui::draw_rect_f(marker, marker_color);
            }
        }
        for (const mv::composition::event_trigger& trigger : layer.event_triggers) {
            if (trigger.time_ms < 0.0 || trigger.time_ms > duration) {
                continue;
            }
            const float marker_x = track_area.x +
                static_cast<float>(trigger.time_ms / safe_duration) * track_area.width;
            const Rectangle marker = {marker_x - 1.5f, row_y - 5.0f, 3.0f, 26.0f};
            const Color marker_color = layer.id == selected_layer_id_
                ? g_theme->success
                : with_alpha(g_theme->success, 135);
            ui::draw_rect_f(marker, marker_color);
        }
        row_y += 24.0f;
        if (row_y > track_area.y + track_area.height - 12.0f) {
            break;
        }
    }
    if (timeline_dragging) {
        if (mv::composition::layer* layer = selected_layer()) {
            const double delta_ms = (timeline_mouse.x - timeline_drag_origin_mouse_x_) * ms_per_pixel;
            const double original_start = timeline_drag_origin_start_ms_;
            const double original_duration = std::max(250.0, timeline_drag_origin_duration_ms_);
            if (timeline_drag_mode_ == timeline_drag_mode::move) {
                layer->start_ms = std::clamp(original_start + delta_ms, 0.0,
                                             std::max(0.0, safe_duration - original_duration));
                layer->duration_ms = original_duration;
                playhead_ms_ = layer->start_ms;
            } else if (timeline_drag_mode_ == timeline_drag_mode::trim_start) {
                const double original_end = std::min(safe_duration, original_start + original_duration);
                const double next_start = std::clamp(original_start + delta_ms, 0.0, original_end - 250.0);
                layer->start_ms = next_start;
                layer->duration_ms = std::max(250.0, original_end - next_start);
                playhead_ms_ = layer->start_ms;
            } else if (timeline_drag_mode_ == timeline_drag_mode::trim_end) {
                const double next_end = std::clamp(original_start + original_duration + delta_ms,
                                                   original_start + 250.0, safe_duration);
                layer->duration_ms = std::max(250.0, next_end - original_start);
                playhead_ms_ = original_start + layer->duration_ms;
            }
            dirty_ = true;
        }
    }
    if (timeline_drag_mode_ != timeline_drag_mode::none && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        commit_history("Edit Timing");
        timeline_drag_mode_ = timeline_drag_mode::none;
        timeline_drag_layer_id_.clear();
        seek_preview_audio_to_playhead();
        validate_composition();
    }
    if (!timeline_dragging &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        ui::is_hovered(track_area) &&
        !timeline_span_hit) {
        playhead_ms_ = std::clamp((timeline_mouse.x - track_area.x) / std::max(1.0f, track_area.width),
                                  0.0f, 1.0f) * duration;
        set_preview_playing(false);
        seek_preview_audio_to_playhead();
    }
    const float playhead_x = track_area.x + static_cast<float>(playhead_ms_ / std::max(1.0, duration)) * track_area.width;
    ui::draw_rect_f({playhead_x - 1.0f, track_area.y, 2.0f, track_area.height}, g_theme->border_active);

    ui::draw_panel(inspector_panel);
    draw_section_title(inspector_panel, "Inspector", "Selected layer properties");
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        ui::draw_text_in_rect("No layer selected", 16,
                              {inspector_panel.x + 18.0f, inspector_panel.y + 70.0f,
                               inspector_panel.width - 36.0f, 36.0f},
                              g_theme->text_muted, ui::text_align::left);
    } else {
        sync_inspector_inputs(*layer);
        Rectangle body = {inspector_panel.x + 18.0f, inspector_panel.y + 62.0f,
                          inspector_panel.width - 36.0f, inspector_panel.height - 80.0f};
        const auto layer_name_result =
            ui::draw_text_input({body.x, body.y, body.width, 40.0f}, layer_name_input_,
                                "Name", "Layer name", nullptr, ui::draw_layer::base,
                                14, 96, wide_text_filter, 78.0f);
        if (layer_name_result.changed) {
            layer->name = layer_name_input_.value.empty() ? "Layer" : layer_name_input_.value;
            dirty_ = true;
            inspector_edit_pending_ = true;
        }
        if ((layer_name_result.deactivated || layer_name_result.submitted) && inspector_edit_pending_) {
            commit_history("Edit Layer Name");
        }
        const std::string type = layer_type_label(*layer);
        ui::draw_text_in_rect(type.c_str(), 12, {body.x, body.y + 42.0f, body.width, 22.0f},
                              g_theme->text_muted, ui::text_align::left);
        Rectangle delete_btn = {body.x, body.y + 72.0f, 132.0f, 36.0f};
        if (ui::draw_button_colored(delete_btn, "Delete", 13, g_theme->row, g_theme->row_hover,
                                    g_theme->error, 1.5f).clicked) {
            delete_selected_layer();
            layer = selected_layer();
        }
        if (layer != nullptr) {
            bool inspector_changed = false;
            Rectangle visible_btn = {body.x + 144.0f, body.y + 72.0f, 96.0f, 36.0f};
            Rectangle lock_btn = {visible_btn.x + visible_btn.width + 10.0f, visible_btn.y, 96.0f, 36.0f};
            Rectangle key_btn = {body.x, body.y + 114.0f, (body.width - 10.0f) * 0.5f, 34.0f};
            Rectangle clear_key_btn = {key_btn.x + key_btn.width + 10.0f, key_btn.y, key_btn.width, key_btn.height};
            if (ui::draw_button_colored(visible_btn, layer->visible ? "Visible" : "Hidden", 13,
                                        layer->visible ? g_theme->row_selected : g_theme->row,
                                        g_theme->row_hover, g_theme->text, 1.5f).clicked) {
                layer->visible = !layer->visible;
                validate_composition();
                commit_history("Toggle Visibility");
                inspector_changed = false;
            }
            if (ui::draw_button_colored(lock_btn, layer->locked ? "Locked" : "Unlocked", 13,
                                        layer->locked ? g_theme->row_selected : g_theme->row,
                                        g_theme->row_hover, g_theme->text, 1.5f).clicked) {
                layer->locked = !layer->locked;
                validate_composition();
                commit_history("Toggle Lock");
                inspector_changed = false;
            }
            if (ui::draw_button(key_btn, "Set Key", 12, 1.5f).clicked) {
                key_selected_transform();
                layer = selected_layer();
                inspector_changed = false;
            }
            const int keys_at_playhead = transform_keyframe_count_at_playhead();
            const std::string clear_label = keys_at_playhead > 0
                ? "Clear Key (" + std::to_string(keys_at_playhead) + ")"
                : "Clear Key";
            if (ui::draw_button_colored(clear_key_btn, clear_label.c_str(), 12,
                                        keys_at_playhead > 0 ? g_theme->row : with_alpha(g_theme->row, 110),
                                        g_theme->row_hover, g_theme->text, 1.5f).clicked &&
                keys_at_playhead > 0) {
                delete_transform_keyframes_at_playhead();
                layer = selected_layer();
                inspector_changed = false;
            }

            const auto value_row = [&](float y, const char* label, const std::string& value) {
                ui::draw_rect_f({body.x, y, body.width, 34.0f}, g_theme->section);
                ui::draw_rect_lines({body.x, y, body.width, 34.0f}, 1.0f, g_theme->border_light);
                ui::draw_text_in_rect(label, 12, {body.x + 12.0f, y, 120.0f, 34.0f},
                                      g_theme->text_muted, ui::text_align::left);
                ui::draw_text_in_rect(value.c_str(), 12, {body.x + 134.0f, y, body.width - 146.0f, 34.0f},
                                      g_theme->text, ui::text_align::right);
            };
            const auto slider_value = [&](float y, const char* label, const std::string& value,
                                          float ratio) {
                return ui::draw_slider_relative({body.x, y, body.width, 38.0f},
                                                label, value.c_str(), ratio,
                                                120.0f, 80.0f, 12, 19.0f, 92.0f, 12.0f);
            };
            const double duration = composition_duration_ms();
            float changed = slider_value(body.y + 158.0f, "X",
                                         std::to_string(static_cast<int>(std::round(layer->transform_data.position_x))),
                                         layer->transform_data.position_x /
                                             static_cast<float>(std::max(1, composition_.canvas_data.width)));
            if (changed >= 0.0f) {
                layer->transform_data.position_x =
                    changed * static_cast<float>(std::max(1, composition_.canvas_data.width));
                dirty_ = true;
                inspector_changed = true;
            }
            changed = slider_value(body.y + 202.0f, "Y",
                                   std::to_string(static_cast<int>(std::round(layer->transform_data.position_y))),
                                   layer->transform_data.position_y /
                                       static_cast<float>(std::max(1, composition_.canvas_data.height)));
            if (changed >= 0.0f) {
                layer->transform_data.position_y =
                    changed * static_cast<float>(std::max(1, composition_.canvas_data.height));
                dirty_ = true;
                inspector_changed = true;
            }
            changed = slider_value(body.y + 246.0f, "Scale",
                                   std::to_string(layer->transform_data.scale_x).substr(0, 4),
                                   std::clamp((layer->transform_data.scale_x - 0.1f) / 2.9f, 0.0f, 1.0f));
            if (changed >= 0.0f) {
                const float scale = 0.1f + changed * 2.9f;
                layer->transform_data.scale_x = scale;
                layer->transform_data.scale_y = scale;
                dirty_ = true;
                inspector_changed = true;
            }
            changed = slider_value(body.y + 290.0f, "Opacity",
                                   std::to_string(static_cast<int>(std::round(layer->transform_data.opacity * 100.0f))) + "%",
                                   layer->transform_data.opacity);
            if (changed >= 0.0f) {
                layer->transform_data.opacity = changed;
                dirty_ = true;
                inspector_changed = true;
            }
            changed = slider_value(body.y + 334.0f, "Start", ms_label(layer->start_ms),
                                   static_cast<float>(layer->start_ms / duration));
            if (changed >= 0.0f) {
                layer->start_ms = changed * duration;
                playhead_ms_ = layer->start_ms;
                dirty_ = true;
                inspector_changed = true;
            }
            changed = slider_value(body.y + 378.0f, "Duration",
                                   layer->duration_ms <= 0.0 ? "Full" : ms_label(layer->duration_ms),
                                   static_cast<float>((layer->duration_ms <= 0.0 ? duration : layer->duration_ms) / duration));
            if (changed >= 0.0f) {
                layer->duration_ms = std::max(250.0, static_cast<double>(changed) * duration);
                dirty_ = true;
                inspector_changed = true;
            }
            const std::string keyframe_count = std::to_string(layer->keyframes.size()) + " track(s)";
            value_row(body.y + 428.0f, "Keyframes", keyframe_count);
            const std::string motion_count = std::to_string(layer->effects.size()) + " FX / " +
                                             std::to_string(layer->event_triggers.size()) + " cue(s)";
            value_row(body.y + 470.0f, "Motion", motion_count);
            const float fx_gap = 8.0f;
            const float fx_btn_width = (body.width - fx_gap * 2.0f) / 3.0f;
            Rectangle cue_flash_btn = {body.x, body.y + 512.0f, fx_btn_width, 30.0f};
            Rectangle cue_show_btn = {cue_flash_btn.x + fx_btn_width + fx_gap,
                                      cue_flash_btn.y, fx_btn_width, cue_flash_btn.height};
            Rectangle clear_cue_btn = {cue_show_btn.x + fx_btn_width + fx_gap,
                                       cue_flash_btn.y, fx_btn_width, cue_flash_btn.height};
            if (ui::draw_button(cue_flash_btn, "Cue Flash", 12, 1.5f).clicked) {
                add_flash_event_trigger_at_playhead();
                layer = selected_layer();
                inspector_changed = false;
            }
            const bool text_layer = layer->source_data.type == "text";
            if (ui::draw_button(cue_show_btn, text_layer ? "Cue Text" : "Cue Show", 12, 1.5f).clicked) {
                if (text_layer) {
                    add_text_event_trigger_at_playhead();
                } else {
                    add_show_event_trigger_at_playhead();
                }
                layer = selected_layer();
                inspector_changed = false;
            }
            const int cues_at_playhead = event_trigger_count_at_playhead();
            const std::string clear_cue_label = cues_at_playhead > 0
                ? "Clear Cue"
                : "No Cue";
            if (ui::draw_button_colored(clear_cue_btn, clear_cue_label.c_str(), 12,
                                        cues_at_playhead > 0 ? g_theme->row : with_alpha(g_theme->row, 110),
                                        g_theme->row_hover, g_theme->text, 1.5f).clicked &&
                cues_at_playhead > 0) {
                clear_event_triggers_at_playhead();
                layer = selected_layer();
                inspector_changed = false;
            }
            Rectangle fade_btn = {body.x, body.y + 548.0f, fx_btn_width, 30.0f};
            Rectangle pulse_btn = {fade_btn.x + fx_btn_width + fx_gap, fade_btn.y, fx_btn_width, fade_btn.height};
            Rectangle flash_btn = {pulse_btn.x + fx_btn_width + fx_gap, fade_btn.y, fx_btn_width, fade_btn.height};
            Rectangle shake_btn = {body.x, body.y + 584.0f, fx_btn_width, 30.0f};
            Rectangle clear_fx_btn = {shake_btn.x + fx_btn_width + fx_gap, shake_btn.y, fx_btn_width * 2.0f + fx_gap,
                                      shake_btn.height};
            if (ui::draw_button(fade_btn, "Fade", 12, 1.5f).clicked) {
                add_fade_effect_to_selected_layer();
                layer = selected_layer();
                inspector_changed = false;
            }
            if (ui::draw_button(pulse_btn, "Pulse", 12, 1.5f).clicked) {
                add_pulse_effect_to_selected_layer();
                layer = selected_layer();
                inspector_changed = false;
            }
            if (ui::draw_button(flash_btn, "Flash", 12, 1.5f).clicked) {
                add_flash_effect_to_selected_layer();
                layer = selected_layer();
                inspector_changed = false;
            }
            if (ui::draw_button(shake_btn, "Shake", 12, 1.5f).clicked) {
                add_shake_effect_to_selected_layer();
                layer = selected_layer();
                inspector_changed = false;
            }
            if (ui::draw_button_colored(clear_fx_btn, "Clear FX", 12,
                                        layer != nullptr && !layer->effects.empty() ? g_theme->row : with_alpha(g_theme->row, 110),
                                        g_theme->row_hover, g_theme->text, 1.5f).clicked) {
                clear_selected_layer_effects();
                layer = selected_layer();
                inspector_changed = false;
            }
            if (layer != nullptr) {
                float detail_y = body.y + 626.0f;
                auto find_effect = [&](const std::string& type) -> mv::composition::effect* {
                    auto it = std::find_if(layer->effects.begin(), layer->effects.end(), [&](const auto& effect) {
                        return effect.type == type;
                    });
                    return it == layer->effects.end() ? nullptr : &*it;
                };
                if (mv::composition::effect* fade = find_effect("fade")) {
                    const float amount = fade->amount <= 0.0f ? 650.0f : fade->amount;
                    changed = slider_value(detail_y, "Fade", ms_label(amount),
                                           std::clamp((amount - 100.0f) / 1900.0f, 0.0f, 1.0f));
                    if (changed >= 0.0f) {
                        fade->amount = 100.0f + changed * 1900.0f;
                        dirty_ = true;
                        inspector_changed = true;
                    }
                    detail_y += 44.0f;
                }
                if (mv::composition::effect* pulse = find_effect("pulse")) {
                    const float amount = std::clamp(pulse->amount <= 0.0f ? 0.08f : pulse->amount, 0.0f, 0.3f);
                    const std::string pulse_label =
                        std::to_string(static_cast<int>(std::round(amount * 100.0f))) + "%";
                    changed = slider_value(detail_y, "Pulse", pulse_label,
                                           std::clamp(amount / 0.3f, 0.0f, 1.0f));
                    if (changed >= 0.0f) {
                        pulse->amount = changed * 0.3f;
                        dirty_ = true;
                        inspector_changed = true;
                    }
                    detail_y += 44.0f;
                }
                if (mv::composition::effect* flash = find_effect("flash")) {
                    const float amount = std::clamp(flash->amount <= 0.0f ? 0.35f : flash->amount, 0.0f, 1.0f);
                    const std::string flash_label =
                        std::to_string(static_cast<int>(std::round(amount * 100.0f))) + "%";
                    changed = slider_value(detail_y, "Flash", flash_label, amount);
                    if (changed >= 0.0f) {
                        flash->amount = changed;
                        dirty_ = true;
                        inspector_changed = true;
                    }
                    detail_y += 44.0f;
                }
                if (mv::composition::effect* shake = find_effect("shake")) {
                    const float amount = std::clamp(shake->amount <= 0.0f ? 18.0f : shake->amount, 0.0f, 120.0f);
                    const std::string shake_label =
                        std::to_string(static_cast<int>(std::round(amount))) + "px";
                    changed = slider_value(detail_y, "Shake", shake_label, amount / 120.0f);
                    if (changed >= 0.0f) {
                        shake->amount = changed * 120.0f;
                        dirty_ = true;
                        inspector_changed = true;
                    }
                    detail_y += 44.0f;
                }
                const auto fill_result =
                    ui::draw_text_input({body.x, detail_y, body.width, 38.0f}, layer_fill_input_,
                                        "Fill", "#ffffff", "#ffffff", ui::draw_layer::base,
                                        12, 7, hex_color_filter, 92.0f);
                if (fill_result.changed) {
                    if (is_valid_hex_color(layer_fill_input_.value)) {
                        layer_fill_input_.value = normalize_hex_color(layer_fill_input_.value);
                        layer->source_data.fill = layer_fill_input_.value;
                        dirty_ = true;
                        inspector_edit_pending_ = true;
                    }
                }
                if ((fill_result.deactivated || fill_result.submitted) && !is_valid_hex_color(layer_fill_input_.value)) {
                    layer_fill_input_.value = layer->source_data.fill.empty() ? "#ffffff" : layer->source_data.fill;
                }
                if ((fill_result.deactivated || fill_result.submitted) && inspector_edit_pending_) {
                    commit_history("Edit Fill");
                }
                detail_y += 44.0f;
                if (layer->source_data.type == "text") {
                    const auto text_result =
                        ui::draw_text_input({body.x, detail_y, body.width, 38.0f}, layer_text_input_,
                                            "Text", "Text", "Text", ui::draw_layer::base,
                                            12, 160, wide_text_filter, 92.0f);
                    if (text_result.changed) {
                        layer->source_data.text = layer_text_input_.value;
                        dirty_ = true;
                        inspector_edit_pending_ = true;
                    }
                    if ((text_result.deactivated || text_result.submitted) && inspector_edit_pending_) {
                        commit_history("Edit Text");
                    }
                } else if (layer->source_data.type == "image") {
                    value_row(detail_y, "Asset", layer->source_data.asset_id);
                }
            }
            if (inspector_changed) {
                validate_composition();
                inspector_edit_pending_ = true;
            }
            if (inspector_edit_pending_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                commit_history("Edit Layer");
            }
        }
    }

    if (!diagnostics_.empty()) {
        const Rectangle diag = {inspector_panel.x + 18.0f,
                                inspector_panel.y + inspector_panel.height - 58.0f,
                                inspector_panel.width - 36.0f, 40.0f};
        ui::draw_rect_f(diag, with_alpha(g_theme->error, 50));
        ui::draw_rect_lines(diag, 1.0f, with_alpha(g_theme->error, 160));
        ui::draw_text_in_rect(diagnostics_.front().c_str(), 11, ui::inset(diag, 8.0f),
                              g_theme->text, ui::text_align::left);
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
            metadata_dirty_ = true;
            dirty_ = true;
        }
    }

    ui::flush_draw_queue();
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

void mv_editor_scene::save_mv() {
    if (inspector_edit_pending_) {
        commit_history("Edit Layer");
    }
    package_.meta.song_id = song_.meta.song_id;
    package_.meta.name = name_input_.value.empty() ? (song_.meta.title + " MV") : name_input_.value;
    package_.meta.author = author_input_.value;
    package_.meta.composition_file = "composition.rmvcomp";
    package_.meta.format_version = 1;
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
    if (selected_layer() == nullptr && !composition_.layers.empty()) {
        selected_layer_id_ = composition_.layers.back().id;
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
    inspector_input_layer_id_.clear();
}

void mv_editor_scene::sync_inspector_inputs(const mv::composition::layer& layer) {
    if (inspector_input_layer_id_ == layer.id) {
        return;
    }
    inspector_input_layer_id_ = layer.id;
    layer_name_input_ = {};
    layer_text_input_ = {};
    layer_fill_input_ = {};
    layer_name_input_.value = layer.name;
    layer_text_input_.value = layer.source_data.text;
    layer_fill_input_.value = layer.source_data.fill.empty() ? "#ffffff" : layer.source_data.fill;
}

bool mv_editor_scene::inspector_text_input_active() const {
    return layer_name_input_.active || layer_text_input_.active || layer_fill_input_.active;
}

void mv_editor_scene::add_text_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.layers.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-text");
    layer.name = "Text " + std::to_string(index);
    layer.z = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = 8000.0;
    layer.source_data.type = "text";
    layer.source_data.text = "Text";
    layer.source_data.fill = "#d8d4ff";
    layer.transform_data.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    layer.transform_data.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    composition_.layers.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Text");
}

void mv_editor_scene::add_rect_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.layers.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-rect");
    layer.name = "Rectangle " + std::to_string(index);
    layer.z = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = 8000.0;
    layer.source_data.type = "shape";
    layer.source_data.shape = "rect";
    layer.source_data.fill = "#6ee7b7";
    layer.transform_data.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    layer.transform_data.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    layer.transform_data.scale_x = 1.0f;
    layer.transform_data.scale_y = 1.0f;
    layer.transform_data.opacity = 0.75f;
    composition_.layers.push_back(layer);
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
    const int index = static_cast<int>(composition_.layers.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-image");
    layer.name = "Image " + std::to_string(index);
    layer.z = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = 8000.0;
    layer.source_data.type = "image";
    layer.source_data.asset_id = imported->id;
    layer.source_data.fill = "#ffffff";
    layer.transform_data.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    layer.transform_data.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    layer.transform_data.scale_x = 1.0f;
    layer.transform_data.scale_y = 1.0f;
    composition_.layers.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Image");
}

void mv_editor_scene::add_beat_grid_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.layers.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-beat-grid");
    layer.name = "Beat Grid " + std::to_string(index);
    layer.z = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = std::max(8000.0, composition_duration_ms() - playhead_ms_);
    layer.source_data.type = "beatGrid";
    layer.source_data.fill = "#8b7cf6";
    layer.transform_data.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    layer.transform_data.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    layer.transform_data.scale_x = 1.0f;
    layer.transform_data.scale_y = 1.0f;
    layer.transform_data.opacity = 0.8f;
    composition_.layers.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Beat Grid");
}

void mv_editor_scene::add_waveform_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.layers.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-waveform");
    layer.name = "Waveform " + std::to_string(index);
    layer.z = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = std::max(8000.0, composition_duration_ms() - playhead_ms_);
    layer.source_data.type = "waveform";
    layer.source_data.fill = "#6ee7b7";
    layer.transform_data.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    layer.transform_data.position_y = static_cast<float>(composition_.canvas_data.height) * 0.72f;
    layer.transform_data.scale_x = 1.0f;
    layer.transform_data.scale_y = 1.0f;
    layer.transform_data.opacity = 0.85f;
    composition_.layers.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Waveform");
}

void mv_editor_scene::add_spectrum_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.layers.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-spectrum");
    layer.name = "Spectrum " + std::to_string(index);
    layer.z = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = std::max(8000.0, composition_duration_ms() - playhead_ms_);
    layer.source_data.type = "spectrum";
    layer.source_data.fill = "#38bdf8";
    layer.transform_data.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    layer.transform_data.position_y = static_cast<float>(composition_.canvas_data.height) * 0.76f;
    layer.transform_data.scale_x = 1.0f;
    layer.transform_data.scale_y = 1.0f;
    layer.transform_data.opacity = 0.82f;
    composition_.layers.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Spectrum");
}

void mv_editor_scene::apply_builtin_preset(const std::string& preset_id) {
    const double start_ms = playhead_ms_;
    const mv::composition::preset_apply_result result =
        mv::composition::apply_preset(composition_, preset_id, start_ms, 0.0);
    if (!result.success) {
        diagnostics_ = {result.message.empty() ? "Failed to apply MV preset." : result.message};
        return;
    }
    if (!result.selected_layer_id.empty()) {
        selected_layer_id_ = result.selected_layer_id;
    }
    normalize_layer_z_order();
    reset_inspector_inputs();
    validate_composition();
    commit_history("Apply Preset");
}

void mv_editor_scene::key_selected_transform() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    const auto add_point = [&](const std::string& target, float value) {
        mv::composition::keyframe_track& track = mv::composition::ensure_keyframe_track(*layer, target);
        mv::composition::upsert_keyframe(track, {playhead_ms_, value, "linear"});
    };
    add_point("transform.position.x", layer->transform_data.position_x);
    add_point("transform.position.y", layer->transform_data.position_y);
    add_point("transform.scale.x", layer->transform_data.scale_x);
    add_point("transform.scale.y", layer->transform_data.scale_y);
    add_point("transform.rotationDeg", layer->transform_data.rotation_deg);
    add_point("transform.opacity", layer->transform_data.opacity);
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

void mv_editor_scene::add_flash_event_trigger_at_playhead() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    mv::composition::add_flash_cue(*layer, composition_, playhead_ms_, kKeyframeHitToleranceMs);
    validate_composition();
    commit_history("Add Flash Cue");
}

void mv_editor_scene::add_show_event_trigger_at_playhead() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    mv::composition::add_show_cue(*layer, playhead_ms_, kKeyframeHitToleranceMs);
    validate_composition();
    commit_history("Add Show Cue");
}

void mv_editor_scene::add_text_event_trigger_at_playhead() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr || layer->source_data.type != "text") {
        return;
    }
    const std::string cue_text =
        layer_text_input_.value.empty() ? (layer->source_data.text.empty() ? "Text" : layer->source_data.text)
                                        : layer_text_input_.value;
    mv::composition::add_text_cue(*layer, playhead_ms_, kKeyframeHitToleranceMs, cue_text);
    layer_text_input_.value = cue_text;
    validate_composition();
    commit_history("Add Text Cue");
}

void mv_editor_scene::clear_event_triggers_at_playhead() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    if (mv::composition::clear_timeline_cues_near(*layer, playhead_ms_, kKeyframeHitToleranceMs) <= 0) {
        return;
    }
    validate_composition();
    commit_history("Clear Cue");
}

int mv_editor_scene::event_trigger_count_at_playhead() const {
    const mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return 0;
    }
    return mv::composition::count_timeline_cues_near(*layer, playhead_ms_, kKeyframeHitToleranceMs);
}

void mv_editor_scene::add_fade_effect_to_selected_layer() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    auto it = std::find_if(layer->effects.begin(), layer->effects.end(), [](const auto& effect) {
        return effect.type == "fade";
    });
    if (it == layer->effects.end()) {
        mv::composition::effect effect;
        effect.id = next_effect_id(composition_, "fx-fade");
        effect.type = "fade";
        effect.target = "transform.opacity";
        effect.amount = 650.0f;
        layer->effects.push_back(std::move(effect));
    } else {
        it->target = "transform.opacity";
        it->amount = it->amount <= 0.0f ? 650.0f : it->amount;
    }
    validate_composition();
    commit_history("Add Fade");
}

void mv_editor_scene::add_pulse_effect_to_selected_layer() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    auto it = std::find_if(layer->effects.begin(), layer->effects.end(), [](const auto& effect) {
        return effect.type == "pulse";
    });
    if (it == layer->effects.end()) {
        mv::composition::effect effect;
        effect.id = next_effect_id(composition_, "fx-pulse");
        effect.type = "pulse";
        effect.target = "transform.scale";
        effect.amount = 0.08f;
        layer->effects.push_back(std::move(effect));
    } else {
        it->target = "transform.scale";
        it->amount = it->amount <= 0.0f ? 0.08f : it->amount;
    }
    validate_composition();
    commit_history("Add Pulse");
}

void mv_editor_scene::add_flash_effect_to_selected_layer() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    auto it = std::find_if(layer->effects.begin(), layer->effects.end(), [](const auto& effect) {
        return effect.type == "flash";
    });
    if (it == layer->effects.end()) {
        mv::composition::effect effect;
        effect.id = next_effect_id(composition_, "fx-flash");
        effect.type = "flash";
        effect.target = "transform.opacity";
        effect.amount = 0.35f;
        layer->effects.push_back(std::move(effect));
    } else {
        it->target = "transform.opacity";
        it->amount = it->amount <= 0.0f ? 0.35f : it->amount;
    }
    validate_composition();
    commit_history("Add Flash");
}

void mv_editor_scene::add_shake_effect_to_selected_layer() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        return;
    }
    auto it = std::find_if(layer->effects.begin(), layer->effects.end(), [](const auto& effect) {
        return effect.type == "shake";
    });
    if (it == layer->effects.end()) {
        mv::composition::effect effect;
        effect.id = next_effect_id(composition_, "fx-shake");
        effect.type = "shake";
        effect.target = "transform.position";
        effect.amount = 18.0f;
        layer->effects.push_back(std::move(effect));
    } else {
        it->target = "transform.position";
        it->amount = it->amount <= 0.0f ? 18.0f : it->amount;
    }
    validate_composition();
    commit_history("Add Shake");
}

void mv_editor_scene::clear_selected_layer_effects() {
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr || layer->effects.empty()) {
        return;
    }
    layer->effects.clear();
    validate_composition();
    commit_history("Clear FX");
}

void mv_editor_scene::normalize_layer_z_order() {
    for (std::size_t i = 0; i < composition_.layers.size(); ++i) {
        composition_.layers[i].z = static_cast<int>((i + 1) * 10);
    }
}

bool mv_editor_scene::move_selected_layer(int direction) {
    if (selected_layer_id_.empty() || direction == 0 || composition_.layers.size() < 2) {
        return false;
    }
    const std::size_t index = layer_index_by_id(composition_, selected_layer_id_);
    if (index == static_cast<std::size_t>(-1)) {
        return false;
    }
    const int next_index_signed = static_cast<int>(index) + direction;
    if (next_index_signed < 0 || next_index_signed >= static_cast<int>(composition_.layers.size())) {
        return false;
    }
    std::swap(composition_.layers[index], composition_.layers[static_cast<std::size_t>(next_index_signed)]);
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
    const auto it = std::find_if(composition_.layers.begin(), composition_.layers.end(), [&](const auto& layer) {
        return layer.id == selected_layer_id_;
    });
    if (it == composition_.layers.end()) {
        return;
    }
    composition_.layers.erase(it);
    selected_layer_id_.clear();
    if (!composition_.layers.empty()) {
        selected_layer_id_ = composition_.layers.back().id;
    }
    normalize_layer_z_order();
    validate_composition();
    commit_history("Delete Layer");
}

mv::composition::layer* mv_editor_scene::selected_layer() {
    const auto it = std::find_if(composition_.layers.begin(), composition_.layers.end(), [&](auto& layer) {
        return layer.id == selected_layer_id_;
    });
    return it == composition_.layers.end() ? nullptr : &*it;
}

const mv::composition::layer* mv_editor_scene::selected_layer() const {
    const auto it = std::find_if(composition_.layers.begin(), composition_.layers.end(), [&](const auto& layer) {
        return layer.id == selected_layer_id_;
    });
    return it == composition_.layers.end() ? nullptr : &*it;
}

const mv::composition::asset_ref* mv_editor_scene::find_asset(const std::string& asset_id) const {
    const auto it = std::find_if(composition_.assets.begin(), composition_.assets.end(), [&](const auto& asset) {
        return asset.id == asset_id;
    });
    return it == composition_.assets.end() ? nullptr : &*it;
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
    for (const auto& layer : composition_.layers) {
        duration = std::max(duration, layer.duration_ms <= 0.0 ? 1.0 : layer.start_ms + layer.duration_ms);
    }
    return std::max(duration, 1000.0);
}
