#include "ui_font.h"

#include <cctype>
#include <algorithm>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "core/app_paths.h"
#include "core/path_utils.h"

namespace ui {

namespace {

constexpr int kFontBaseSize = 48;
constexpr float kCustomFontSizeScale = 0.8f;
constexpr float kCustomFontSpacingOffset = 2.0f;

std::string g_font_path;
Font g_font = {};
bool g_font_loaded = false;
std::set<int> g_loaded_codepoints;

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

std::string find_font_path() {
    std::vector<std::filesystem::path> candidates;
    const std::filesystem::path assets_root = app_paths::assets_root();
    if (!std::filesystem::exists(assets_root)) {
        return {};
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(assets_root)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string ext = entry.path().extension().string();
        std::ranges::transform(ext, ext.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".ttf" || ext == ".otf") {
            candidates.push_back(entry.path());
        }
    }

    if (candidates.empty()) {
        return {};
    }

    std::ranges::sort(candidates);
    return path_utils::to_utf8(candidates.front());
}

void rebuild_font() {
    if (g_font_path.empty() || g_loaded_codepoints.empty()) {
        return;
    }

    std::vector<int> codepoints(g_loaded_codepoints.begin(), g_loaded_codepoints.end());
    Font next_font = LoadFontEx(g_font_path.c_str(), kFontBaseSize,
                                codepoints.data(), static_cast<int>(codepoints.size()));
    if (next_font.texture.id == 0) {
        return;
    }

    SetTextureFilter(next_font.texture, TEXTURE_FILTER_BILINEAR);
    if (g_font_loaded) {
        UnloadFont(g_font);
    }
    g_font = next_font;
    g_font_loaded = true;
}

void seed_ascii_codepoints() {
    for (int cp = 32; cp <= 126; ++cp) {
        g_loaded_codepoints.insert(cp);
    }
}

}  // namespace

void initialize_text_font() {
    g_font_path = find_font_path();
    g_loaded_codepoints.clear();
    seed_ascii_codepoints();

    if (g_font_path.empty()) {
        g_font = GetFontDefault();
        g_font_loaded = false;
        return;
    }

    rebuild_font();
    if (!g_font_loaded) {
        g_font = GetFontDefault();
    }
}

void shutdown_text_font() {
    if (g_font_loaded) {
        UnloadFont(g_font);
    }
    g_font = {};
    g_font_loaded = false;
    g_font_path.clear();
    g_loaded_codepoints.clear();
}

Font text_font() {
    return g_font_loaded ? g_font : GetFontDefault();
}

Font text_font_for_text(const char* text) {
    if (g_font_loaded && contains_non_ascii_bytes(text)) {
        return g_font;
    }
    return GetFontDefault();
}

float text_font_size_for_text(const char* text, float font_size) {
    if (g_font_loaded && contains_non_ascii_bytes(text)) {
        return font_size * kCustomFontSizeScale;
    }
    return font_size;
}

float text_spacing_for_text(const char* text, float font_size, float spacing) {
    if (g_font_loaded && contains_non_ascii_bytes(text)) {
        return spacing + kCustomFontSpacingOffset;
    }

    if (spacing != 0.0f) {
        return spacing;
    }

    const Font font = text_font_for_text(text);
    if (font.texture.id == GetFontDefault().texture.id && font.baseSize > 0) {
        return font_size / static_cast<float>(font.baseSize);
    }
    return 0.0f;
}

void ensure_text_glyphs(const char* text) {
    if (!g_font_loaded || !contains_non_ascii_bytes(text)) {
        return;
    }

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
        rebuild_font();
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
    DrawTextEx(text_font_for_text(text), text, position, adjusted_font_size,
               text_spacing_for_text(text, adjusted_font_size, spacing), color);
}

}  // namespace ui
