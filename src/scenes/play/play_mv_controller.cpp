#include "play/play_mv_controller.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "audio_manager.h"
#include "mv/composition/mv_composition_evaluator.h"
#include "mv/composition/mv_composition_event_evaluator.h"
#include "mv/mv_storage.h"
#include "path_utils.h"
#include "raylib.h"
#include "scene_common.h"

namespace {

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

Color parse_color(const std::string& value, float opacity) {
    Color result = WHITE;
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
                                        const mv::composition::asset_ref& asset) {
    std::vector<std::string> errors;
    const std::optional<std::vector<unsigned char>> bytes = mv::read_asset_bytes(package, asset, &errors);
    if (!bytes.has_value() || bytes->empty()) {
        return {};
    }
    Image image = LoadImageFromMemory(image_extension_for_asset(asset),
                                      bytes->data(),
                                      static_cast<int>(bytes->size()));
    if (image.data == nullptr) {
        return {};
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

bool is_layer_active(const mv::composition::layer& layer, double visual_time_ms) {
    if (!layer.visible || visual_time_ms < layer.start_ms) {
        return false;
    }
    return layer.duration_ms <= 0.0 || visual_time_ms <= layer.start_ms + layer.duration_ms;
}

Vector2 to_screen_position(const mv::composition::mv_composition& composition,
                           const mv::composition::transform& transform) {
    const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
    const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
    return {
        transform.position_x / canvas_w * static_cast<float>(kScreenWidth),
        transform.position_y / canvas_h * static_cast<float>(kScreenHeight),
    };
}

void draw_composition_layer(const mv::composition::mv_composition& composition,
                            const mv::composition::layer& layer,
                            double visual_time_ms,
                            const Texture2D* texture = nullptr,
                            const std::array<float, 128>* spectrum = nullptr) {
    const auto& source = layer.source_data;
    const auto& transform = layer.transform_data;
    const Color tint = parse_color(source.fill.empty() ? composition.canvas_data.background : source.fill,
                                   transform.opacity);

    if (source.type == "background") {
        DrawRectangle(0, 0, kScreenWidth, kScreenHeight, tint);
        return;
    }

    const Vector2 position = to_screen_position(composition, transform);
    if (source.type == "text") {
        const std::string text = source.text.empty() ? "MV" : source.text;
        const float scale_y = static_cast<float>(kScreenHeight) /
                              static_cast<float>(std::max(1, composition.canvas_data.height));
        const float font_size = std::clamp(56.0f * transform.scale_y * scale_y, 12.0f, 180.0f);
        const Vector2 size = MeasureTextEx(GetFontDefault(), text.c_str(), font_size, 1.0f);
        const Vector2 origin = {size.x * transform.anchor_x, size.y * transform.anchor_y};
        DrawTextPro(GetFontDefault(), text.c_str(), position, origin, transform.rotation_deg, font_size, 1.0f, tint);
        return;
    }

    if (source.type == "shape" && (source.shape.empty() || source.shape == "rect")) {
        const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
        const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
        const float rect_w = 480.0f * transform.scale_x / canvas_w * static_cast<float>(kScreenWidth);
        const float rect_h = 270.0f * transform.scale_y / canvas_h * static_cast<float>(kScreenHeight);
        const Rectangle rect = {position.x, position.y, rect_w, rect_h};
        const Vector2 origin = {rect_w * transform.anchor_x, rect_h * transform.anchor_y};
        DrawRectanglePro(rect, origin, transform.rotation_deg, tint);
        return;
    }

    if (source.type == "image" && texture != nullptr && texture->id != 0) {
        const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
        const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
        const float rect_w = static_cast<float>(texture->width) * transform.scale_x / canvas_w *
                             static_cast<float>(kScreenWidth);
        const float rect_h = static_cast<float>(texture->height) * transform.scale_y / canvas_h *
                             static_cast<float>(kScreenHeight);
        const Rectangle src = {0.0f, 0.0f, static_cast<float>(texture->width), static_cast<float>(texture->height)};
        const Rectangle dest = {position.x, position.y, rect_w, rect_h};
        const Vector2 origin = {rect_w * transform.anchor_x, rect_h * transform.anchor_y};
        DrawTexturePro(*texture, src, dest, origin, transform.rotation_deg, tint);
        return;
    }

    if (source.type == "beatGrid") {
        const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
        const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
        const float rect_w = 1280.0f * transform.scale_x / canvas_w * static_cast<float>(kScreenWidth);
        const float rect_h = 720.0f * transform.scale_y / canvas_h * static_cast<float>(kScreenHeight);
        const Rectangle area = {position.x - rect_w * transform.anchor_x,
                                position.y - rect_h * transform.anchor_y,
                                rect_w, rect_h};
        const Color base = parse_color(source.fill.empty() ? "#8b7cf6" : source.fill, 1.0f);
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
        return;
    }

    if (source.type == "waveform") {
        const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
        const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
        const float rect_w = 1280.0f * transform.scale_x / canvas_w * static_cast<float>(kScreenWidth);
        const float rect_h = 280.0f * transform.scale_y / canvas_h * static_cast<float>(kScreenHeight);
        const Rectangle area = {position.x - rect_w * transform.anchor_x,
                                position.y - rect_h * transform.anchor_y,
                                rect_w, rect_h};
        const Color base = parse_color(source.fill.empty() ? "#6ee7b7" : source.fill, 1.0f);
        const Color line = with_opacity(base, 0.78f * transform.opacity);
        const Color shadow = with_opacity(base, 0.20f * transform.opacity);
        const float center_y = area.y + area.height * 0.5f;
        const float phase = static_cast<float>((visual_time_ms - layer.start_ms) / 320.0);
        Vector2 previous = {area.x, center_y};
        constexpr int kSegments = 96;
        for (int i = 1; i <= kSegments; ++i) {
            const float ratio = static_cast<float>(i) / static_cast<float>(kSegments);
            const float x = area.x + area.width * ratio;
            const float envelope = 0.25f + 0.75f * std::sin((ratio * 3.0f + 0.15f) * 3.1415926f);
            const float wave = std::sin(ratio * 42.0f + phase) * 0.55f +
                               std::sin(ratio * 19.0f - phase * 0.6f) * 0.35f;
            const float y = center_y + wave * envelope * area.height * 0.42f;
            const Vector2 current = {x, y};
            DrawLineEx(previous, current, 5.0f, shadow);
            DrawLineEx(previous, current, 2.0f, line);
            previous = current;
        }
        return;
    }

    if (source.type == "spectrum") {
        const float canvas_w = static_cast<float>(std::max(1, composition.canvas_data.width));
        const float canvas_h = static_cast<float>(std::max(1, composition.canvas_data.height));
        const float rect_w = 1280.0f * transform.scale_x / canvas_w * static_cast<float>(kScreenWidth);
        const float rect_h = 420.0f * transform.scale_y / canvas_h * static_cast<float>(kScreenHeight);
        const Rectangle area = {position.x - rect_w * transform.anchor_x,
                                position.y - rect_h * transform.anchor_y,
                                rect_w, rect_h};
        const Color base = parse_color(source.fill.empty() ? "#38bdf8" : source.fill, 1.0f);
        const Color bar = with_opacity(base, 0.72f * transform.opacity);
        const Color peak = with_opacity(base, 0.95f * transform.opacity);
        constexpr int kBars = 32;
        const float gap = 3.0f;
        const float bar_w = std::max(2.0f, (area.width - gap * static_cast<float>(kBars - 1)) /
                                           static_cast<float>(kBars));
        const float phase = static_cast<float>((visual_time_ms - layer.start_ms) / 260.0);
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
        return;
    }
}

}  // namespace

play_mv_controller::play_mv_controller() = default;
play_mv_controller::~play_mv_controller() = default;

void play_mv_controller::load_for_song(const std::optional<song_data>& song) {
    reset();
    if (!song.has_value()) {
        TraceLog(LOG_INFO, "MV: no song_data, skipping composition load");
        return;
    }

    const auto package = mv::find_first_package_for_song(song->meta.song_id);
    if (!package.has_value()) {
        TraceLog(LOG_INFO, "MV: composition package not found");
        return;
    }
    package_ = *package;

    std::vector<std::string> errors;
    composition_ = mv::load_composition(*package, &errors);
    if (composition_.has_value()) {
        TraceLog(LOG_INFO,
                 "MV: composition loaded OK (%d layer(s))",
                 static_cast<int>(composition_->layers.size()));
        return;
    }

    TraceLog(LOG_WARNING, "MV: composition load failed");
    for (const std::string& error : errors) {
        TraceLog(LOG_WARNING, "MV:   %s", error.c_str());
    }
}

void play_mv_controller::reset() {
    unload_asset_textures();
    package_.reset();
    composition_.reset();
    previous_visual_time_ms_.reset();
}

void play_mv_controller::notify_song_visual_event(const std::string& event_name, double event_time_ms) {
    if (!composition_.has_value()) {
        return;
    }
    mv::composition::apply_event(*composition_, event_name, event_time_ms);
}

void play_mv_controller::draw(const play_session_state& state, double visual_time_ms) {
    (void)state;
    if (!composition_.has_value()) {
        return;
    }
    const double previous_time_ms =
        (!previous_visual_time_ms_.has_value() || visual_time_ms < *previous_visual_time_ms_)
            ? -1.0
            : *previous_visual_time_ms_;
    mv::composition::apply_timeline_events(*composition_, previous_time_ms, visual_time_ms);
    previous_visual_time_ms_ = visual_time_ms;
    std::array<float, 128> spectrum = {};
    const bool has_spectrum = audio_manager::instance().get_bgm_fft256(spectrum);
    std::vector<const mv::composition::layer*> layers;
    layers.reserve(composition_->layers.size());
    for (const mv::composition::layer& layer : composition_->layers) {
        if (is_layer_active(layer, visual_time_ms)) {
            layers.push_back(&layer);
        }
    }
    std::sort(layers.begin(), layers.end(), [](const auto* left, const auto* right) {
        return left->z < right->z;
    });
    for (const mv::composition::layer* layer : layers) {
        const Texture2D* texture = nullptr;
        if (layer->source_data.type == "image") {
            if (const mv::composition::asset_ref* asset = find_asset(layer->source_data.asset_id);
                asset != nullptr) {
                texture = texture_for_asset(*asset);
            }
        }
        mv::composition::layer evaluated_layer = *layer;
        evaluated_layer.transform_data = mv::composition::evaluate_transform(*layer, visual_time_ms);
        draw_composition_layer(*composition_, evaluated_layer, visual_time_ms, texture,
                               has_spectrum ? &spectrum : nullptr);
    }
}

const mv::composition::asset_ref* play_mv_controller::find_asset(const std::string& asset_id) const {
    if (!composition_.has_value()) {
        return nullptr;
    }
    const auto it = std::find_if(composition_->assets.begin(), composition_->assets.end(), [&](const auto& asset) {
        return asset.id == asset_id;
    });
    return it == composition_->assets.end() ? nullptr : &*it;
}

const Texture2D* play_mv_controller::texture_for_asset(const mv::composition::asset_ref& asset) {
    const auto cached = asset_textures_.find(asset.id);
    if (cached != asset_textures_.end()) {
        return cached->second.id == 0 ? nullptr : &cached->second;
    }
    if (!package_.has_value()) {
        return nullptr;
    }
    Texture2D texture = load_texture_from_asset_bytes(*package_, asset);
    if (texture.id == 0) {
        return nullptr;
    }
    const auto inserted = asset_textures_.emplace(asset.id, texture);
    return &inserted.first->second;
}

void play_mv_controller::unload_asset_textures() {
    for (auto& [_, texture] : asset_textures_) {
        if (texture.id != 0) {
            UnloadTexture(texture);
        }
    }
    asset_textures_.clear();
}
