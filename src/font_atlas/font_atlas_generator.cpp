#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "raylib.h"

namespace {

constexpr int kAtlasBaseSize = 64;
constexpr int kAtlasPadding = 0;
constexpr const char8_t* kExtraUiGlyphs = u8"✓✕×○●◎◇◆□■△▲▽▼◀▶↑↓←→…‥・ー〜～／＼｜（）[]{}「」『』【】";

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::vector<int> build_codepoint_set(const std::string& charset_text) {
    std::set<int> unique;
    for (int cp = 32; cp <= 126; ++cp) {
        unique.insert(cp);
    }

    const std::string combined_text = charset_text + reinterpret_cast<const char*>(kExtraUiGlyphs);
    int codepoint_count = 0;
    int* codepoints = LoadCodepoints(combined_text.c_str(), &codepoint_count);
    if (codepoints != nullptr) {
        for (int i = 0; i < codepoint_count; ++i) {
            if (codepoints[i] >= 32) {
                unique.insert(codepoints[i]);
            }
        }
        UnloadCodepoints(codepoints);
    }

    return {unique.begin(), unique.end()};
}

bool write_metadata(const std::filesystem::path& metadata_path,
                    const std::filesystem::path& texture_path,
                    const Font& font) {
    std::ofstream out(metadata_path, std::ios::binary);
    if (!out) {
        return false;
    }

    out << "raythm-rfont-sdf-v1\n";
    out << "texture " << texture_path.filename().generic_string() << "\n";
    out << "baseSize " << font.baseSize << "\n";
    out << "glyphPadding " << font.glyphPadding << "\n";
    out << "glyphCount " << font.glyphCount << "\n";

    for (int i = 0; i < font.glyphCount; ++i) {
        const GlyphInfo& glyph = font.glyphs[i];
        const Rectangle& rec = font.recs[i];
        out << "glyph "
            << glyph.value << " "
            << rec.x << " " << rec.y << " " << rec.width << " " << rec.height << " "
            << glyph.offsetX << " " << glyph.offsetY << " " << glyph.advanceX << "\n";
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: font_atlas_generator <font.ttf> <charset.txt> <output-base>\n";
        return 2;
    }

    const std::filesystem::path font_path = std::filesystem::path(argv[1]);
    const std::filesystem::path charset_path = std::filesystem::path(argv[2]);
    const std::filesystem::path output_base = std::filesystem::path(argv[3]);
    std::filesystem::path png_path = output_base;
    png_path.replace_extension(".png");
    const std::filesystem::path metadata_path = std::filesystem::path(output_base).replace_extension(".rfont");

    const std::string charset_text = read_text_file(charset_path);
    if (charset_text.empty()) {
        std::cerr << "failed to read charset: " << charset_path << "\n";
        return 1;
    }

    std::vector<int> codepoints = build_codepoint_set(charset_text);
    if (codepoints.empty()) {
        std::cerr << "charset produced no codepoints\n";
        return 1;
    }

    int file_size = 0;
    unsigned char* file_data = LoadFileData(font_path.string().c_str(), &file_size);
    if (file_data == nullptr || file_size <= 0) {
        std::cerr << "failed to read font: " << font_path << "\n";
        return 1;
    }

    Font font = {};
    font.baseSize = kAtlasBaseSize;
    font.glyphCount = static_cast<int>(codepoints.size());
    font.glyphPadding = kAtlasPadding;
    font.glyphs = LoadFontData(file_data, file_size, kAtlasBaseSize,
                               codepoints.data(), static_cast<int>(codepoints.size()), FONT_SDF);
    UnloadFileData(file_data);

    if (font.glyphs == nullptr) {
        std::cerr << "failed to generate SDF glyph data\n";
        return 1;
    }

    Image atlas = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount,
                                    font.baseSize, font.glyphPadding, 1);
    if (atlas.data == nullptr || font.recs == nullptr) {
        std::cerr << "failed to pack font atlas\n";
        UnloadFontData(font.glyphs, font.glyphCount);
        MemFree(font.recs);
        return 1;
    }

    std::filesystem::create_directories(png_path.parent_path());
    const bool texture_written = ExportImage(atlas, png_path.string().c_str());
    const bool metadata_written = write_metadata(metadata_path, png_path, font);

    std::cout << "generated " << font.glyphCount << " glyphs, atlas "
              << atlas.width << "x" << atlas.height << "\n";
    std::cout << png_path << "\n" << metadata_path << "\n";

    UnloadImage(atlas);
    UnloadFontData(font.glyphs, font.glyphCount);
    MemFree(font.recs);

    if (!texture_written || !metadata_written) {
        std::cerr << "failed to write atlas files\n";
        return 1;
    }
    return 0;
}
