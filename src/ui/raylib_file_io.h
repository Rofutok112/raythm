#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "file_io.h"
#include "path_utils.h"
#include "raylib.h"

namespace raylib_file_io {

inline std::string extension_for_raylib(const std::filesystem::path& path) {
    return path.extension().string();
}

inline Image load_image(const std::filesystem::path& path) {
    std::vector<unsigned char> bytes = file_io::read_binary_file(path);
    if (bytes.empty()) {
        return {};
    }

    const std::string extension = extension_for_raylib(path);
    Image image = {};
    if (!extension.empty()) {
        image = LoadImageFromMemory(extension.c_str(), bytes.data(), static_cast<int>(bytes.size()));
    }
    if (image.data == nullptr) {
        image = LoadImageFromMemory(".png", bytes.data(), static_cast<int>(bytes.size()));
    }
    if (image.data == nullptr) {
        image = LoadImageFromMemory(".jpg", bytes.data(), static_cast<int>(bytes.size()));
    }
    if (image.data == nullptr) {
        image = LoadImageFromMemory(".jpeg", bytes.data(), static_cast<int>(bytes.size()));
    }
    return image;
}

inline Image load_image_utf8(const std::string& utf8_path) {
    return load_image(path_utils::from_utf8(utf8_path));
}

inline Texture2D load_texture(const std::filesystem::path& path) {
    Image image = load_image(path);
    if (image.data == nullptr) {
        return {};
    }

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

inline Texture2D load_texture_utf8(const std::string& utf8_path) {
    return load_texture(path_utils::from_utf8(utf8_path));
}

inline Font load_font(const std::filesystem::path& path,
                      int font_size,
                      int* codepoints,
                      int codepoint_count) {
    std::vector<unsigned char> bytes = file_io::read_binary_file(path);
    if (bytes.empty()) {
        return {};
    }

    const std::string extension = extension_for_raylib(path);
    if (extension.empty()) {
        return {};
    }
    return LoadFontFromMemory(extension.c_str(),
                              bytes.data(),
                              static_cast<int>(bytes.size()),
                              font_size,
                              codepoints,
                              codepoint_count);
}

inline Font load_font_utf8(const std::string& utf8_path,
                           int font_size,
                           int* codepoints,
                           int codepoint_count) {
    return load_font(path_utils::from_utf8(utf8_path), font_size, codepoints, codepoint_count);
}

}  // namespace raylib_file_io

