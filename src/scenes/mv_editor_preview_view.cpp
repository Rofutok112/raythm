#include "mv_editor_preview_view.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "theme.h"
#include "ui_draw.h"
#include "ui_hit.h"

namespace {

constexpr float kPreviewHandleSize = 10.0f;
constexpr float kPreviewMinLayerSize = 8.0f;

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

mv::composition::transform transform_or_default(const mv::composition::layer& layer) {
    const mv::composition::component* transform = mv::composition::transform_component(layer);
    return transform == nullptr ? mv::composition::transform{} : mv::composition::transform_from_component(*transform);
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
    const mv::composition::component* source_ptr = mv::composition::renderable_component(layer);
    if (source_ptr == nullptr) {
        return {480.0f, 270.0f};
    }
    const auto& source = *source_ptr;
    if (source.type == "BackgroundRenderer") {
        return {static_cast<float>(std::max(1, composition.canvas_data.width)),
                static_cast<float>(std::max(1, composition.canvas_data.height))};
    }
    if (source.type == "TextRenderer") {
        const std::string text = source.text.empty() ? "Text" : source.text;
        const Vector2 measured = MeasureTextEx(GetFontDefault(), text.c_str(), 44.0f, 1.0f);
        return {std::max(1.0f, measured.x), std::max(1.0f, measured.y)};
    }
    if (source.type == "ShapeRenderer" && (source.shape.empty() || source.shape == "rect")) {
        return {480.0f, 270.0f};
    }
    if (source.type == "ImageRenderer" && texture != nullptr && texture->id != 0) {
        return {static_cast<float>(std::max(1, texture->width)),
                static_cast<float>(std::max(1, texture->height))};
    }
    if (source.type == "BeatGridRenderer") {
        return {1280.0f, 720.0f};
    }
    if (source.type == "WaveformRenderer") {
        return {1280.0f, 280.0f};
    }
    if (source.type == "SpectrumRenderer") {
        return {1280.0f, 420.0f};
    }
    return {480.0f, 270.0f};
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
        ui::block_spectrum_bar(x,
                               baseline,
                               bar_w,
                               height,
                               area.height,
                               normalized * area.height,
                               block_height,
                               block_gap,
                               base_low,
                               base_mid,
                               base_top,
                               peak_glow,
                               peak_color);
    }
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

} // namespace

bool preview_transformable(const mv::composition::layer& layer) {
    const mv::composition::component* renderer = mv::composition::renderable_component(layer);
    return renderer != nullptr && renderer->type != "BackgroundRenderer";
}

void draw_mv_preview_background(Rectangle preview,
                                const mv::composition::mv_composition& composition) {
    ui::surface_fill(preview, parse_color(composition.canvas_data.background));
}

void draw_preview_transform_overlay(Rectangle bounds, bool locked) {
    const Color line = locked ? with_alpha(g_theme->text_muted, 170) : g_theme->border_active;
    DrawRectangleLinesEx(bounds, 1.5f, line);
    for (const Rectangle handle : preview_transform_handles(bounds)) {
        ui::surface(handle, locked ? with_alpha(g_theme->row, 180) : g_theme->accent, g_theme->bg, 1.0f);
    }
}

