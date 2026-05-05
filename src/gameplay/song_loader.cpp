#include "song_loader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>

#include "chart_identity_store.h"
#include "path_utils.h"

namespace {
namespace fs = std::filesystem;

std::string trim(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

bool is_chart_file_path(const fs::path& path) {
    return path.extension() == ".rchart";
}

std::vector<fs::path> collect_chart_files_in_directory(const fs::path& directory) {
    std::map<std::string, fs::path> by_stem;
    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        return {};
    }

    for (const fs::directory_entry& chart_entry : fs::directory_iterator(directory)) {
        if (!chart_entry.is_regular_file() || !is_chart_file_path(chart_entry.path())) {
            continue;
        }

        const std::string stem = path_utils::to_utf8(chart_entry.path().stem());
        const auto existing = by_stem.find(stem);
        if (existing == by_stem.end()) {
            by_stem[stem] = chart_entry.path();
        }
    }

    std::vector<fs::path> result;
    result.reserve(by_stem.size());
    for (const auto& [stem, path] : by_stem) {
        (void)stem;
        result.push_back(path);
    }

    std::sort(result.begin(), result.end(), [](const fs::path& left, const fs::path& right) {
        if (left.stem() != right.stem()) {
            return left.stem().wstring() < right.stem().wstring();
        }
        return left.wstring() < right.wstring();
    });
    return result;
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

std::optional<std::string> extract_json_string(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    const size_t colon_pos = content.find(':', key_pos + token.size());
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    size_t value_start = colon_pos + 1;
    while (value_start < content.size() &&
           std::isspace(static_cast<unsigned char>(content[value_start])) != 0) {
        ++value_start;
    }

    if (value_start >= content.size() || content[value_start] != '"') {
        return std::nullopt;
    }

    ++value_start;
    size_t value_end = value_start;
    while (value_end < content.size()) {
        if (content[value_end] == '"' && content[value_end - 1] != '\\') {
            return content.substr(value_start, value_end - value_start);
        }
        ++value_end;
    }

    return std::nullopt;
}

std::optional<std::string> extract_json_number_token(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    const size_t colon_pos = content.find(':', key_pos + token.size());
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    size_t value_start = colon_pos + 1;
    while (value_start < content.size() &&
           std::isspace(static_cast<unsigned char>(content[value_start])) != 0) {
        ++value_start;
    }

    size_t value_end = value_start;
    while (value_end < content.size() && content[value_end] != ',' && content[value_end] != '}' &&
           content[value_end] != '\n' && content[value_end] != '\r') {
        ++value_end;
    }

    const std::string token_value = trim(content.substr(value_start, value_end - value_start));
    if (token_value.empty()) {
        return std::nullopt;
    }

    return token_value;
}

std::optional<int> parse_int(const std::string& value) {
    try {
        size_t parsed = 0;
        const int result = std::stoi(value, &parsed);
        if (parsed != value.size()) {
            return std::nullopt;
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<float> parse_float(const std::string& value) {
    try {
        size_t parsed = 0;
        const float result = std::stof(value, &parsed);
        if (parsed != value.size()) {
            return std::nullopt;
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<song_meta> parse_song_meta(const fs::path& song_json_path,
                                         std::vector<std::string>& errors) {
    const std::string content = read_file(song_json_path);
    if (content.empty()) {
        errors.push_back("Failed to read song metadata file: " + path_utils::to_utf8(song_json_path));
        return std::nullopt;
    }

    song_meta meta;

    const std::optional<std::string> song_id = extract_json_string(content, "songId");
    const std::optional<std::string> title = extract_json_string(content, "title");
    const std::optional<std::string> artist = extract_json_string(content, "artist");
    const std::optional<std::string> audio_file = extract_json_string(content, "audioFile");
    const std::optional<std::string> jacket_file = extract_json_string(content, "jacketFile");
    const std::optional<std::string> difficulty_bpm = extract_json_number_token(content, "baseBpm");
    const std::optional<std::string> preview_start_ms = extract_json_number_token(content, "previewStartMs");
    const std::optional<std::string> song_version = extract_json_number_token(content, "songVersion");

    if (!song_id.has_value()) {
        errors.push_back("Missing required field songId in " + path_utils::to_utf8(song_json_path));
    } else {
        meta.song_id = *song_id;
    }

    if (!title.has_value()) {
        errors.push_back("Missing required field title in " + path_utils::to_utf8(song_json_path));
    } else {
        meta.title = *title;
    }

    if (!artist.has_value()) {
        errors.push_back("Missing required field artist in " + path_utils::to_utf8(song_json_path));
    } else {
        meta.artist = *artist;
    }

    if (!audio_file.has_value()) {
        errors.push_back("Missing required field audioFile in " + path_utils::to_utf8(song_json_path));
    } else {
        meta.audio_file = *audio_file;
    }

    if (!jacket_file.has_value()) {
        errors.push_back("Missing required field jacketFile in " + path_utils::to_utf8(song_json_path));
    } else {
        meta.jacket_file = *jacket_file;
    }

    if (!difficulty_bpm.has_value()) {
        errors.push_back("Missing required field baseBpm in " + path_utils::to_utf8(song_json_path));
    } else {
        const std::optional<float> parsed = parse_float(*difficulty_bpm);
        if (!parsed.has_value()) {
            errors.push_back("baseBpm must be a number in " + path_utils::to_utf8(song_json_path));
        } else {
            meta.base_bpm = *parsed;
        }
    }

    if (preview_start_ms.has_value()) {
        const std::optional<int> parsed = parse_int(*preview_start_ms);
        if (!parsed.has_value()) {
            errors.push_back("previewStartMs must be an integer in " + path_utils::to_utf8(song_json_path));
        } else {
            meta.preview_start_ms = *parsed;
            meta.preview_start_seconds = static_cast<float>(*parsed) / 1000.0f;
        }
    } else {
        errors.push_back("Missing required field previewStartMs in " + path_utils::to_utf8(song_json_path));
    }

    const std::optional<std::string> sns_youtube = extract_json_string(content, "snsYoutube");
    if (sns_youtube.has_value()) {
        meta.sns_youtube = *sns_youtube;
    }
    const std::optional<std::string> sns_niconico = extract_json_string(content, "snsNiconico");
    if (sns_niconico.has_value()) {
        meta.sns_niconico = *sns_niconico;
    }
    const std::optional<std::string> sns_x = extract_json_string(content, "snsX");
    if (sns_x.has_value()) {
        meta.sns_x = *sns_x;
    }

    if (!song_version.has_value()) {
        errors.push_back("Missing required field songVersion in " + path_utils::to_utf8(song_json_path));
    } else {
        const std::optional<int> parsed = parse_int(*song_version);
        if (!parsed.has_value()) {
            errors.push_back("songVersion must be an integer in " + path_utils::to_utf8(song_json_path));
        } else {
            meta.song_version = *parsed;
        }
    }

    if (!errors.empty()) {
        return std::nullopt;
    }

    return meta;
}

}

song_load_result song_loader::load_all(const std::string& songs_dir) {
    song_load_result result;
    const fs::path root = path_utils::from_utf8(songs_dir);

    if (!fs::exists(root) || !fs::is_directory(root)) {
        result.errors.push_back("Songs directory does not exist: " + path_utils::to_utf8(root));
        return result;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(root)) {
        if (!entry.is_directory()) {
            continue;
        }

        const fs::path song_dir = entry.path();
        const fs::path song_json_path = song_dir / "song.json";
        if (!fs::exists(song_json_path)) {
            result.errors.push_back("Skipping " + path_utils::to_utf8(song_dir) + ": missing song.json");
            continue;
        }

        std::vector<std::string> song_errors;
        const std::optional<song_meta> meta =
            parse_song_meta(song_json_path, song_errors);
        if (!meta.has_value()) {
            result.errors.insert(result.errors.end(), song_errors.begin(), song_errors.end());
            continue;
        }

        song_data song;
        song.meta = *meta;
        song.directory = path_utils::to_utf8(song_dir);

        const fs::path charts_dir = song_dir / "charts";
        if (fs::exists(charts_dir) && fs::is_directory(charts_dir)) {
            for (const fs::path& chart_path : collect_chart_files_in_directory(charts_dir)) {
                song.chart_paths.push_back(path_utils::to_utf8(chart_path));
            }
            std::sort(song.chart_paths.begin(), song.chart_paths.end());
        }

        result.songs.push_back(song);
    }

    std::sort(result.songs.begin(), result.songs.end(), [](const song_data& left, const song_data& right) {
        return left.meta.title < right.meta.title;
    });

    return result;
}

song_load_result song_loader::load_directory(const std::string& song_dir_utf8) {
    song_load_result result;
    const fs::path song_dir = path_utils::from_utf8(song_dir_utf8);
    if (!fs::exists(song_dir) || !fs::is_directory(song_dir)) {
        result.errors.push_back("Song directory does not exist: " + path_utils::to_utf8(song_dir));
        return result;
    }

    const fs::path song_json_path = song_dir / "song.json";
    if (!fs::exists(song_json_path)) {
        result.errors.push_back("Skipping " + path_utils::to_utf8(song_dir) + ": missing song.json");
        return result;
    }

    std::vector<std::string> song_errors;
    const std::optional<song_meta> meta =
        parse_song_meta(song_json_path, song_errors);
    if (!meta.has_value()) {
        result.errors = std::move(song_errors);
        return result;
    }

    song_data song;
    song.meta = *meta;
    song.directory = path_utils::to_utf8(song_dir);

    const fs::path charts_dir = song_dir / "charts";
    if (fs::exists(charts_dir) && fs::is_directory(charts_dir)) {
        for (const fs::path& chart_path : collect_chart_files_in_directory(charts_dir)) {
            song.chart_paths.push_back(path_utils::to_utf8(chart_path));
        }
        std::sort(song.chart_paths.begin(), song.chart_paths.end());
    }

    result.songs.push_back(std::move(song));
    return result;
}

chart_parse_result song_loader::load_chart(const std::string& path) {
    return chart_parser::parse(path);
}

void song_loader::attach_external_charts(const std::string& charts_dir, std::vector<song_data>& songs) {
    const fs::path root = path_utils::from_utf8(charts_dir);
    if (!fs::exists(root) || !fs::is_directory(root)) {
        return;
    }

    for (const fs::path& chart_path : collect_chart_files_in_directory(root)) {
        const fs::directory_entry entry(chart_path);
        const chart_parse_result parse_result = chart_parser::parse(path_utils::to_utf8(entry.path()));
        if (!parse_result.success || !parse_result.data.has_value()) {
            continue;
        }

        std::string song_id = chart_identity::find_song_id(parse_result.data->meta.chart_id).value_or("");
        if (song_id.empty() && !parse_result.data->meta.song_id.empty()) {
            song_id = parse_result.data->meta.song_id;
            chart_identity::put(parse_result.data->meta.chart_id, song_id);
        }

        for (song_data& song : songs) {
            if (song.meta.song_id == song_id) {
                song.chart_paths.push_back(path_utils::to_utf8(entry.path()));
                break;
            }
        }
    }
}
