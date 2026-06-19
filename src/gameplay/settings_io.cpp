#include "settings_io.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

#include "app_paths.h"
#include "localization/localization.h"

namespace {
namespace fs = std::filesystem;
constexpr float kMinNoteSpeed = 0.010f;
constexpr float kMaxNoteSpeed = 0.200f;
constexpr float kMinNoteHeight = 0.5f;
constexpr float kMaxNoteHeight = 2.0f;

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
        settings.lane_width = std::clamp(std::stof(*v), kMinLaneWidth, kMaxLaneWidth);
    if (auto v = extract_number_token(content, "laneFogHiddenPercent"))
        settings.lane_fog_hidden_percent =
            std::clamp(std::stof(*v), kMinLaneFogHiddenPercent, kMaxLaneFogHiddenPercent);
    if (auto v = extract_number_token(content, "noteSpeed"))
        settings.note_speed = std::clamp(std::stof(*v), kMinNoteSpeed, kMaxNoteSpeed);
    if (auto v = extract_number_token(content, "noteHeight"))
        settings.note_height = std::clamp(std::stof(*v), kMinNoteHeight, kMaxNoteHeight);
    if (auto v = extract_number_token(content, "globalNoteOffsetMs"))
        settings.global_note_offset_ms = std::clamp(std::stoi(*v), -10000, 10000);
    if (auto v = extract_number_token(content, "bgmVolume"))
        settings.bgm_volume = std::clamp(std::stof(*v), 0.0f, 1.0f);
    if (auto v = extract_number_token(content, "seVolume"))
        settings.se_volume = std::clamp(std::stof(*v), 0.0f, 1.0f);
    if (auto v = extract_number_token(content, "hitsoundPanStrength"))
        settings.hitsound_pan_strength = std::clamp(std::stof(*v), 0.0f, 1.0f);
    if (auto v = extract_bool(content, "loudnessNormalizationEnabled"))
        settings.loudness_normalization_enabled = *v;
    if (auto v = extract_number_token(content, "targetFps"))
        settings.target_fps = sanitize_target_fps(std::stoi(*v));
    if (auto v = extract_number_token(content, "resolutionIndex"))
        settings.resolution_index = std::clamp(std::stoi(*v), 0, kResolutionPresetCount - 1);
    if (auto v = extract_number_token(content, "windowedWidth"))
        settings.windowed_width = std::clamp(std::stoi(*v), 640, 7680);
    if (auto v = extract_number_token(content, "windowedHeight"))
        settings.windowed_height = std::clamp(std::stoi(*v), 360, 4320);
    settings.windowed_width = kDefaultWindowedWidth;
    settings.windowed_height = kDefaultWindowedHeight;
    if (auto v = extract_bool(content, "fullscreen"))
        settings.fullscreen = *v;
    if (auto v = extract_bool(content, "windowMaximized"))
        settings.window_maximized = *v;
    if (auto v = extract_bool(content, "darkMode"))
        settings.dark_mode = *v;
    if (auto v = extract_string(content, "language"))
        settings.ui_locale = localization::parse_locale_code_or_default(*v);

    if (auto v = extract_string(content, "keys4"))
        parse_key_array(*v, settings.keys.keys_4);
    if (auto v = extract_string(content, "keys6"))
        parse_key_array(*v, settings.keys.keys_6);
}

void initialize_settings_storage(const game_settings& defaults) {
    app_paths::ensure_directories();

    std::error_code ec;
    if (fs::exists(settings_path(), ec) && !ec) {
        return;
    }

    save_settings(defaults);
}

void save_settings(const game_settings& settings) {
    app_paths::ensure_directories();
    std::ofstream out(settings_path());
    if (!out.is_open()) return;

    out << "{\n";
    out << "  \"cameraAngle\": " << settings.camera_angle_degrees << ",\n";
    out << "  \"laneWidth\": " << settings.lane_width << ",\n";
    out << "  \"laneFogHiddenPercent\": " << settings.lane_fog_hidden_percent << ",\n";
    out << "  \"noteSpeed\": " << settings.note_speed << ",\n";
    out << "  \"noteHeight\": " << settings.note_height << ",\n";
    out << "  \"globalNoteOffsetMs\": " << settings.global_note_offset_ms << ",\n";
    out << "  \"bgmVolume\": " << settings.bgm_volume << ",\n";
    out << "  \"seVolume\": " << settings.se_volume << ",\n";
    out << "  \"hitsoundPanStrength\": " << settings.hitsound_pan_strength << ",\n";
    out << "  \"loudnessNormalizationEnabled\": "
        << (settings.loudness_normalization_enabled ? "true" : "false") << ",\n";
    out << "  \"targetFps\": " << sanitize_target_fps(settings.target_fps) << ",\n";
    out << "  \"resolutionIndex\": " << settings.resolution_index << ",\n";
    out << "  \"windowedWidth\": " << settings.windowed_width << ",\n";
    out << "  \"windowedHeight\": " << settings.windowed_height << ",\n";
    out << "  \"fullscreen\": " << (settings.fullscreen ? "true" : "false") << ",\n";
    out << "  \"windowMaximized\": " << (settings.window_maximized ? "true" : "false") << ",\n";
    out << "  \"darkMode\": " << (settings.dark_mode ? "true" : "false") << ",\n";
    out << "  \"language\": \"" << localization::locale_code(settings.ui_locale) << "\",\n";
    out << "  \"keys4\": \"" << keys_to_csv(settings.keys.keys_4) << "\",\n";
    out << "  \"keys6\": \"" << keys_to_csv(settings.keys.keys_6) << "\"\n";
    out << "}\n";
}