Rectangle layer_preview_bounds(Rectangle preview,
                               const mv::composition::mv_composition& composition,
                               const mv::composition::layer& layer,
                               const Texture2D* texture) {
    const mv::composition::transform transform = transform_or_default(layer);
    const mv::composition::component* source = mv::composition::renderable_component(layer);
    if (source != nullptr && source->type == "BackgroundRenderer") {
        return preview;
    }
    const Vector2 position = preview_position(preview, composition, transform);
    if (source != nullptr && source->type == "TextRenderer") {
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
    const mv::composition::component* source = mv::composition::renderable_component(layer);
    if (source != nullptr && source->type == "BackgroundRenderer") {
        return;
    }
    if (source != nullptr && source->type == "TextRenderer") {
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

void draw_preview_layer(Rectangle preview,
                        const mv::composition::mv_composition& composition,
                        const mv::composition::layer& layer,
                        bool selected,
                        double visual_time_ms,
                        const Texture2D* texture,
                        const std::array<float, 256>* waveform_samples,
                        const std::array<float, 128>* spectrum) {
    const mv::composition::component* source_ptr = mv::composition::renderable_component(layer);
    const mv::composition::component* transform_ptr = mv::composition::transform_component(layer);
    if (source_ptr == nullptr || transform_ptr == nullptr) {
        return;
    }
    const auto& source = *source_ptr;
    const mv::composition::transform transform = mv::composition::transform_from_component(*transform_ptr);
    const Color fill = parse_color(source.fill.empty() ? composition.canvas_data.background : source.fill,
                                   transform.opacity);
    if (source.type == "BackgroundRenderer") {
        ui::surface_fill(preview, fill);
        return;
    }

    const Vector2 position = preview_position(preview, composition, transform);
    if (source.type == "TextRenderer") {
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

    if (source.type == "ShapeRenderer" && (source.shape.empty() || source.shape == "rect")) {
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

    if (source.type == "ImageRenderer" && texture != nullptr && texture->id != 0) {
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

    if (source.type == "BeatGridRenderer") {
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

    if (source.type == "WaveformRenderer") {
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

    if (source.type == "SpectrumRenderer") {
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
            ui::solid_spectrum_bar(rect, bar, peak);
        }
        if (selected) {
            DrawRectangleLinesEx(area, 1.5f, g_theme->border_active);
        }
    }
}

mv_editor_preview_drag_update_result preview_drag_update_result_for(
    const std::string& layer_id,
    mv_preview_drag_mode mode,
    Vector2 mouse,
    Vector2 origin_mouse,
    Rectangle origin_rect) {
    if (layer_id.empty() || mode == mv_preview_drag_mode::none || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        return {};
    }

    const Vector2 delta = {
        mouse.x - origin_mouse.x,
        mouse.y - origin_mouse.y
    };
    Rectangle next_bounds = origin_rect;
    if (mode == mv_preview_drag_mode::move) {
        next_bounds.x += delta.x;
        next_bounds.y += delta.y;
    } else {
        next_bounds = resized_preview_rect(origin_rect, delta, mode);
    }

    return {
        .active = true,
        .layer_id = layer_id,
        .mode = mode,
        .next_bounds = next_bounds,
    };
}

mv_editor_preview_drag_start_result preview_drag_start_result_for_selected_handles(
    const mv::composition::layer& selected,
    Rectangle selected_bounds,
    Vector2 mouse) {
    if (!preview_transformable(selected) || selected.locked) {
        return {};
    }

    const std::array<Rectangle, 8> handles = preview_transform_handles(selected_bounds);
    for (int i = 0; i < static_cast<int>(handles.size()); ++i) {
        if (ui::contains_point(handles[static_cast<std::size_t>(i)], mouse)) {
            return {
                .drag_started = true,
                .drag_layer_id = selected.id,
                .mode = preview_drag_mode_for_handle(i),
                .origin_mouse = mouse,
                .origin_rect = selected_bounds,
                .origin_transform = transform_or_default(selected),
            };
        }
    }
    return {};
}

mv_editor_preview_drag_start_result preview_drag_start_result_for_layer_body(
    const mv::composition::layer& candidate,
    Rectangle bounds,
    Vector2 mouse) {
    if (!preview_transformable(candidate) || !ui::contains_point(bounds, mouse)) {
        return {};
    }

    mv_editor_preview_drag_start_result result;
    result.selected_layer_id = candidate.id;
    result.sync_inspector = true;
    if (!candidate.locked) {
        result.drag_started = true;
        result.drag_layer_id = candidate.id;
        result.mode = mv_preview_drag_mode::move;
        result.origin_mouse = mouse;
        result.origin_rect = bounds;
        result.origin_transform = transform_or_default(candidate);
    }
    return result;
}

mv_editor_preview_drag_end_result preview_drag_end_result_for(bool drag_active,
                                                              bool editable_layer_available) {
    if (!drag_active) {
        return {};
    }
    if (!editable_layer_available) {
        return {
            .cancelled = true,
        };
    }
    return {
        .ended = IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
    };
}
