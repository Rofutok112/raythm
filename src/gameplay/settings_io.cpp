#include "settings_io.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "app_paths.h"

namespace {
namespace fs = std::filesystem;
constexpr float kMinNoteSpeed = 0.010f;
constexpr float kMaxNoteSpeed = 0.200f;

fs::path settings_path() {
    return app_paths::settings_path();
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// JSON から文字列値を取り出す。
std::optional<std::string> extract_string(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string::npos) return std::nullopt;

    const size_t colon_pos = content.find(':', key_pos + token.size());
    if (colon_pos == std::string::npos) return std::nullopt;

    size_t start = colon_pos + 1;
    while (start < content.size() && std::isspace(static_cast<unsigned char>(content[start]))) ++start;
    if (start >= content.size() || content[start] != '"') return std::nullopt;
    ++start;

    std::string result;
    for (size_t i = start; i < content.size(); ++i) {
        if (content[i] == '"') return result;
        if (content[i] == '\\' && i + 1 < content.size()) {
            result += content[++i];
        } else {
            result += content[i];
        }
    }
    return std::nullopt;
}

// JSON から数値トークンを取り出す。
std::optional<std::string> extract_number_token(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string::npos) return std::nullopt;

    const size_t colon_pos = content.find(':', key_pos + token.size());
    if (colon_pos == std::string::npos) return std::nullopt;

    size_t start = colon_pos + 1;
    while (start < content.size() && std::isspace(static_cast<unsigned char>(content[start]))) ++start;

    size_t end = start;
    while (end < content.size() && (std::isdigit(static_cast<unsigned char>(content[end])) ||
           content[end] == '.' || content[end] == '-' || content[end] == '+')) ++end;

    if (end == start) return std::nullopt;
    return std::string(content, start, end - start);
}

// JSON から bool 値を取り出す。
std::optional<bool> extract_bool(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string::npos) return std::nullopt;

    const size_t colon_pos = content.find(':', key_pos + token.size());
    if (colon_pos == std::string::npos) return std::nullopt;

    size_t start = colon_pos + 1;
    while (start < content.size() && std::isspace(static_cast<unsigned char>(content[start]))) ++start;

    if (content.compare(start, 4, "true") == 0) return true;
    if (content.compare(start, 5, "false") == 0) return false;
    return std::nullopt;
}

// カンマ区切り整数文字列 → KeyboardKey 配列にパース。
template <size_t N>
void parse_key_array(const std::string& csv, std::array<KeyboardKey, N>& keys) {
    size_t index = 0;
    std::istringstream stream(csv);
    std::string token;
    while (std::getline(stream, token, ',') && index < N) {
        try {
            keys[index++] = static_cast<KeyboardKey>(std::stoi(token));
        } catch (...) {
            // 不正な値はスキップ
        }
    }
}

// KeyboardKey 配列 → カンマ区切り整数文字列に変換。
template <size_t N>
std::string keys_to_csv(const std::array<KeyboardKey, N>& keys) {
    std::string result;
    for (size_t i = 0; i < N; ++i) {
        if (i > 0) result += ',';
        result += std::to_string(static_cast<int>(keys[i]));
    }
    return result;
}

}  // namespace

void load_settings(game_settings& settings) {
    const std::string content = read_file(settings_path());
    if (content.empty()) return;

    if (auto v = extract_number_token(content, "cameraAngle"))
        settings.camera_angle_degrees = std::clamp(std::stof(*v), 5.0f, 90.0f);
    if (auto v = extract_number_token(content, "laneWidth"))
        settings.lane_width = std::clamp(std::stof(*v), 0.6f, 5.0f);
    if (auto v = extract_number_token(content, "noteSpeed"))
        settings.note_speed = std::clamp(std::stof(*v), kMinNoteSpeed, kMaxNoteSpeed);
    if (auto v = extract_number_token(content, "globalNoteOffsetMs"))
        settings.global_note_offset_ms = std::clamp(std::stoi(*v), -10000, 10000);
    if (auto v = extract_number_token(content, "bgmVolume"))
        settings.bgm_volume = std::clamp(std::stof(*v), 0.0f, 1.0f);
    if (auto v = extract_number_token(content, "seVolume"))
        settings.se_volume = std::clamp(std::stof(*v), 0.0f, 1.0f);
    if (auto v = extract_number_token(content, "targetFps"))
        settings.target_fps = std::stoi(*v);
    if (auto v = extract_number_token(content, "resolutionIndex"))
        settings.resolution_index = std::clamp(std::stoi(*v), 0, kResolutionPresetCount - 1);
    if (auto v = extract_bool(content, "fullscreen"))
        settings.fullscreen = *v;
    if (auto v = extract_bool(content, "darkMode"))
        settings.dark_mode = *v;

    if (auto v = extract_string(content, "keys4"))
        parse_key_array(*v, settings.keys.keys_4);
    if (auto v = extract_string(content, "keys6"))
        parse_key_array(*v, settings.keys.keys_6);
}

void save_settings(const game_settings& settings) {
    app_paths::ensure_directories();
    std::ofstream out(settings_path());
    if (!out.is_open()) return;

    out << "{\n";
    out << "  \"cameraAngle\": " << settings.camera_angle_degrees << ",\n";
    out << "  \"laneWidth\": " << settings.lane_width << ",\n";
    out << "  \"noteSpeed\": " << settings.note_speed << ",\n";
    out << "  \"globalNoteOffsetMs\": " << settings.global_note_offset_ms << ",\n";
    out << "  \"bgmVolume\": " << settings.bgm_volume << ",\n";
    out << "  \"seVolume\": " << settings.se_volume << ",\n";
    out << "  \"targetFps\": " << settings.target_fps << ",\n";
    out << "  \"resolutionIndex\": " << settings.resolution_index << ",\n";
    out << "  \"fullscreen\": " << (settings.fullscreen ? "true" : "false") << ",\n";
    out << "  \"darkMode\": " << (settings.dark_mode ? "true" : "false") << ",\n";
    out << "  \"keys4\": \"" << keys_to_csv(settings.keys.keys_4) << "\",\n";
    out << "  \"keys6\": \"" << keys_to_csv(settings.keys.keys_6) << "\"\n";
    out << "}\n";
}
