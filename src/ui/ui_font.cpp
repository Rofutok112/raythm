#include "ui_font.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/app_paths.h"
#include "core/file_io.h"
#include "core/path_utils.h"
#include "raylib_file_io.h"
#include "virtual_screen.h"

namespace ui {

namespace {

constexpr int kFontBaseSize = 64;
constexpr float kBodyFontSizeScale = 1.0f;
constexpr float kUiAuthoringScale1080p = 1920.0f / 1280.0f;

struct loaded_font {
    std::string path;
    std::string atlas_metadata_path;
    Font font = {};
    bool loaded = false;
    float size_scale = 1.0f;
    float spacing_offset = 0.0f;
    bool sdf = false;
    bool prefer_sdf = false;
    bool prebuilt_atlas = false;
    bool dynamic_glyphs = true;
};

loaded_font g_ui_font;
Shader g_sdf_shader = {};
bool g_sdf_shader_loaded = false;
font_locale_mode g_font_mode = font_locale_mode::automatic;
std::set<int> g_loaded_codepoints;
std::unordered_map<int, int> g_ui_glyph_indices;
int g_ui_fallback_glyph_index = 0;

constexpr const char8_t* kExtraUiGlyphs = u8"✓✕×○●◎◇◆□■△▲▽▼◀▶↑↓←→…‥・ー〜～／＼｜（）[]{}「」『』【】";

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

std::string find_ui_font_atlas_metadata_path() {
    const std::filesystem::path path = app_paths::assets_root() / "fonts" / "ui_sdf.rfont";
    if (std::filesystem::exists(path)) {
        return path_utils::to_utf8(path);
    }
    return {};
}

std::string find_ui_font_charset_path() {
    const std::filesystem::path path = app_paths::assets_root() / "fonts" / "japanese_full.txt";
    if (std::filesystem::exists(path)) {
        return path_utils::to_utf8(path);
    }
    return {};
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

void index_ui_font_glyphs(const Font& font) {
    g_ui_glyph_indices.clear();
    g_ui_fallback_glyph_index = 0;
    if (font.glyphs == nullptr || font.glyphCount <= 0) {
        return;
    }
    g_ui_glyph_indices.reserve(static_cast<size_t>(font.glyphCount));
    for (int i = 0; i < font.glyphCount; ++i) {
        g_ui_glyph_indices[font.glyphs[i].value] = i;
        if (font.glyphs[i].value == '?') {
            g_ui_fallback_glyph_index = i;
        }
    }
}

void unload_loaded_font(loaded_font& target) {
    if (target.loaded) {
        UnloadFont(target.font);
    }
    target.font = {};
    target.loaded = false;
    target.sdf = false;
    target.prebuilt_atlas = false;
    target.dynamic_glyphs = true;
    if (&target == &g_ui_font) {
        g_ui_glyph_indices.clear();
        g_ui_fallback_glyph_index = 0;
    }
}

std::vector<int> loaded_codepoint_vector() {
    std::vector<int> codepoints(g_loaded_codepoints.begin(), g_loaded_codepoints.end());
    return codepoints;
}

bool load_regular_font(loaded_font& target, const std::vector<int>& codepoints) {
    Font next_font = raylib_file_io::load_font_utf8(target.path,
                                                    kFontBaseSize,
                                                    const_cast<int*>(codepoints.data()),
                                                    static_cast<int>(codepoints.size()));
    if (next_font.texture.id == 0) {
        return false;
    }

    SetTextureFilter(next_font.texture, TEXTURE_FILTER_BILINEAR);
    const bool dynamic_glyphs = target.dynamic_glyphs;
    unload_loaded_font(target);
    target.font = next_font;
    target.loaded = true;
    target.sdf = false;
    target.dynamic_glyphs = dynamic_glyphs;
    if (&target == &g_ui_font) {
        index_ui_font_glyphs(target.font);
    }
    return true;
}

bool load_sdf_font(loaded_font& target, const std::vector<int>& codepoints) {
    if (!g_sdf_shader_loaded) {
        return false;
    }

    std::vector<unsigned char> file_data = file_io::read_binary_file(path_utils::from_utf8(target.path));
    if (file_data.empty()) {
        return false;
    }

    Font next_font = {};
    next_font.baseSize = kFontBaseSize;
    next_font.glyphCount = static_cast<int>(codepoints.size());
    next_font.glyphPadding = 0;
    next_font.glyphs = LoadFontData(file_data.data(), static_cast<int>(file_data.size()), kFontBaseSize,
                                    const_cast<int*>(codepoints.data()),
                                    static_cast<int>(codepoints.size()), FONT_SDF);

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
    const bool dynamic_glyphs = target.dynamic_glyphs;
    unload_loaded_font(target);
    target.font = next_font;
    target.loaded = true;
    target.sdf = true;
    target.dynamic_glyphs = dynamic_glyphs;
    if (&target == &g_ui_font) {
        index_ui_font_glyphs(target.font);
    }
    return true;
}

bool load_prebuilt_sdf_font(loaded_font& target) {
    if (!g_sdf_shader_loaded || target.atlas_metadata_path.empty()) {
        return false;
    }

    const std::filesystem::path metadata_path = path_utils::from_utf8(target.atlas_metadata_path);
    std::ifstream in(metadata_path, std::ios::binary);
    if (!in) {
        return false;
    }

    std::string magic;
    std::getline(in, magic);
    if (!magic.empty() && magic.back() == '\r') {
        magic.pop_back();
    }
    if (magic != "raythm-rfont-sdf-v1") {
        return false;
    }

    std::string token;
    std::string texture_filename;
    int base_size = 0;
    int glyph_padding = 0;
    int glyph_count = 0;
    if (!(in >> token >> texture_filename) || token != "texture") {
        return false;
    }
    if (!(in >> token >> base_size) || token != "baseSize" || base_size <= 0) {
        return false;
    }
    if (!(in >> token >> glyph_padding) || token != "glyphPadding" || glyph_padding < 0) {
        return false;
    }
    if (!(in >> token >> glyph_count) || token != "glyphCount" || glyph_count <= 0) {
        return false;
    }

    GlyphInfo* glyphs = static_cast<GlyphInfo*>(MemAlloc(sizeof(GlyphInfo) * static_cast<size_t>(glyph_count)));
    Rectangle* recs = static_cast<Rectangle*>(MemAlloc(sizeof(Rectangle) * static_cast<size_t>(glyph_count)));
    if (glyphs == nullptr || recs == nullptr) {
        MemFree(glyphs);
        MemFree(recs);
        return false;
    }

    for (int i = 0; i < glyph_count; ++i) {
        GlyphInfo glyph = {};
        Rectangle rec = {};
        if (!(in >> token
              >> glyph.value
              >> rec.x >> rec.y >> rec.width >> rec.height
              >> glyph.offsetX >> glyph.offsetY >> glyph.advanceX) ||
            token != "glyph") {
            MemFree(glyphs);
            MemFree(recs);
            return false;
        }
        glyphs[i] = glyph;
        recs[i] = rec;
    }

    const std::filesystem::path texture_path = metadata_path.parent_path() / path_utils::from_utf8(texture_filename);
    Texture2D texture = raylib_file_io::load_texture(texture_path);
    if (texture.id == 0) {
        MemFree(glyphs);
        MemFree(recs);
        return false;
    }

    Font next_font = {};
    next_font.baseSize = base_size;
    next_font.glyphCount = glyph_count;
    next_font.glyphPadding = glyph_padding;
    next_font.texture = texture;
    next_font.glyphs = glyphs;
    next_font.recs = recs;

    SetTextureFilter(next_font.texture, TEXTURE_FILTER_BILINEAR);
    unload_loaded_font(target);
    target.font = next_font;
    target.loaded = true;
    target.sdf = true;
    target.prebuilt_atlas = true;
    target.dynamic_glyphs = false;
    if (&target == &g_ui_font) {
        index_ui_font_glyphs(target.font);
    }
    return true;
}

int prebuilt_glyph_index(int codepoint) {
    const auto it = g_ui_glyph_indices.find(codepoint);
    if (it != g_ui_glyph_indices.end()) {
        return it->second;
    }
    return g_ui_fallback_glyph_index;
}

float prebuilt_glyph_advance(const Font& font, int index, float scale_factor, float spacing) {
    if (index < 0 || index >= font.glyphCount) {
        return spacing;
    }
    const GlyphInfo& glyph = font.glyphs[index];
    return (glyph.advanceX == 0 ? font.recs[index].width : static_cast<float>(glyph.advanceX)) * scale_factor + spacing;
}

Vector2 measure_prebuilt_text(const char* text, float font_size, float spacing) {
    Vector2 text_size = {0.0f, 0.0f};
    const Font font = g_ui_font.font;
    if (font.texture.id == 0 || text == nullptr || text[0] == '\0' || font.baseSize <= 0) {
        return text_size;
    }

    const int size = TextLength(text);
    int temp_byte_counter = 0;
    int byte_counter = 0;
    float text_width = 0.0f;
    float temp_text_width = 0.0f;
    float text_height = font_size;
    const float scale_factor = font_size / static_cast<float>(font.baseSize);

    for (int i = 0; i < size;) {
        ++byte_counter;
        int codepoint_byte_count = 0;
        const int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        const int index = prebuilt_glyph_index(codepoint);
        i += codepoint_byte_count;

        if (codepoint != '\n') {
            if (index >= 0 && index < font.glyphCount) {
                const GlyphInfo& glyph = font.glyphs[index];
                if (glyph.advanceX > 0) {
                    text_width += static_cast<float>(glyph.advanceX);
                } else {
                    text_width += font.recs[index].width + static_cast<float>(glyph.offsetX);
                }
            }
        } else {
            if (temp_text_width < text_width) {
                temp_text_width = text_width;
            }
            byte_counter = 0;
            text_width = 0.0f;
            text_height += font_size;
        }

        if (temp_byte_counter < byte_counter) {
            temp_byte_counter = byte_counter;
        }
    }

    if (temp_text_width < text_width) {
        temp_text_width = text_width;
    }
    text_size.x = temp_text_width * scale_factor + static_cast<float>(std::max(0, temp_byte_counter - 1)) * spacing;
    text_size.y = text_height;
    return text_size;
}

void draw_prebuilt_codepoint(const Font& font, int index, Vector2 position, float font_size, Color tint) {
    if (index < 0 || index >= font.glyphCount || font.baseSize <= 0) {
        return;
    }

    const float scale_factor = font_size / static_cast<float>(font.baseSize);
    const Rectangle dst_rec = {
        position.x + static_cast<float>(font.glyphs[index].offsetX) * scale_factor -
            static_cast<float>(font.glyphPadding) * scale_factor,
        position.y + static_cast<float>(font.glyphs[index].offsetY) * scale_factor -
            static_cast<float>(font.glyphPadding) * scale_factor,
        (font.recs[index].width + 2.0f * static_cast<float>(font.glyphPadding)) * scale_factor,
        (font.recs[index].height + 2.0f * static_cast<float>(font.glyphPadding)) * scale_factor,
    };
    const Rectangle src_rec = {
        font.recs[index].x - static_cast<float>(font.glyphPadding),
        font.recs[index].y - static_cast<float>(font.glyphPadding),
        font.recs[index].width + 2.0f * static_cast<float>(font.glyphPadding),
        font.recs[index].height + 2.0f * static_cast<float>(font.glyphPadding),
    };
    DrawTexturePro(font.texture, src_rec, dst_rec, {0.0f, 0.0f}, 0.0f, tint);
}

void draw_prebuilt_text(const char* text, Vector2 position, float font_size, float spacing, Color color) {
    const Font font = g_ui_font.font;
    if (font.texture.id == 0 || text == nullptr || text[0] == '\0' || font.baseSize <= 0) {
        return;
    }

    const int size = TextLength(text);
    const float scale_factor = font_size / static_cast<float>(font.baseSize);
    float text_offset_y = 0.0f;
    float text_offset_x = 0.0f;
    for (int i = 0; i < size;) {
        int codepoint_byte_count = 0;
        const int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        const int index = prebuilt_glyph_index(codepoint);

        if (codepoint == '\n') {
            text_offset_y += font_size;
            text_offset_x = 0.0f;
        } else {
            if (codepoint != ' ' && codepoint != '\t') {
                draw_prebuilt_codepoint(font, index,
                                        {position.x + text_offset_x, position.y + text_offset_y},
                                        font_size,
                                        color);
            }
            text_offset_x += prebuilt_glyph_advance(font, index, scale_factor, spacing);
        }

        i += codepoint_byte_count;
    }
}

void rebuild_one_font(loaded_font& target) {
    if (target.prebuilt_atlas) {
        return;
    }
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

bool seed_japanese_full_codepoints() {
    const std::string charset_path = find_ui_font_charset_path();
    if (charset_path.empty()) {
        return false;
    }

    const std::string charset_text = read_text_file(path_utils::from_utf8(charset_path));
    if (charset_text.empty()) {
        return false;
    }

    bool changed = false;
    const std::string combined_text = charset_text + reinterpret_cast<const char*>(kExtraUiGlyphs);
    int codepoint_count = 0;
    int* codepoints = LoadCodepoints(combined_text.c_str(), &codepoint_count);
    if (codepoints == nullptr) {
        return false;
    }
    for (int i = 0; i < codepoint_count; ++i) {
        if (codepoints[i] < 32) {
            continue;
        }
        changed = g_loaded_codepoints.insert(codepoints[i]).second || changed;
    }
    UnloadCodepoints(codepoints);
    return changed;
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

    g_ui_font = {.path = find_ui_font_path(), .atlas_metadata_path = find_ui_font_atlas_metadata_path(), .size_scale = kBodyFontSizeScale,
                 .spacing_offset = 0.0f, .prefer_sdf = true};
    g_loaded_codepoints.clear();
    seed_ascii_codepoints();

    if (load_prebuilt_sdf_font(g_ui_font)) {
        TraceLog(LOG_INFO, "raythm ui font: loaded prebuilt atlas %s", g_ui_font.atlas_metadata_path.c_str());
    } else {
        TraceLog(LOG_WARNING, "raythm ui font: prebuilt atlas unavailable; building fallback atlas once at startup");
        if (g_font_mode == font_locale_mode::japanese_ui && seed_japanese_full_codepoints()) {
            g_ui_font.dynamic_glyphs = false;
        }
        rebuild_fonts();
    }
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
    g_ui_font.atlas_metadata_path.clear();
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
    if (!g_ui_font.dynamic_glyphs) {
        return;
    }
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
    if (!g_ui_font.dynamic_glyphs) {
        return;
    }
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
    if (role == text_role::ui_body && !g_ui_glyph_indices.empty()) {
        return measure_prebuilt_text(text, adjusted_font_size, text_spacing(role, font_size, spacing));
    }
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
    if (role == text_role::ui_body && !g_ui_glyph_indices.empty()) {
        draw_prebuilt_text(text, draw_position, adjusted_font_size, text_spacing(role, font_size, spacing), color);
    } else {
        DrawTextEx(text_font(role), text, draw_position, adjusted_font_size,
                   text_spacing(role, font_size, spacing), color);
    }
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
