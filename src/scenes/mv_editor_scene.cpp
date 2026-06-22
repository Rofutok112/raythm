#include "mv_editor_scene.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include "audio_manager.h"
#include "managed_content_storage.h"
#include "mv/composition/mv_composition_evaluator.h"
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
#include "ui_inspector.h"
#include "ui_layout.h"
#include "ui_text.h"
#include "ui_text_input.h"
#include "ui/icons/raythm_icons.h"
#include "virtual_screen.h"

namespace {

constexpr float kHeaderHeight = 58.0f;
constexpr float kPadding = 16.0f;
constexpr float kPanelGap = 10.0f;
constexpr float kBackButtonWidth = 112.0f;
constexpr float kHeaderButtonHeight = 34.0f;
constexpr float kHeaderButtonWidth = 108.0f;
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
constexpr float kContextMenuWidth = 188.0f;
constexpr float kContextMenuItemHeight = 24.0f;
constexpr float kContextMenuItemSpacing = 3.0f;
constexpr float kInspectorWheelStep = 48.0f;
constexpr float kHierarchyRowHeight = 34.0f;
constexpr float kTimelineRowHeight = 24.0f;
constexpr float kTimelineWheelStep = 44.0f;
constexpr float kTimelineHorizontalWheelMs = 700.0f;
constexpr float kPreviewHandleSize = 10.0f;
constexpr float kPreviewMinLayerSize = 8.0f;

bool wide_text_filter(int codepoint, const std::string&) {
    return codepoint >= 32;
}

int hex_digit(char ch);

bool signed_number_filter(int codepoint, const std::string& current_value) {
    if (codepoint >= '0' && codepoint <= '9') {
        return true;
    }
    if (codepoint == '.') {
        return current_value.find('.') == std::string::npos;
    }
    if (codepoint == '-') {
        return current_value.empty();
    }
    return false;
}

std::optional<float> parse_float_input(const std::string& value) {
    if (value.empty() || value == "-" || value == "." || value == "-.") {
        return std::nullopt;
    }
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    if (end == nullptr || *end != '\0' || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed;
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
    const mv::composition::component* renderer = mv::composition::renderable_component(layer);
    if (renderer == nullptr) {
        return "Empty";
    }
    if (renderer->type == "shape" && !renderer->shape.empty()) {
        return renderer->type + "/" + renderer->shape;
    }
    return renderer->type.empty() ? "unknown" : renderer->type;
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
        for (const mv::composition::layer& layer : composition.layers) {
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
        if ((component.type == "fade" || component.type == "pulse" ||
             component.type == "flash" || component.type == "shake" ||
             component.type == "beatPulse") && component.type == type) {
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

mv::composition::transform transform_or_default(const mv::composition::layer& layer) {
    const mv::composition::component* transform = mv::composition::transform_component(layer);
    return transform == nullptr ? mv::composition::transform{} : mv::composition::transform_from_component(*transform);
}

void apply_evaluated_transform(mv::composition::layer& layer, const mv::composition::transform& transform) {
    if (mv::composition::component* component = ensure_transform(layer)) {
        mv::composition::apply_transform_to_component(*component, transform);
    }
}

mv::composition::component make_effect_component(std::string id,
                                                 std::string type,
                                                 std::string target,
                                                 float amount) {
    mv::composition::component effect;
    effect.id = std::move(id);
    effect.kind = "component";
    effect.type = std::move(type);
    effect.target = std::move(target);
    effect.amount = amount;
    return effect;
}

std::string component_display_title(const mv::composition::component& component) {
    std::string title = component.type;
    if (title == "beatGrid") {
        title = "Beat Grid";
    } else if (!title.empty()) {
        title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
    }
    return title.empty() ? std::string{"Component"} : title;
}

float component_inspector_card_height(
    const mv::composition::component& component,
    const ui::inspector::color_picker_state* color_picker = nullptr) {
    const float color_picker_extra =
        color_picker != nullptr && color_picker->open
            ? ui::inspector::color_picker_height() + ui::inspector::card_style{}.row_gap
            : 0.0f;
    if (component.type == "transform") {
        return ui::inspector::component_card_height(4);
    }
    if (component.type == "text") {
        return ui::inspector::component_card_height(2) + color_picker_extra;
    }
    if (component.type == "spectrum") {
        return ui::inspector::component_card_height(2) + color_picker_extra;
    }
    if (component.type == "shape" ||
        component.type == "background" ||
        component.type == "beatGrid" ||
        component.type == "waveform") {
        return ui::inspector::component_card_height(1) + color_picker_extra;
    }
    if (component.type == "image" ||
        component.type == "fade" ||
        component.type == "pulse" ||
        component.type == "flash" ||
        component.type == "shake") {
        return ui::inspector::component_card_height(1);
    }
    return ui::inspector::component_card_height(1);
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

Color mix_color(Color from, Color to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return {
        static_cast<unsigned char>(static_cast<float>(from.r) + (static_cast<float>(to.r) - static_cast<float>(from.r)) * t),
        static_cast<unsigned char>(static_cast<float>(from.g) + (static_cast<float>(to.g) - static_cast<float>(from.g)) * t),
        static_cast<unsigned char>(static_cast<float>(from.b) + (static_cast<float>(to.b) - static_cast<float>(from.b)) * t),
        static_cast<unsigned char>(static_cast<float>(from.a) + (static_cast<float>(to.a) - static_cast<float>(from.a)) * t)
    };
}

void draw_title_style_spectrum(Rectangle area, float opacity, double visual_time_ms,
                               const std::array<float, 128>* spectrum) {
    constexpr int kBars = 64;
    const float alpha = std::clamp(opacity, 0.0f, 1.0f);
    const float gap = 3.0f;
    const float bar_w = std::max(2.0f, (area.width - gap * static_cast<float>(kBars - 1)) /
                                           static_cast<float>(kBars));
    const float baseline = area.y + area.height;
    const float block_height = 8.0f;
    const float block_gap = 4.0f;
    const float block_step = block_height + block_gap;
    const Color base_low = with_opacity({107, 33, 168, 255}, alpha * (128.0f / 255.0f));
    const Color base_mid = with_opacity({168, 85, 247, 255}, alpha * (178.0f / 255.0f));
    const Color base_top = with_opacity({216, 180, 254, 255}, alpha * (230.0f / 255.0f));
    const Color peak_glow = with_opacity({216, 180, 254, 255}, alpha * (110.0f / 255.0f));
    const Color peak_color = with_opacity({216, 180, 254, 255}, alpha * (166.0f / 255.0f));
    const float phase = static_cast<float>(visual_time_ms / 260.0);

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
            normalized = std::clamp(sum / static_cast<float>(std::max<std::size_t>(1, end - start)), 0.0f, 1.0f);
        } else {
            const float bass_bias = 1.0f - ratio * 0.35f;
            const float wave = 0.42f + 0.34f * std::sin(phase + ratio * 9.0f) +
                               0.18f * std::sin(phase * 0.55f + ratio * 27.0f);
            normalized = std::clamp(wave * bass_bias, 0.0f, 1.0f);
        }

        const float height = normalized * area.height;
        const float x = area.x + static_cast<float>(i) * (bar_w + gap);
        if (height > 0.5f) {
            for (float block_bottom = baseline; block_bottom > baseline - height; block_bottom -= block_step) {
                const float block_top = std::max(baseline - height, block_bottom - block_height);
                const float segment_height = block_bottom - block_top;
                if (segment_height > 0.5f) {
                    const float color_t = std::clamp((baseline - block_top) / std::max(1.0f, area.height), 0.0f, 1.0f);
                    const Color block_color =
                        color_t < 0.6f
                            ? mix_color(base_low, base_mid, color_t / 0.6f)
                            : mix_color(base_mid, base_top, (color_t - 0.6f) / 0.4f);
                    ui::draw_rect_f({x, block_top, bar_w, segment_height}, block_color);
                }
            }
        }

        const float peak_y = baseline - normalized * area.height - 2.0f;
        ui::draw_rect_f({x, peak_y - 1.0f, bar_w, 4.0f}, peak_glow);
        ui::draw_rect_f({x, peak_y, bar_w, 2.0f}, peak_color);
    }
}

bool layer_active_at(const mv::composition::layer& layer, double time_ms) {
    if (!layer.visible || time_ms < layer.start_ms) {
        return false;
    }
    return layer.duration_ms <= 0.0 || time_ms <= layer.start_ms + layer.duration_ms;
}

bool preview_transformable(const mv::composition::layer& layer) {
    const mv::composition::component* renderer = renderer_or_null(layer);
    return renderer != nullptr && renderer->type != "background";
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

Vector2 layer_base_size_canvas(const mv::composition::mv_composition& composition,
                               const mv::composition::layer& layer,
                               const Texture2D* texture) {
    const mv::composition::component* source_ptr = renderer_or_null(layer);
    if (source_ptr == nullptr) {
        return {480.0f, 270.0f};
    }
    const auto& source = *source_ptr;
    if (source.type == "background") {
        return {static_cast<float>(std::max(1, composition.canvas_data.width)),
                static_cast<float>(std::max(1, composition.canvas_data.height))};
    }
    if (source.type == "text") {
        const std::string text = source.text.empty() ? "Text" : source.text;
        const Vector2 measured = MeasureTextEx(GetFontDefault(), text.c_str(), 44.0f, 1.0f);
        return {std::max(1.0f, measured.x), std::max(1.0f, measured.y)};
    }
    if (source.type == "shape" && (source.shape.empty() || source.shape == "rect")) {
        return {480.0f, 270.0f};
    }
    if (source.type == "image" && texture != nullptr && texture->id != 0) {
        return {static_cast<float>(std::max(1, texture->width)),
                static_cast<float>(std::max(1, texture->height))};
    }
    if (source.type == "beatGrid") {
        return {1280.0f, 720.0f};
    }
    if (source.type == "waveform") {
        return {1280.0f, 280.0f};
    }
    if (source.type == "spectrum") {
        return {1280.0f, 420.0f};
    }
    return {480.0f, 270.0f};
}

Rectangle layer_preview_bounds(Rectangle preview,
                               const mv::composition::mv_composition& composition,
                               const mv::composition::layer& layer,
                               const Texture2D* texture) {
    const mv::composition::transform transform = transform_or_default(layer);
    const mv::composition::component* source = renderer_or_null(layer);
    if (source != nullptr && source->type == "background") {
        return preview;
    }
    const Vector2 position = preview_position(preview, composition, transform);
    if (source != nullptr && source->type == "text") {
        const std::string text = source->text.empty() ? "Text" : source->text;
        const float font_size = std::clamp(44.0f * transform.scale_y * (preview.height / 1080.0f), 10.0f, 72.0f);
        const Vector2 size = MeasureTextEx(GetFontDefault(), text.c_str(), font_size, 1.0f);
        return {position.x - size.x * transform.anchor_x,
                position.y - size.y * transform.anchor_y,
                size.x,
                size.y};
    }

    const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
    const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
    const Vector2 base_size = layer_base_size_canvas(composition, layer, texture);
    const float width = base_size.x * transform.scale_x / canvas_w * preview.width;
    const float height = base_size.y * transform.scale_y / canvas_h * preview.height;
    return {position.x - width * transform.anchor_x,
            position.y - height * transform.anchor_y,
            width,
            height};
}

std::array<Rectangle, 8> preview_transform_handles(Rectangle bounds) {
    const float size = kPreviewHandleSize;
    const float half = size * 0.5f;
    const float cx = bounds.x + bounds.width * 0.5f;
    const float cy = bounds.y + bounds.height * 0.5f;
    const float right = bounds.x + bounds.width;
    const float bottom = bounds.y + bounds.height;
    return {{
        {bounds.x - half, bounds.y - half, size, size},
        {cx - half, bounds.y - half, size, size},
        {right - half, bounds.y - half, size, size},
        {bounds.x - half, cy - half, size, size},
        {right - half, cy - half, size, size},
        {bounds.x - half, bottom - half, size, size},
        {cx - half, bottom - half, size, size},
        {right - half, bottom - half, size, size},
    }};
}

mv_preview_drag_mode preview_drag_mode_for_handle(int index) {
    switch (index) {
        case 0: return mv_preview_drag_mode::northwest;
        case 1: return mv_preview_drag_mode::north;
        case 2: return mv_preview_drag_mode::northeast;
        case 3: return mv_preview_drag_mode::west;
        case 4: return mv_preview_drag_mode::east;
        case 5: return mv_preview_drag_mode::southwest;
        case 6: return mv_preview_drag_mode::south;
        case 7: return mv_preview_drag_mode::southeast;
        default: return mv_preview_drag_mode::none;
    }
}

void draw_preview_transform_overlay(Rectangle bounds, bool locked) {
    const Color line = locked ? with_alpha(g_theme->text_muted, 170) : g_theme->border_active;
    DrawRectangleLinesEx(bounds, 1.5f, line);
    for (const Rectangle handle : preview_transform_handles(bounds)) {
        ui::draw_rect_f(handle, locked ? with_alpha(g_theme->row, 180) : g_theme->accent);
        ui::draw_rect_lines(handle, 1.0f, g_theme->bg);
    }
}

Rectangle resized_preview_rect(Rectangle origin, Vector2 delta,
                               mv_preview_drag_mode mode) {
    Rectangle rect = origin;
    const bool affects_left = mode == mv_preview_drag_mode::west ||
                              mode == mv_preview_drag_mode::northwest ||
                              mode == mv_preview_drag_mode::southwest;
    const bool affects_right = mode == mv_preview_drag_mode::east ||
                               mode == mv_preview_drag_mode::northeast ||
                               mode == mv_preview_drag_mode::southeast;
    const bool affects_top = mode == mv_preview_drag_mode::north ||
                             mode == mv_preview_drag_mode::northwest ||
                             mode == mv_preview_drag_mode::northeast;
    const bool affects_bottom = mode == mv_preview_drag_mode::south ||
                                mode == mv_preview_drag_mode::southwest ||
                                mode == mv_preview_drag_mode::southeast;
    if (affects_left) {
        const float right = origin.x + origin.width;
        rect.x = std::min(right - kPreviewMinLayerSize, origin.x + delta.x);
        rect.width = right - rect.x;
    } else if (affects_right) {
        rect.width = std::max(kPreviewMinLayerSize, origin.width + delta.x);
    }
    if (affects_top) {
        const float bottom = origin.y + origin.height;
        rect.y = std::min(bottom - kPreviewMinLayerSize, origin.y + delta.y);
        rect.height = bottom - rect.y;
    } else if (affects_bottom) {
        rect.height = std::max(kPreviewMinLayerSize, origin.height + delta.y);
    }
    return rect;
}

void apply_preview_rect_to_transform(Rectangle preview,
                                     const mv::composition::mv_composition& composition,
                                     const mv::composition::layer& layer,
                                     const Texture2D* texture,
                                     Rectangle bounds,
                                     mv::composition::transform& transform) {
    const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
    const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
    const Vector2 base_size = layer_base_size_canvas(composition, layer, texture);
    transform.position_x = (bounds.x + bounds.width * transform.anchor_x - preview.x) / preview.width * canvas_w;
    transform.position_y = (bounds.y + bounds.height * transform.anchor_y - preview.y) / preview.height * canvas_h;
    const mv::composition::component* source = renderer_or_null(layer);
    if (source != nullptr && source->type == "background") {
        return;
    }
    if (source != nullptr && source->type == "text") {
        const float scale_from_width = bounds.width / std::max(1.0f, base_size.x * (preview.height / 1080.0f));
        const float scale_from_height = bounds.height / std::max(1.0f, base_size.y * (preview.height / 1080.0f));
        const float scale = std::clamp((scale_from_width + scale_from_height) * 0.5f, 0.1f, 8.0f);
        transform.scale_x = scale;
        transform.scale_y = scale;
        return;
    }
    transform.scale_x = std::clamp(bounds.width / preview.width * canvas_w / std::max(1.0f, base_size.x), 0.05f, 8.0f);
    transform.scale_y = std::clamp(bounds.height / preview.height * canvas_h / std::max(1.0f, base_size.y), 0.05f, 8.0f);
}

void draw_preview_layer(Rectangle preview, const mv::composition::mv_composition& composition,
                        const mv::composition::layer& layer, bool selected, double visual_time_ms,
                        const Texture2D* texture = nullptr,
                        const std::array<float, 256>* waveform_samples = nullptr,
                        const std::array<float, 128>* spectrum = nullptr) {
    const mv::composition::component* source_ptr = renderer_or_null(layer);
    const mv::composition::component* transform_ptr = mv::composition::transform_component(layer);
    if (source_ptr == nullptr || transform_ptr == nullptr) {
        return;
    }
    const auto& source = *source_ptr;
    const mv::composition::transform transform = mv::composition::transform_from_component(*transform_ptr);
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
        if (source.shape == "title") {
            draw_title_style_spectrum(area, transform.opacity, visual_time_ms - layer.start_ms, spectrum);
            if (selected) {
                DrawRectangleLinesEx(area, 1.5f, g_theme->border_active);
            }
            return;
        }
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

using mv_icon_draw_fn = void (*)(Rectangle, Color, float);

bool draw_timeline_icon_button(Rectangle rect, mv_icon_draw_fn icon, bool active,
                               Color icon_color, float icon_inset = 4.0f) {
    const ui::row_state state = ui::draw_row(rect,
                                            active ? g_theme->row_selected : with_alpha(g_theme->row, 120),
                                            g_theme->row_hover,
                                            active ? g_theme->border_active : g_theme->border,
                                            1.0f);
    if (icon != nullptr) {
        const Color color = active ? icon_color : with_alpha(icon_color, 150);
        icon(ui::inset(state.visual, icon_inset), color, 2.0f);
    }
    return state.clicked;
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

    if (ui::draw_button_colored(metadata_button_rect(), "Metadata", 12,
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

    if (ui::draw_button(play_btn, preview_playing_ ? "Pause" : "Play", 12).clicked) {
        set_preview_playing(!preview_playing_);
    }
    if (ui::draw_button_colored(undo_btn, "Undo", 11,
                                history_.can_undo() ? g_theme->row : with_alpha(g_theme->row, 110),
                                g_theme->row_hover, g_theme->text, 1.5f).clicked) {
        undo_edit();
    }
    if (ui::draw_button_colored(redo_btn, "Redo", 11,
                                history_.can_redo() ? g_theme->row : with_alpha(g_theme->row, 110),
                                g_theme->row_hover, g_theme->text, 1.5f).clicked) {
        redo_edit();
    }
    if (ui::draw_button(save_btn, dirty_ ? "Save *" : "Saved", 12).clicked) {
        save_mv();
    }
    if (ui::draw_button(back_btn, "Back", 12).clicked) {
        manager_.change_scene(song_select::make_seamless_create_scene(manager_, song_.meta.song_id));
        ui::flush_draw_queue();
        virtual_screen::end();
        ClearBackground(BLACK);
        virtual_screen::draw_to_screen();
        return;
    }

    const std::string title = package_.meta.name.empty() ? song_.meta.title + " MV" : package_.meta.name;
    ui::draw_text_in_rect(title.c_str(), 18,
                          {metadata_button_rect().x + metadata_button_rect().width + 18.0f, 8.0f,
                           play_btn.x - metadata_button_rect().x - metadata_button_rect().width - 36.0f, 24.0f},
                          g_theme->text, ui::text_align::left);
    const std::string subtitle = song_.meta.title + " / " + song_.meta.artist + "   " + ms_label(playhead_ms_);
    ui::draw_text_in_rect(subtitle.c_str(), 11,
                          {metadata_button_rect().x + metadata_button_rect().width + 18.0f, 32.0f,
                           play_btn.x - metadata_button_rect().x - metadata_button_rect().width - 36.0f, 18.0f},
                          g_theme->text_muted, ui::text_align::left);

    const Rectangle content = {
        kPadding, kHeaderHeight + kPanelGap,
        static_cast<float>(kScreenWidth) - kPadding * 2.0f,
        static_cast<float>(kScreenHeight) - kHeaderHeight - kPanelGap - kPadding
    };
    const auto main_rows = ui::split_rows(content, content.height - kTimelineHeight - kPanelGap, kPanelGap);
    const Rectangle work_area = main_rows.first;
    const Rectangle timeline_panel = main_rows.second;
    const auto left_split = ui::split_columns(work_area, kProjectPanelWidth, kPanelGap);
    const auto right_split = ui::split_columns(left_split.second,
                                              left_split.second.width - kInspectorWidth - kPanelGap,
                                              kPanelGap);
    const Rectangle layers_panel = left_split.first;
    const Rectangle preview_panel = right_split.first;
    const Rectangle inspector_panel = right_split.second;
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const float wheel = GetMouseWheelMove();
    const bool shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool ctrl_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool context_menu_opened_this_frame = false;
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        context_menu_open_ = true;
        context_menu_opened_this_frame = true;
        context_menu_position_ = mouse;
        if (CheckCollisionPointRec(mouse, timeline_panel)) {
            context_menu_target_ = context_menu_target::timeline;
        } else if (CheckCollisionPointRec(mouse, layers_panel)) {
            context_menu_target_ = context_menu_target::project;
        } else {
            context_menu_open_ = false;
            context_menu_target_ = context_menu_target::none;
        }
    }

    ui::draw_panel(layers_panel);
    draw_section_title(layers_panel, "Hierarchy", "Right-click to create");
    const Rectangle layer_view = {layers_panel.x + 10.0f, layers_panel.y + 54.0f,
                                  layers_panel.width - 32.0f, layers_panel.height - 64.0f};
    const Rectangle hierarchy_scrollbar = {layer_view.x + layer_view.width + 8.0f,
                                           layer_view.y, 8.0f, layer_view.height};
    const float hierarchy_content_height =
        static_cast<float>(composition_.layers.size()) * (kHierarchyRowHeight + 6.0f);
    const float hierarchy_max_scroll = std::max(0.0f, hierarchy_content_height - layer_view.height);
    hierarchy_scroll_offset_ = std::clamp(hierarchy_scroll_offset_, 0.0f, hierarchy_max_scroll);
    if (CheckCollisionPointRec(mouse, layer_view) && wheel != 0.0f && !shift_down && !ctrl_down) {
        hierarchy_scroll_offset_ = std::clamp(hierarchy_scroll_offset_ - wheel * kTimelineWheelStep,
                                              0.0f, hierarchy_max_scroll);
    }
    const ui::scrollbar_interaction hierarchy_scrollbar_result =
        ui::update_vertical_scrollbar(hierarchy_scrollbar, hierarchy_content_height, hierarchy_scroll_offset_,
                                      hierarchy_scrollbar_dragging_, hierarchy_scrollbar_drag_offset_, 30.0f);
    hierarchy_scroll_offset_ = hierarchy_scrollbar_result.scroll_offset;
    hierarchy_scrollbar_dragging_ = hierarchy_scrollbar_result.dragging;
    {
        ui::scoped_clip_rect clip(layer_view);
        float y = layer_view.y - hierarchy_scroll_offset_;
        for (const mv::composition::layer& layer : composition_.layers) {
            const std::size_t layer_index = layer_index_by_id(composition_, layer.id);
            const Rectangle row = {layer_view.x, y, layer_view.width, kHierarchyRowHeight};
            const bool selected = layer.id == selected_layer_id_;
            const auto state = ui::draw_selectable_row(row, selected, 1.5f);
            if (state.clicked) {
                selected_layer_id_ = layer.id;
                sync_inspector_inputs(layer);
            }
            const float reorder_button_width = selected ? 28.0f : 0.0f;
            const float reorder_gap = selected ? 5.0f : 0.0f;
            const float text_width = row.width - 20.0f - reorder_button_width * 2.0f - reorder_gap * 2.0f;
            ui::draw_text_in_rect(layer.name.c_str(), 12,
                                  {row.x + 10.0f, row.y + 2.0f, text_width, 17.0f},
                                  g_theme->text, ui::text_align::left);
            const std::string meta = layer_type_label(layer) + "   z " + std::to_string(layer.z);
            ui::draw_text_in_rect(meta.c_str(), 10,
                                  {row.x + 10.0f, row.y + 18.0f, text_width, 15.0f},
                                  g_theme->text_muted, ui::text_align::left);
            if (selected) {
                const Rectangle up_btn = {row.x + row.width - 62.0f, row.y + 5.0f, 26.0f, 24.0f};
                const Rectangle down_btn = {up_btn.x + up_btn.width + 5.0f, up_btn.y, 26.0f, 24.0f};
                const bool can_move_up = layer_index != static_cast<std::size_t>(-1) &&
                                         layer_index + 1 < composition_.layers.size();
                const bool can_move_down = layer_index != static_cast<std::size_t>(-1) && layer_index > 0;
                if (ui::draw_button_colored(up_btn, "Up", 9,
                                            can_move_up ? g_theme->row : with_alpha(g_theme->row, 110),
                                            g_theme->row_hover, g_theme->text, 1.5f).clicked &&
                    can_move_up) {
                    move_selected_layer(1);
                }
                if (ui::draw_button_colored(down_btn, "Dn", 9,
                                            can_move_down ? g_theme->row : with_alpha(g_theme->row, 110),
                                            g_theme->row_hover, g_theme->text, 1.5f).clicked &&
                    can_move_down) {
                    move_selected_layer(-1);
                }
            }
            y += kHierarchyRowHeight + 6.0f;
        }
    }
    ui::draw_scrollbar(hierarchy_scrollbar, hierarchy_content_height, hierarchy_scroll_offset_,
                       with_alpha(g_theme->row, 120), g_theme->slider_fill, 30.0f);

    ui::draw_panel(preview_panel);
    draw_section_title(preview_panel, "Composition", "Active camera preview");
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
            const mv::composition::component* renderer = renderer_or_null(*layer);
            if (renderer != nullptr && renderer->type == "image") {
                if (const mv::composition::asset_ref* asset = find_asset(renderer->asset_id);
                    asset != nullptr) {
                    texture = texture_for_asset(*asset);
                }
            }
            mv::composition::layer evaluated_layer = *layer;
            apply_evaluated_transform(evaluated_layer, mv::composition::evaluate_transform(*layer, playhead_ms_));
            draw_preview_layer(preview, composition_, evaluated_layer, layer->id == selected_layer_id_,
                               playhead_ms_,
                               texture,
                               has_waveform_samples ? &waveform_samples : nullptr,
                               has_spectrum ? &spectrum : nullptr);
        }

        auto texture_for_layer = [&](const mv::composition::layer& layer) -> const Texture2D* {
            const mv::composition::component* renderer = renderer_or_null(layer);
            if (renderer == nullptr || renderer->type != "image") {
                return nullptr;
            }
            const mv::composition::asset_ref* asset = find_asset(renderer->asset_id);
            return asset == nullptr ? nullptr : texture_for_asset(*asset);
        };
        auto evaluated_bounds_for_layer = [&](const mv::composition::layer& layer) {
            mv::composition::layer evaluated_layer = layer;
            apply_evaluated_transform(evaluated_layer, mv::composition::evaluate_transform(layer, playhead_ms_));
            return layer_preview_bounds(preview, composition_, evaluated_layer, texture_for_layer(layer));
        };

        if (preview_drag_mode_ != mv_preview_drag_mode::none) {
            if (mv::composition::layer* layer = selected_layer(); layer != nullptr && !layer->locked) {
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    const Vector2 delta = {
                        mouse.x - preview_drag_origin_mouse_.x,
                        mouse.y - preview_drag_origin_mouse_.y
                    };
                    Rectangle next_bounds = preview_drag_origin_rect_;
                    if (preview_drag_mode_ == mv_preview_drag_mode::move) {
                        next_bounds.x += delta.x;
                        next_bounds.y += delta.y;
                    } else {
                        next_bounds = resized_preview_rect(preview_drag_origin_rect_, delta, preview_drag_mode_);
                    }
                    mv::composition::transform next_transform = preview_drag_origin_transform_;
                    apply_preview_rect_to_transform(preview, composition_, *layer, texture_for_layer(*layer),
                                                    next_bounds, next_transform);
                    if (mv::composition::component* transform = ensure_transform(*layer)) {
                        mv::composition::apply_transform_to_component(*transform, next_transform);
                    }
                    dirty_ = true;
                }
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    commit_history("Edit Transform");
                    validate_composition();
                    preview_drag_mode_ = mv_preview_drag_mode::none;
                    preview_drag_layer_id_.clear();
                }
            } else {
                preview_drag_mode_ = mv_preview_drag_mode::none;
                preview_drag_layer_id_.clear();
            }
        }

        if (CheckCollisionPointRec(mouse, preview) &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            preview_drag_mode_ == mv_preview_drag_mode::none &&
            !context_menu_open_) {
            bool started_drag = false;
            if (mv::composition::layer* selected = selected_layer(); selected != nullptr) {
                const Rectangle selected_bounds = evaluated_bounds_for_layer(*selected);
                const std::array<Rectangle, 8> handles = preview_transform_handles(selected_bounds);
                for (int i = 0; i < static_cast<int>(handles.size()); ++i) {
                    if (preview_transformable(*selected) &&
                        !selected->locked &&
                        CheckCollisionPointRec(mouse, handles[static_cast<std::size_t>(i)])) {
                        preview_drag_mode_ = preview_drag_mode_for_handle(i);
                        preview_drag_layer_id_ = selected->id;
                        preview_drag_origin_mouse_ = mouse;
                        preview_drag_origin_rect_ = selected_bounds;
                        preview_drag_origin_transform_ = transform_or_default(*selected);
                        set_preview_playing(false);
                        started_drag = true;
                        break;
                    }
                }
            }
            if (!started_drag) {
                for (auto it = draw_layers.rbegin(); it != draw_layers.rend(); ++it) {
                    const mv::composition::layer& candidate = **it;
                    if (!preview_transformable(candidate)) {
                        continue;
                    }
                    const Rectangle bounds = evaluated_bounds_for_layer(candidate);
                    if (CheckCollisionPointRec(mouse, bounds)) {
                        selected_layer_id_ = candidate.id;
                        sync_inspector_inputs(candidate);
                        if (!candidate.locked) {
                            preview_drag_mode_ = mv_preview_drag_mode::move;
                            preview_drag_layer_id_ = candidate.id;
                            preview_drag_origin_mouse_ = mouse;
                            preview_drag_origin_rect_ = bounds;
                            preview_drag_origin_transform_ = transform_or_default(candidate);
                            set_preview_playing(false);
                        }
                        break;
                    }
                }
            }
        }

        if (const mv::composition::layer* selected = selected_layer();
            selected != nullptr && preview_transformable(*selected)) {
            draw_preview_transform_overlay(evaluated_bounds_for_layer(*selected), selected->locked);
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
    const float layer_gutter_width = 260.0f;
    const Rectangle layer_name_area = {track_area.x, track_area.y, layer_gutter_width, track_area.height};
    const Rectangle lane_area = {track_area.x + layer_gutter_width, track_area.y,
                                 track_area.width - layer_gutter_width, track_area.height};
    ui::draw_rect_f(layer_name_area, with_alpha(g_theme->row, 145));
    ui::draw_rect_lines(layer_name_area, 1.0f, g_theme->border_light);
    const double safe_duration = std::max(1.0, duration);
    const bool timeline_hovered = CheckCollisionPointRec(mouse, track_area);
    if (timeline_hovered && wheel != 0.0f) {
        if (ctrl_down) {
            const double mouse_ratio = std::clamp((mouse.x - lane_area.x) / std::max(1.0f, lane_area.width),
                                                  0.0f, 1.0f);
            const double previous_visible_ms = safe_duration / std::max(1.0f, timeline_zoom_);
            const double anchor_ms = timeline_horizontal_scroll_ms_ + previous_visible_ms * mouse_ratio;
            timeline_zoom_ = std::clamp(timeline_zoom_ * (wheel > 0.0f ? 1.18f : 0.85f), 1.0f, 16.0f);
            const double next_visible_ms = std::max(250.0, safe_duration / std::max(1.0f, timeline_zoom_));
            timeline_horizontal_scroll_ms_ = anchor_ms - next_visible_ms * mouse_ratio;
        } else if (shift_down) {
            timeline_horizontal_scroll_ms_ -= static_cast<double>(wheel) * kTimelineHorizontalWheelMs / timeline_zoom_;
        } else {
            timeline_vertical_scroll_offset_ -= wheel * kTimelineWheelStep;
        }
    }
    const double visible_duration_ms = std::max(250.0, safe_duration / std::max(1.0f, timeline_zoom_));
    timeline_horizontal_scroll_ms_ = std::clamp(timeline_horizontal_scroll_ms_, 0.0,
                                                std::max(0.0, safe_duration - visible_duration_ms));
    const float timeline_content_height =
        static_cast<float>(composition_.layers.size()) * kTimelineRowHeight + 20.0f;
    timeline_vertical_scroll_offset_ = std::clamp(timeline_vertical_scroll_offset_,
                                                  0.0f,
                                                  std::max(0.0f, timeline_content_height - lane_area.height));
    {
        ui::scoped_clip_rect lane_grid_clip(lane_area);
        for (int i = 0; i <= 8; ++i) {
            const float x = lane_area.x + lane_area.width * (static_cast<float>(i) / 8.0f);
            ui::draw_rect_f({x, lane_area.y, 1.0f, lane_area.height}, g_theme->editor_grid_minor);
            const std::string tick = ms_label(timeline_horizontal_scroll_ms_ +
                                              visible_duration_ms * static_cast<double>(i) / 8.0);
            ui::draw_text_in_rect(tick.c_str(), 10,
                                  {x + 4.0f, lane_area.y + 2.0f, 76.0f, 16.0f},
                                  g_theme->text_muted, ui::text_align::left);
        }
    }
    const Vector2 timeline_mouse = virtual_screen::get_virtual_mouse();
    const double ms_per_pixel = visible_duration_ms / std::max(1.0f, lane_area.width);
    bool timeline_span_hit = false;
    bool timeline_dragging = timeline_drag_mode_ != timeline_drag_mode::none &&
                             IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool timeline_delete_requested = false;
    std::string timeline_delete_layer_id;
    float row_y = track_area.y + 10.0f - timeline_vertical_scroll_offset_;
    for (auto& layer : composition_.layers) {
        const Rectangle layer_row = {layer_name_area.x, row_y - 4.0f, layer_name_area.width, 24.0f};
        const bool selected = layer.id == selected_layer_id_;
        {
            ui::scoped_clip_rect layer_name_clip(layer_name_area);
            ui::draw_rect_f(layer_row, selected ? g_theme->row_selected : with_alpha(g_theme->section, 145));
            const Rectangle visible_btn = {layer_row.x + 6.0f, layer_row.y + 3.0f, 22.0f, 18.0f};
            const Rectangle lock_btn = {visible_btn.x + visible_btn.width + 4.0f, visible_btn.y, 22.0f, 18.0f};
            const Rectangle delete_btn = {layer_row.x + layer_row.width - 28.0f, visible_btn.y, 22.0f, 18.0f};
            if (draw_timeline_icon_button(visible_btn, raythm_icons::draw_eye,
                                          layer.visible, g_theme->text, 3.0f)) {
                selected_layer_id_ = layer.id;
                layer.visible = !layer.visible;
                validate_composition();
                commit_history("Toggle Visibility");
            }
            if (draw_timeline_icon_button(lock_btn,
                                          layer.locked ? raythm_icons::draw_lock : raythm_icons::draw_unlock,
                                          layer.locked, g_theme->text, 4.0f)) {
                selected_layer_id_ = layer.id;
                layer.locked = !layer.locked;
                validate_composition();
                commit_history("Toggle Lock");
            }
            if (draw_timeline_icon_button(delete_btn, raythm_icons::draw_trash_2,
                                          false, g_theme->error, 4.0f)) {
                timeline_delete_requested = true;
                timeline_delete_layer_id = layer.id;
            }
            ui::draw_text_in_rect(layer.name.c_str(), 10,
                                  {lock_btn.x + lock_btn.width + 8.0f, layer_row.y,
                                   delete_btn.x - lock_btn.x - lock_btn.width - 12.0f, layer_row.height},
                                  selected ? g_theme->text : g_theme->text_muted, ui::text_align::left);
            if (ui::is_hovered(layer_row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                selected_layer_id_ = layer.id;
                sync_inspector_inputs(layer);
            }
        }
        const float start_x = lane_area.x +
            static_cast<float>((layer.start_ms - timeline_horizontal_scroll_ms_) / visible_duration_ms) * lane_area.width;
        const double end_ms = layer.duration_ms <= 0.0 ? duration : std::min(duration, layer.start_ms + layer.duration_ms);
        const float end_x = lane_area.x +
            static_cast<float>((end_ms - timeline_horizontal_scroll_ms_) / visible_duration_ms) * lane_area.width;
        const Rectangle span = {start_x, row_y, std::max(3.0f, end_x - start_x), 16.0f};
        const bool locked = layer.locked;
        const Color span_color = locked
            ? with_alpha(g_theme->slider_fill, 95)
            : selected ? g_theme->accent : g_theme->slider_fill;
        {
            ui::scoped_clip_rect lane_clip(lane_area);
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
                    const float marker_x = lane_area.x +
                        static_cast<float>((point.time_ms - timeline_horizontal_scroll_ms_) / visible_duration_ms) *
                        lane_area.width;
                    const Rectangle marker = {marker_x - 2.5f, row_y - 3.0f, 5.0f, 22.0f};
                    const Color marker_color = layer.id == selected_layer_id_
                        ? g_theme->border_active
                        : with_alpha(g_theme->text_muted, 170);
                    ui::draw_rect_f(marker, marker_color);
                }
            }
        }
        row_y += kTimelineRowHeight;
        if (row_y > lane_area.y + lane_area.height - 12.0f) {
            break;
        }
    }
    if (timeline_delete_requested) {
        selected_layer_id_ = timeline_delete_layer_id;
        delete_selected_layer();
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
        ui::is_hovered(lane_area) &&
        !timeline_span_hit) {
        playhead_ms_ = std::clamp(
            timeline_horizontal_scroll_ms_ +
                std::clamp((timeline_mouse.x - lane_area.x) / std::max(1.0f, lane_area.width), 0.0f, 1.0f) *
                    visible_duration_ms,
            0.0, duration);
        set_preview_playing(false);
        seek_preview_audio_to_playhead();
    }
    const float playhead_x = lane_area.x +
        static_cast<float>((playhead_ms_ - timeline_horizontal_scroll_ms_) / std::max(1.0, visible_duration_ms)) *
            lane_area.width;
    {
        ui::scoped_clip_rect lane_playhead_clip(lane_area);
        ui::draw_rect_f({playhead_x - 1.0f, lane_area.y, 2.0f, lane_area.height}, g_theme->border_active);
    }

    ui::draw_panel(inspector_panel);
    draw_section_title(inspector_panel, "Inspector", "Selected object");
    mv::composition::layer* layer = selected_layer();
    if (layer == nullptr) {
        ui::draw_text_in_rect("No object selected", 16,
                              {inspector_panel.x + 18.0f, inspector_panel.y + 70.0f,
                               inspector_panel.width - 36.0f, 36.0f},
                              g_theme->text_muted, ui::text_align::left);
    } else {
        sync_inspector_inputs(*layer);
        const Rectangle inspector_view = {inspector_panel.x + 18.0f, inspector_panel.y + 58.0f,
                                          inspector_panel.width - 50.0f, inspector_panel.height - 76.0f};
        const Rectangle inspector_scrollbar = {inspector_view.x + inspector_view.width + 8.0f,
                                               inspector_view.y, 8.0f, inspector_view.height};
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
        const float inspector_max_scroll = std::max(0.0f, inspector_content_height - inspector_view.height);
        inspector_scroll_offset_ = std::clamp(inspector_scroll_offset_, 0.0f, inspector_max_scroll);
        if (CheckCollisionPointRec(mouse, inspector_view) && wheel != 0.0f && !shift_down && !ctrl_down) {
            inspector_scroll_offset_ = std::clamp(
                inspector_scroll_offset_ - wheel * kInspectorWheelStep,
                0.0f, inspector_max_scroll);
        }
        const ui::scrollbar_interaction inspector_scrollbar_result =
            ui::update_vertical_scrollbar(inspector_scrollbar,
                                          inspector_content_height,
                                          inspector_scroll_offset_,
                                          inspector_scrollbar_dragging_,
                                          inspector_scrollbar_drag_offset_,
                                          30.0f);
        inspector_scroll_offset_ = inspector_scrollbar_result.scroll_offset;
        inspector_scrollbar_dragging_ = inspector_scrollbar_result.dragging;
        Rectangle body = {inspector_view.x, inspector_view.y - inspector_scroll_offset_,
                          inspector_view.width, inspector_content_height};
        {
            ui::scoped_clip_rect inspector_clip(inspector_view);
        const auto layer_name_result =
            ui::draw_text_input({body.x, body.y, body.width, 30.0f}, layer_name_input_,
                                "Name", "Object name", nullptr, ui::draw_layer::base,
                                12, 96, wide_text_filter, 72.0f);
        if (layer_name_result.changed) {
            layer->name = layer_name_input_.value.empty() ? "Object" : layer_name_input_.value;
            dirty_ = true;
            inspector_edit_pending_ = true;
        }
        if ((layer_name_result.deactivated || layer_name_result.submitted) && inspector_edit_pending_) {
            commit_history("Edit Object Name");
        }
        const std::string type = layer_type_label(*layer);
        ui::draw_text_in_rect(type.c_str(), 11, {body.x, body.y + 31.0f, body.width, 18.0f},
                              g_theme->text_muted, ui::text_align::left);
        if (layer != nullptr) {
            bool inspector_changed = false;
            float detail_y = body.y + 58.0f;
            const float transform_card_h = component_inspector_card_height(*transform);
            const ui::inspector::component_card_result transform_card =
                ui::inspector::draw_component_card({body.x, detail_y, body.width, transform_card_h},
                                                   "Transform",
                                                   false);
            ui::inspector::field_cursor field_cursor =
                ui::inspector::make_field_cursor(transform_card.body);
            if (!transform_x_input_.active) {
                transform_x_input_.value = format_float_input(transform->position_x, 0);
            }
            if (!transform_y_input_.active) {
                transform_y_input_.value = format_float_input(transform->position_y, 0);
            }
            if (!transform_scale_input_.active) {
                transform_scale_input_.value = format_float_input(transform->scale_x, 2);
            }
            auto draw_transform_input = [&](float y,
                                            const char* label,
                                            ui::text_input_state& input,
                                            float& target,
                                            int decimals,
                                            float* mirrored_target = nullptr) {
                const auto result =
                    ui::inspector::draw_number_row(field_cursor.body,
                                                   y,
                                                   input,
                                                   label,
                                                   "0",
                                                   signed_number_filter);
                if (result.changed) {
                    if (const std::optional<float> parsed = parse_float_input(input.value)) {
                        target = decimals <= 0 ? std::round(*parsed) : *parsed;
                        if (mirrored_target != nullptr) {
                            target = std::clamp(target, 0.05f, 8.0f);
                            *mirrored_target = target;
                        }
                        dirty_ = true;
                        inspector_changed = true;
                        inspector_edit_pending_ = true;
                    }
                }
                if ((result.deactivated || result.submitted) && !input.value.empty()) {
                    if (const std::optional<float> parsed = parse_float_input(input.value)) {
                        target = decimals <= 0 ? std::round(*parsed) : *parsed;
                        if (mirrored_target != nullptr) {
                            target = std::clamp(target, 0.05f, 8.0f);
                            *mirrored_target = target;
                        }
                    }
                    input.value = format_float_input(target, decimals);
                    if (inspector_edit_pending_) {
                        commit_history("Edit Transform");
                    }
                }
            };
            draw_transform_input(field_cursor.y, "X", transform_x_input_, transform->position_x, 0);
            field_cursor.advance();
            draw_transform_input(field_cursor.y, "Y", transform_y_input_, transform->position_y, 0);
            field_cursor.advance();
            draw_transform_input(field_cursor.y, "Scale", transform_scale_input_,
                                 transform->scale_x, 2, &transform->scale_y);
            field_cursor.advance();
            float changed = ui::inspector::draw_slider_row(
                field_cursor.body, field_cursor.y, "Opacity",
                std::to_string(static_cast<int>(std::round(transform->opacity * 100.0f))) + "%",
                transform->opacity);
            if (changed >= 0.0f) {
                transform->opacity = changed;
                dirty_ = true;
                inspector_changed = true;
            }
            detail_y += transform_card_h + 10.0f;
            if (layer != nullptr) {
                std::string remove_component_id;
                for (mv::composition::component& component : layer->components) {
                    if (component.type == "transform") {
                        continue;
                    }
                    const float card_top = detail_y;
                    const float card_h = component_inspector_card_height(component, color_picker_for(component));
                    const ui::inspector::component_card_result component_card =
                        ui::inspector::draw_component_card({body.x, card_top, body.width, card_h},
                                                           component_display_title(component),
                                                           true);
                    if (component_card.remove_clicked) {
                        remove_component_id = component.id;
                    }
                    field_cursor = ui::inspector::make_field_cursor(component_card.body);
                    if (component.type == "text") {
                        ui::text_input_state& text_input = component_text_inputs_[component.id];
                        ui::text_input_state& fill_input = component_fill_inputs_[component.id];
                        if (text_input.value.empty() && !component.text.empty()) {
                            text_input.value = component.text;
                        }
                        if (fill_input.value.empty()) {
                            fill_input.value = component.fill.empty() ? "#ffffff" : component.fill;
                        }
                        const auto text_result =
                            ui::inspector::draw_text_row(field_cursor.body,
                                                         field_cursor.y,
                                                         text_input,
                                                         "Text",
                                                         "Text",
                                                         "Text",
                                                         wide_text_filter,
                                                         {},
                                                         ui::draw_layer::base,
                                                         160);
                        if (text_result.changed) {
                            component.text = text_input.value;
                            dirty_ = true;
                            inspector_edit_pending_ = true;
                        }
                        field_cursor.advance();
                        ui::inspector::color_picker_state& color_picker =
                            component_color_pickers_[component.id];
                        const auto color_result =
                            ui::inspector::draw_color_row(field_cursor.body,
                                                          field_cursor.y,
                                                          fill_input,
                                                          color_picker);
                        if (color_result.changed && is_valid_hex_color(fill_input.value)) {
                            fill_input.value = ui::inspector::normalize_hex_color(fill_input.value);
                            component.fill = fill_input.value;
                            dirty_ = true;
                            inspector_edit_pending_ = true;
                        }
                        if ((text_result.deactivated || text_result.submitted ||
                             color_result.input.deactivated || color_result.input.submitted) &&
                            inspector_edit_pending_) {
                            commit_history("Edit Text");
                        }
                    } else if (component.type == "shape" || component.type == "background" ||
                               component.type == "beatGrid" || component.type == "waveform" ||
                               component.type == "spectrum") {
                        ui::text_input_state& fill_input = component_fill_inputs_[component.id];
                        if (fill_input.value.empty()) {
                            fill_input.value = component.fill.empty() ? "#ffffff" : component.fill;
                        }
                        ui::inspector::color_picker_state& color_picker =
                            component_color_pickers_[component.id];
                        const auto color_result =
                            ui::inspector::draw_color_row(field_cursor.body,
                                                          field_cursor.y,
                                                          fill_input,
                                                          color_picker);
                        if (color_result.changed && is_valid_hex_color(fill_input.value)) {
                            fill_input.value = ui::inspector::normalize_hex_color(fill_input.value);
                            component.fill = fill_input.value;
                            dirty_ = true;
                            inspector_edit_pending_ = true;
                        }
                        if ((color_result.input.deactivated || color_result.input.submitted) && inspector_edit_pending_) {
                            commit_history("Edit Color");
                        }
                        if (component.type == "spectrum") {
                            field_cursor.advance();
                            if (color_picker.open) {
                                field_cursor.y += ui::inspector::color_picker_height() +
                                                  ui::inspector::card_style{}.row_gap;
                            }
                            ui::inspector::draw_value_row(field_cursor.body, field_cursor.y, "Style",
                                                          component.shape.empty() ? "bars" : component.shape);
                        }
                    } else if (component.type == "image") {
                        ui::inspector::draw_value_row(field_cursor.body, field_cursor.y, "Asset", component.asset_id);
                    } else if (component.type == "fade") {
                        const float amount = component.amount <= 0.0f ? 650.0f : component.amount;
                        changed = ui::inspector::draw_slider_row(
                            field_cursor.body, field_cursor.y, "Amount", ms_label(amount),
                            std::clamp((amount - 100.0f) / 1900.0f, 0.0f, 1.0f));
                        if (changed >= 0.0f) {
                            component.amount = 100.0f + changed * 1900.0f;
                            dirty_ = true;
                            inspector_changed = true;
                        }
                    } else if (component.type == "pulse") {
                        const float amount = std::clamp(component.amount <= 0.0f ? 0.08f : component.amount, 0.0f, 0.3f);
                        const std::string pulse_label =
                            std::to_string(static_cast<int>(std::round(amount * 100.0f))) + "%";
                        changed = ui::inspector::draw_slider_row(field_cursor.body, field_cursor.y, "Amount", pulse_label,
                                                                 std::clamp(amount / 0.3f, 0.0f, 1.0f));
                        if (changed >= 0.0f) {
                            component.amount = changed * 0.3f;
                            dirty_ = true;
                            inspector_changed = true;
                        }
                    } else if (component.type == "flash") {
                        const float amount = std::clamp(component.amount <= 0.0f ? 0.35f : component.amount, 0.0f, 1.0f);
                        const std::string flash_label =
                            std::to_string(static_cast<int>(std::round(amount * 100.0f))) + "%";
                        changed = ui::inspector::draw_slider_row(field_cursor.body, field_cursor.y, "Amount", flash_label, amount);
                        if (changed >= 0.0f) {
                            component.amount = changed;
                            dirty_ = true;
                            inspector_changed = true;
                        }
                    } else if (component.type == "shake") {
                        const float amount = std::clamp(component.amount <= 0.0f ? 18.0f : component.amount, 0.0f, 120.0f);
                        const std::string shake_label =
                            std::to_string(static_cast<int>(std::round(amount))) + "px";
                        changed = ui::inspector::draw_slider_row(field_cursor.body, field_cursor.y, "Amount", shake_label, amount / 120.0f);
                        if (changed >= 0.0f) {
                            component.amount = changed * 120.0f;
                            dirty_ = true;
                            inspector_changed = true;
                        }
                    }
                    detail_y = card_top + card_h + 10.0f;
                }
                if (!remove_component_id.empty()) {
                    layer->components.erase(
                        std::remove_if(layer->components.begin(), layer->components.end(), [&](const auto& component) {
                            return component.type != "transform" && component.id == remove_component_id;
                        }),
                        layer->components.end());
                    dirty_ = true;
                    inspector_changed = true;
                    commit_history("Remove Component");
                }
                const Rectangle add_component_btn = {body.x, detail_y, body.width, 28.0f};
                if (ui::draw_button(add_component_btn, "Add Component", 11, 1.5f).clicked) {
                    context_menu_open_ = true;
                    context_menu_opened_this_frame = true;
                    context_menu_target_ = context_menu_target::components;
                    context_menu_position_ = {add_component_btn.x, add_component_btn.y + add_component_btn.height + 4.0f};
                }
                detail_y += 36.0f;
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
        ui::draw_scrollbar(inspector_scrollbar, inspector_content_height, inspector_scroll_offset_,
                           with_alpha(g_theme->row, 120), g_theme->slider_fill, 30.0f);
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
        context_menu_open_ = false;
    }
    if (context_menu_open_) {
        auto menu_rect_for_count = [&](int item_count) {
            const float menu_height = 12.0f +
                static_cast<float>(item_count) * kContextMenuItemHeight +
                static_cast<float>(std::max(0, item_count - 1)) * kContextMenuItemSpacing;
            Rectangle rect = {context_menu_position_.x, context_menu_position_.y,
                              kContextMenuWidth, menu_height};
            rect.x = std::clamp(rect.x, kPadding, static_cast<float>(kScreenWidth) - rect.width - kPadding);
            rect.y = std::clamp(rect.y, kPadding, static_cast<float>(kScreenHeight) - rect.height - kPadding);
            return rect;
        };
        if (context_menu_target_ == context_menu_target::project) {
            std::array<ui::context_menu_item, 8> items = {{
                {"Create Object", false, ui::context_menu_item::kind::header},
                {"Empty", true},
                {"Text", true},
                {"Rectangle", true},
                {"Image", true},
                {"Beat Grid", true},
                {"Waveform", true},
                {"Spectrum", true},
            }};
            const Rectangle menu_rect = menu_rect_for_count(static_cast<int>(items.size()));
            const ui::context_menu_state menu =
                ui::enqueue_context_menu(menu_rect, std::span<const ui::context_menu_item>(items),
                                         ui::draw_layer::overlay, 11,
                                         kContextMenuItemHeight, kContextMenuItemSpacing);
            if (menu.clicked_index >= 0) {
                switch (menu.clicked_index) {
                    case 1: add_empty_layer(); break;
                    case 2: add_text_layer(); break;
                    case 3: add_rect_layer(); break;
                    case 4: add_image_layer(); break;
                    case 5: add_beat_grid_layer(); break;
                    case 6: add_waveform_layer(); break;
                    case 7: add_spectrum_layer(); break;
                    default: break;
                }
                context_menu_open_ = false;
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mouse, menu_rect)) {
                context_menu_open_ = false;
            }
        } else if (context_menu_target_ == context_menu_target::components) {
            const bool has_layer = selected_layer() != nullptr;
            const bool has_effects = has_layer && !mv::composition::effect_components(*selected_layer()).empty();
            std::array<ui::context_menu_item, 15> items = {{
                {"Add Component", false, ui::context_menu_item::kind::header},
                {"Text", has_layer},
                {"Rectangle", has_layer},
                {"Image", has_layer},
                {"Beat Grid", has_layer},
                {"Waveform", has_layer},
                {"Spectrum", has_layer},
                {"", false, ui::context_menu_item::kind::separator},
                {"Fade", has_layer},
                {"Pulse", has_layer},
                {"Flash", has_layer},
                {"Shake", has_layer},
                {"", false, ui::context_menu_item::kind::separator},
                {"Clear Effects", has_effects},
            }};
            const Rectangle menu_rect = menu_rect_for_count(static_cast<int>(items.size()));
            const ui::context_menu_state menu =
                ui::enqueue_context_menu(menu_rect, std::span<const ui::context_menu_item>(items),
                                         ui::draw_layer::overlay, 11,
                                         kContextMenuItemHeight, kContextMenuItemSpacing);
            if (menu.clicked_index >= 0) {
                switch (menu.clicked_index) {
                    case 1: add_component_to_selected_layer("text"); break;
                    case 2: add_component_to_selected_layer("shape"); break;
                    case 3: add_component_to_selected_layer("image"); break;
                    case 4: add_component_to_selected_layer("beatGrid"); break;
                    case 5: add_component_to_selected_layer("waveform"); break;
                    case 6: add_component_to_selected_layer("spectrum"); break;
                    case 8: add_component_to_selected_layer("fade"); break;
                    case 9: add_component_to_selected_layer("pulse"); break;
                    case 10: add_component_to_selected_layer("flash"); break;
                    case 11: add_component_to_selected_layer("shake"); break;
                    case 13: clear_selected_layer_effects(); break;
                    default: break;
                }
                context_menu_open_ = false;
            } else if (!context_menu_opened_this_frame &&
                       IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                       !CheckCollisionPointRec(mouse, menu_rect)) {
                context_menu_open_ = false;
            }
        } else if (context_menu_target_ == context_menu_target::timeline) {
            const bool has_layer = selected_layer() != nullptr;
            std::array<ui::context_menu_item, 2> items = {{
                {"Timeline", false, ui::context_menu_item::kind::header},
                {"Delete Layer", has_layer},
            }};
            const Rectangle menu_rect = menu_rect_for_count(static_cast<int>(items.size()));
            const ui::context_menu_state menu =
                ui::enqueue_context_menu(menu_rect, std::span<const ui::context_menu_item>(items),
                                         ui::draw_layer::overlay, 11,
                                         kContextMenuItemHeight, kContextMenuItemSpacing);
            if (menu.clicked_index >= 0) {
                switch (menu.clicked_index) {
                    case 1: delete_selected_layer(); break;
                    default: break;
                }
                context_menu_open_ = false;
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mouse, menu_rect)) {
                context_menu_open_ = false;
            }
        }
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
    package_.meta.format_version = 2;
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
    transform_x_input_ = {};
    transform_y_input_ = {};
    transform_scale_input_ = {};
    component_text_inputs_.clear();
    component_fill_inputs_.clear();
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
    component_color_pickers_.clear();
    layer_name_input_.value = layer.name;
    for (const mv::composition::component& component : layer.components) {
        if (!component.text.empty() || component.type == "text") {
            component_text_inputs_[component.id].value = component.text;
        }
        if (!component.fill.empty() ||
            component.type == "text" ||
            component.type == "shape" ||
            component.type == "background" ||
            component.type == "beatGrid" ||
            component.type == "waveform" ||
            component.type == "spectrum") {
            component_fill_inputs_[component.id].value =
                component.fill.empty() ? "#ffffff" : component.fill;
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
    return false;
}

void mv_editor_scene::add_empty_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.layers.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-object");
    layer.name = "Object " + std::to_string(index);
    layer.z = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = std::max(8000.0, composition_duration_ms() - playhead_ms_);
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    layer.components.push_back(mv::composition::make_transform_component(transform));
    composition_.layers.push_back(layer);
    selected_layer_id_ = layer.id;
    normalize_layer_z_order();
    validate_composition();
    commit_history("Add Empty Object");
}

void mv_editor_scene::add_text_layer() {
    mv::composition::layer layer;
    const int index = static_cast<int>(composition_.layers.size()) + 1;
    layer.id = next_layer_id(composition_, "layer-text");
    layer.name = "Text " + std::to_string(index);
    layer.z = index * 10;
    layer.start_ms = playhead_ms_;
    layer.duration_ms = 8000.0;
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    mv::composition::component renderer = mv::composition::make_component("text");
    renderer.id = "renderer-text";
    renderer.text = "Text";
    renderer.fill = "#d8d4ff";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
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
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    transform.opacity = 0.75f;
    mv::composition::component renderer = mv::composition::make_component("shape");
    renderer.id = "renderer-shape";
    renderer.shape = "rect";
    renderer.fill = "#6ee7b7";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
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
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    mv::composition::component renderer = mv::composition::make_component("image");
    renderer.id = "renderer-image";
    renderer.asset_id = imported->id;
    renderer.fill = "#ffffff";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
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
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.5f;
    transform.opacity = 0.8f;
    mv::composition::component renderer = mv::composition::make_component("beatGrid");
    renderer.id = "renderer-beat-grid";
    renderer.fill = "#8b7cf6";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
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
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) * 0.72f;
    transform.opacity = 0.85f;
    mv::composition::component renderer = mv::composition::make_component("waveform");
    renderer.id = "renderer-waveform";
    renderer.fill = "#6ee7b7";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
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
    mv::composition::transform transform;
    transform.position_x = static_cast<float>(composition_.canvas_data.width) * 0.5f;
    transform.position_y = static_cast<float>(composition_.canvas_data.height) - 249.0f;
    transform.scale_x = 1.5f;
    transform.scale_y = 498.0f / 420.0f;
    mv::composition::component renderer = mv::composition::make_component("spectrum");
    renderer.id = "renderer-spectrum";
    renderer.shape = "title";
    renderer.fill = "#a855f7";
    layer.components.push_back(mv::composition::make_transform_component(transform));
    layer.components.push_back(std::move(renderer));
    composition_.layers.push_back(layer);
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
    if (type == "image") {
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
        mv::composition::component image = mv::composition::make_component("image");
        image.id = next_component_id(composition_, "image");
        image.asset_id = imported->id;
        image.fill = "#ffffff";
        layer->components.push_back(std::move(image));
        validate_composition();
        commit_history("Add Image Component");
        return;
    }
    mv::composition::component component = mv::composition::make_component(type);
    component.id = next_component_id(composition_, type);
    if (type == "text") {
        component.text = "Text";
        component.fill = "#d8d4ff";
    } else if (type == "shape") {
        component.shape = "rect";
        component.fill = "#6ee7b7";
    } else if (type == "beatGrid") {
        component.fill = "#8b7cf6";
    } else if (type == "waveform") {
        component.fill = "#6ee7b7";
    } else if (type == "spectrum") {
        component.shape = "title";
        component.fill = "#a855f7";
    } else {
        component = make_effect_component(next_effect_id(composition_, "fx-" + type),
                                          type,
                                          type == "pulse" ? "transform.scale" :
                                          type == "shake" ? "transform.position" : "transform.opacity",
                                          type == "fade" ? 650.0f :
                                          type == "pulse" ? 0.08f :
                                          type == "shake" ? 18.0f : 0.35f);
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
    if (mv::composition::component* effect = find_effect_component(*layer, "fade"); effect == nullptr) {
        layer->components.push_back(
            make_effect_component(next_effect_id(composition_, "fx-fade"), "fade", "transform.opacity", 650.0f));
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
    if (mv::composition::component* effect = find_effect_component(*layer, "pulse"); effect == nullptr) {
        layer->components.push_back(
            make_effect_component(next_effect_id(composition_, "fx-pulse"), "pulse", "transform.scale", 0.08f));
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
    if (mv::composition::component* effect = find_effect_component(*layer, "flash"); effect == nullptr) {
        layer->components.push_back(
            make_effect_component(next_effect_id(composition_, "fx-flash"), "flash", "transform.opacity", 0.35f));
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
    if (mv::composition::component* effect = find_effect_component(*layer, "shake"); effect == nullptr) {
        layer->components.push_back(
            make_effect_component(next_effect_id(composition_, "fx-shake"), "shake", "transform.position", 18.0f));
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
            return component.type == "fade" ||
                   component.type == "pulse" ||
                   component.type == "beatPulse" ||
                   component.type == "flash" ||
                   component.type == "shake";
        }),
        layer->components.end());
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
