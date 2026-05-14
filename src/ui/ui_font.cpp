#include "ui_font.h"

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
    bool sdf = false;
    bool prefer_sdf = false;
};

loaded_font g_ui_font;
Shader g_sdf_shader = {};
bool g_sdf_shader_loaded = false;
font_locale_mode g_font_mode = font_locale_mode::automatic;
std::set<int> g_loaded_codepoints;

constexpr const char* kSdfFragmentShader = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

void main()
{
    float distanceFromOutline = texture(texture0, fragTexCoord).a - 0.5;
    float distanceChangePerFragment = length(vec2(dFdx(distanceFromOutline), dFdy(distanceFromOutline)));
    float alpha = smoothstep(-distanceChangePerFragment, distanceChangePerFragment, distanceFromOutline);
    finalColor = vec4(fragColor.rgb, fragColor.a * alpha);
}
)";

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
    target.sdf = false;
}

std::vector<int> loaded_codepoint_vector() {
    std::vector<int> codepoints(g_loaded_codepoints.begin(), g_loaded_codepoints.end());
    return codepoints;
}

bool load_regular_font(loaded_font& target, const std::vector<int>& codepoints) {
    Font next_font = LoadFontEx(target.path.c_str(), kFontBaseSize,
                                const_cast<int*>(codepoints.data()), static_cast<int>(codepoints.size()));
    if (next_font.texture.id == 0) {
        return false;
    }

    SetTextureFilter(next_font.texture, TEXTURE_FILTER_BILINEAR);
    unload_loaded_font(target);
    target.font = next_font;
    target.loaded = true;
    target.sdf = false;
    return true;
}

bool load_sdf_font(loaded_font& target, const std::vector<int>& codepoints) {
    if (!g_sdf_shader_loaded) {
        return false;
    }

    int file_size = 0;
    unsigned char* file_data = LoadFileData(target.path.c_str(), &file_size);
    if (file_data == nullptr || file_size <= 0) {
        return false;
    }

    Font next_font = {};
    next_font.baseSize = kFontBaseSize;
    next_font.glyphCount = static_cast<int>(codepoints.size());
    next_font.glyphPadding = 0;
    next_font.glyphs = LoadFontData(file_data, file_size, kFontBaseSize,
                                    const_cast<int*>(codepoints.data()),
                                    static_cast<int>(codepoints.size()), FONT_SDF);
    UnloadFileData(file_data);

    if (next_font.glyphs == nullptr) {
        return false;
    }

    Image atlas = GenImageFontAtlas(next_font.glyphs, &next_font.recs,
                                    next_font.glyphCount, kFontBaseSize, 0, 1);
    if (atlas.data == nullptr) {
        UnloadFontData(next_font.glyphs, next_font.glyphCount);
        MemFree(next_font.recs);
        next_font.glyphs = nullptr;
        next_font.recs = nullptr;
        return false;
    }

    next_font.texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    if (next_font.texture.id == 0) {
        UnloadFontData(next_font.glyphs, next_font.glyphCount);
        MemFree(next_font.recs);
        next_font.glyphs = nullptr;
        next_font.recs = nullptr;
        return false;
    }

    SetTextureFilter(next_font.texture, TEXTURE_FILTER_BILINEAR);
    unload_loaded_font(target);
    target.font = next_font;
    target.loaded = true;
    target.sdf = true;
    return true;
}

void rebuild_one_font(loaded_font& target) {
    if (target.path.empty() || g_loaded_codepoints.empty()) {
        return;
    }

    const std::vector<int> codepoints = loaded_codepoint_vector();
    if (target.prefer_sdf && load_sdf_font(target, codepoints)) {
        return;
    }
    load_regular_font(target, codepoints);
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
    if (!g_sdf_shader_loaded) {
        g_sdf_shader = LoadShaderFromMemory(nullptr, kSdfFragmentShader);
        g_sdf_shader_loaded = g_sdf_shader.id > 0;
    }

    g_ui_font = {.path = find_ui_font_path(), .size_scale = kBodyFontSizeScale,
                 .spacing_offset = 0.0f, .prefer_sdf = true};
    g_loaded_codepoints.clear();
    seed_ascii_codepoints();

    rebuild_fonts();
    if (!g_ui_font.loaded) {
        g_ui_font.font = GetFontDefault();
    }
}

