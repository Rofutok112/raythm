#pragma once

#include <filesystem>
#include <string>

namespace path_utils {

inline std::filesystem::path from_utf8(const std::string& utf8) {
#ifdef _WIN32
    std::u8string encoded;
    encoded.reserve(utf8.size());
    for (const unsigned char ch : utf8) {
        encoded.push_back(static_cast<char8_t>(ch));
    }
    return std::filesystem::path(encoded);
#else
    return std::filesystem::path(utf8);
#endif
}

inline std::string to_utf8(const std::filesystem::path& path) {
#ifdef _WIN32
    const std::u8string utf8 = path.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t ch : utf8) {
        result.push_back(static_cast<char>(ch));
    }
    return result;
#else
    return path.string();
#endif
}

inline std::filesystem::path join_utf8(const std::string& base_utf8, const std::string& child_utf8) {
    return from_utf8(base_utf8) / from_utf8(child_utf8);
}

}  // namespace path_utils
