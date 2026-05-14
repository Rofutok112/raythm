#include "ui_font.h"

#include <cctype>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "core/app_paths.h"
#include "core/path_utils.h"
#include "virtual_screen.h"

namespace ui {

namespace {

constexpr int kFontBaseSize = 64;
constexpr float kBodyFontSizeScale = 1.0f;
constexpr float kUiAuthoringScale1080p = 1920.0f / 1280.0f;

struct loaded_font {
    std::string path;
    Font font = {};
    bool loaded = false;
    float size_scale = 1.0f;
    float spacing_offset = 0.0f;
};

loaded_font g_ui_font;
font_locale_mode g_font_mode = font_locale_mode::automatic;
std::set<int> g_loaded_codepoints;

float snap_custom_font_size(float font_size) {
    return std::max(1.0f, std::round(font_size));
}

float snap_custom_coordinate(float value) {
    return std::round(value);
}

float current_ui_authoring_scale() {
    return virtual_screen::current_render_scale() > 1.0f ? kUiAuthoringScale1080p : 1.0f;
}

float snap_default_font_size(float font_size) {
    return std::max(1.0f, std::round(font_size));
}

float snap_default_coordinate(float value) {
    return std::round(value);
}

bool contains_non_ascii_bytes(const char* text) {
    if (text == nullptr || *text == '\0') {
        return false;
    }

    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor != '\0') {
        if (*cursor > 0x7Fu) {
            return true;
        }
        ++cursor;
    }
    return false;
}

std::string first_existing_font(std::initializer_list<std::filesystem::path> paths) {
    for (const std::filesystem::path& path : paths) {
        if (std::filesystem::exists(path)) {
            return path_utils::to_utf8(path);
        }
    }
    return {};
}

std::string find_ui_font_path() {
    const std::filesystem::path assets = app_paths::assets_root() / "fonts";
    const std::filesystem::path windows_fonts = "C:/Windows/Fonts";
    return first_existing_font({
        assets / "MPLUS1p-Medium.ttf",
        assets / "NotoSansJP-Regular.ttf",
        windows_fonts / "segoeui.ttf",
    });
}

void unload_loaded_font(loaded_font& target) {
    if (target.loaded) {
        UnloadFont(target.font);
    }
    target.font = {};
    target.loaded = false;
}

void rebuild_one_font(loaded_font& target) {
    if (target.path.empty() || g_loaded_codepoints.empty()) {
        return;
    }

    std::vector<int> codepoints(g_loaded_codepoints.begin(), g_loaded_codepoints.end());
    Font next_font = LoadFontEx(target.path.c_str(), kFontBaseSize,
                                codepoints.data(), static_cast<int>(codepoints.size()));
    if (next_font.texture.id == 0) {
        return;
    }

    SetTextureFilter(next_font.texture, TEXTURE_FILTER_BILINEAR);
    unload_loaded_font(target);
    target.font = next_font;
    target.loaded = true;
}

void rebuild_fonts() {
    rebuild_one_font(g_ui_font);
}

void seed_ascii_codepoints() {
    for (int cp = 32; cp <= 126; ++cp) {
        g_loaded_codepoints.insert(cp);
    }
}

}  // namespace

void set_font_locale_mode(font_locale_mode mode) {
    g_font_mode = mode;
}

void initialize_text_font() {
    g_ui_font = {.path = find_ui_font_path(), .size_scale = kBodyFontSizeScale, .spacing_offset = 0.0f};
    g_loaded_codepoints.clear();
    seed_ascii_codepoints();

    rebuild_fonts();
    if (!g_ui_font.loaded) {
        g_ui_font.font = GetFontDefault();
    }
}

void shutdown_text_font() {
    unload_loaded_font(g_ui_font);
    g_ui_font.path.clear();
    g_loaded_codepoints.clear();
}

Font text_font() {
    return g_ui_font.loaded ? g_ui_font.font : GetFontDefault();
}

Font text_font_for_text(const char* text) {
    (void)text;
    return text_font();
}

Font body_font() {
    return g_ui_font.loaded ? g_ui_font.font : text_font();
}

Font display_font() {
    return GetFontDefault();
}

float text_layout_font_size(float font_size) {
    return font_size * current_ui_authoring_scale();
}

float text_font_size_for_text(const char* text, float font_size) {
    (void)text;
    const float scaled_font_size = text_layout_font_size(font_size);
    return snap_custom_font_size(scaled_font_size * (g_ui_font.loaded ? g_ui_font.size_scale : 1.0f));
}