void shutdown_text_font() {
    unload_loaded_font(g_ui_font);
    if (g_sdf_shader_loaded) {
        UnloadShader(g_sdf_shader);
        g_sdf_shader = {};
        g_sdf_shader_loaded = false;
    }
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

Font text_font(text_role role) {
    switch (role) {
    case text_role::ui_body:
        return body_font();
    case text_role::display:
        return display_font();
    }
    return body_font();
}

float text_font_size(text_role role, float font_size) {
    switch (role) {
    case text_role::ui_body:
        return body_font_size(font_size);
    case text_role::display:
        return display_font_size(font_size);
    }
    return body_font_size(font_size);
}

float text_spacing(text_role role, float font_size, float spacing) {
    switch (role) {
    case text_role::ui_body:
        (void)font_size;
        return body_spacing(spacing);
    case text_role::display:
        return display_spacing(font_size, spacing);
    }
    return body_spacing(spacing);
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
    bool changed = false;
    int codepoint_count = 0;
    int* codepoints = LoadCodepoints(text, &codepoint_count);
    if (codepoints == nullptr) {
        return;
    }

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

void preload_text_glyphs(const std::vector<std::string>& texts) {
    bool changed = false;
    for (const std::string& text : texts) {
        if (text.empty()) {
            continue;
        }
        int codepoint_count = 0;
        int* codepoints = LoadCodepoints(text.c_str(), &codepoint_count);
        if (codepoints == nullptr) {
            continue;
        }
        for (int i = 0; i < codepoint_count; ++i) {
            if (codepoints[i] < 32) {
                continue;
            }
            changed = g_loaded_codepoints.insert(codepoints[i]).second || changed;
        }
        UnloadCodepoints(codepoints);
    }

    if (changed) {
        rebuild_fonts();
    }
}

Vector2 measure_text_size(const char* text, float font_size, float spacing) {
    return measure_text_size(text_role::ui_body, text, font_size, spacing);
}

Vector2 measure_text_size(text_role role, const char* text, float font_size, float spacing) {
    if (text == nullptr || *text == '\0') {
        return {0.0f, font_size};
    }

    if (role == text_role::ui_body) {
        ensure_text_glyphs(text);
    }
    const float adjusted_font_size = text_font_size(role, font_size);
    return MeasureTextEx(text_font(role), text, adjusted_font_size,
                         text_spacing(role, font_size, spacing));
}

void draw_text_auto(const char* text, Vector2 position, float font_size, float spacing, Color color) {
    draw_text(text_role::ui_body, text, position, font_size, spacing, color);
}

Vector2 measure_body_text_size(const char* text, float font_size, float spacing) {
    return measure_text_size(text_role::ui_body, text, font_size, spacing);
}

Vector2 measure_display_text_size(const char* text, float font_size, float spacing) {
    return measure_text_size(text_role::display, text, font_size, spacing);
}

void draw_text(text_role role, const char* text, Vector2 position, float font_size, float spacing, Color color) {
    if (text == nullptr || *text == '\0') {
        return;
    }
    if (role == text_role::ui_body) {
        ensure_text_glyphs(text);
    }
    const float adjusted_font_size = text_font_size(role, font_size);
    const Vector2 draw_position = {snap_default_coordinate(position.x), snap_default_coordinate(position.y)};
    const bool use_sdf_shader = role == text_role::ui_body && g_ui_font.sdf && g_sdf_shader_loaded;
    if (use_sdf_shader) {
        BeginShaderMode(g_sdf_shader);
    }
    DrawTextEx(text_font(role), text, draw_position, adjusted_font_size,
               text_spacing(role, font_size, spacing), color);
    if (use_sdf_shader) {
        EndShaderMode();
    }
}

void draw_text_body(const char* text, Vector2 position, float font_size, float spacing, Color color) {
    draw_text(text_role::ui_body, text, position, font_size, spacing, color);
}

void draw_text_display(const char* text, Vector2 position, float font_size, float spacing, Color color) {
    draw_text(text_role::display, text, {snap_custom_coordinate(position.x), snap_custom_coordinate(position.y)},
              font_size, spacing, color);
}

}  // namespace ui