float text_spacing_for_text(const char* text, float font_size, float spacing) {
    (void)font_size;
    if (spacing != 0.0f) {
        return snap_custom_font_size(text_layout_font_size(spacing));
    }
    if (text_font_for_text(text).texture.id == GetFontDefault().texture.id) {
        const Font font = GetFontDefault();
        return font.baseSize > 0 ? snap_default_font_size(font_size) / static_cast<float>(font.baseSize) : 0.0f;
    }
    return std::max(1.0f, std::round(current_ui_authoring_scale()));
}

float body_font_size(float font_size) {
    return snap_default_font_size(text_layout_font_size(font_size) * g_ui_font.size_scale);
}

float body_spacing(float spacing) {
    if (spacing != 0.0f) {
        return snap_custom_font_size(text_layout_font_size(spacing));
    }
    return g_ui_font.loaded ? std::max(1.0f, std::round(current_ui_authoring_scale())) : 0.0f;
}

float display_font_size(float font_size) {
    return snap_default_font_size(text_layout_font_size(font_size));
}

float display_spacing(float font_size, float spacing) {
    if (spacing != 0.0f) {
        return snap_default_font_size(text_layout_font_size(spacing));
    }
    const Font font = GetFontDefault();
    return font.baseSize > 0 ? snap_default_font_size(text_layout_font_size(font_size)) / static_cast<float>(font.baseSize)
                             : 0.0f;
}

void ensure_text_glyphs(const char* text) {
    int codepoint_count = 0;
    int* codepoints = LoadCodepoints(text, &codepoint_count);
    if (codepoints == nullptr) {
        return;
    }

    bool changed = false;
    for (int i = 0; i < codepoint_count; ++i) {
        if (codepoints[i] < 32) {
            continue;
        }
        changed = g_loaded_codepoints.insert(codepoints[i]).second || changed;
    }
    UnloadCodepoints(codepoints);

    if (changed) {
        rebuild_fonts();
    }
}

Vector2 measure_text_size(const char* text, float font_size, float spacing) {
    if (text == nullptr || *text == '\0') {
        return {0.0f, font_size};
    }

    ensure_text_glyphs(text);
    const float adjusted_font_size = text_font_size_for_text(text, font_size);
    return MeasureTextEx(text_font_for_text(text), text, adjusted_font_size,
                         text_spacing_for_text(text, adjusted_font_size, spacing));
}

void draw_text_auto(const char* text, Vector2 position, float font_size, float spacing, Color color) {
    if (text == nullptr || *text == '\0') {
        return;
    }

    ensure_text_glyphs(text);
    const float adjusted_font_size = text_font_size_for_text(text, font_size);
    const Vector2 draw_position = {snap_default_coordinate(position.x), snap_default_coordinate(position.y)};
    DrawTextEx(text_font_for_text(text), text, draw_position, adjusted_font_size,
               text_spacing_for_text(text, adjusted_font_size, spacing), color);
}

Vector2 measure_body_text_size(const char* text, float font_size, float spacing) {
    if (text == nullptr || *text == '\0') {
        return {0.0f, font_size};
    }
    ensure_text_glyphs(text);
    const float adjusted_font_size = body_font_size(font_size);
    return MeasureTextEx(body_font(), text, adjusted_font_size, body_spacing(spacing));
}

Vector2 measure_display_text_size(const char* text, float font_size, float spacing) {
    if (text == nullptr || *text == '\0') {
        return {0.0f, font_size};
    }
    const float adjusted_font_size = display_font_size(font_size);
    return MeasureTextEx(display_font(), text, adjusted_font_size, display_spacing(font_size, spacing));
}

void draw_text_body(const char* text, Vector2 position, float font_size, float spacing, Color color) {
    if (text == nullptr || *text == '\0') {
        return;
    }
    ensure_text_glyphs(text);
    const float adjusted_font_size = body_font_size(font_size);
    const Vector2 draw_position = {snap_default_coordinate(position.x), snap_default_coordinate(position.y)};
    DrawTextEx(body_font(), text, draw_position, adjusted_font_size, body_spacing(spacing), color);
}

void draw_text_display(const char* text, Vector2 position, float font_size, float spacing, Color color) {
    if (text == nullptr || *text == '\0') {
        return;
    }
    const float adjusted_font_size = display_font_size(font_size);
    const Vector2 draw_position = {snap_custom_coordinate(position.x), snap_custom_coordinate(position.y)};
    DrawTextEx(display_font(), text, draw_position, adjusted_font_size, display_spacing(font_size, spacing), color);
}

}  // namespace ui
