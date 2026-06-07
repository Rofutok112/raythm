#include "song_loader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>

#include "managed_content_storage.h"
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
    const managed_content_storage::managed_file_read_result managed =
        managed_content_storage::read_managed_file(path);
    if (managed.managed) {
        if (!managed.success || managed.bytes.empty()) {
            return {};
        }
        return std::string(managed.bytes.begin(), managed.bytes.end());
    }

    std::ifstream input(path);
    if (!input.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool can_read_file(const fs::path& path) {
    std::error_code ec;
    if (fs::exists(path, ec) && fs::is_regular_file(path, ec)) {
        return true;
    }
    const managed_content_storage::managed_file_read_result managed =
        managed_content_storage::read_managed_file(path);
    return managed.managed && managed.success;
}

std::optional<size_t> find_json_key(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    size_t search_start = 0;
    while (true) {
        const size_t key_pos = content.find(token, search_start);
        if (key_pos == std::string::npos) {
            return std::nullopt;
        }
        size_t prefix = key_pos;
        while (prefix > 0 && std::isspace(static_cast<unsigned char>(content[prefix - 1])) != 0) {
            --prefix;
        }
        if (prefix == 0 || content[prefix - 1] == '{' || content[prefix - 1] == ',') {
            return key_pos;
        }
        search_start = key_pos + token.size();
    }
}

std::optional<std::string> extract_json_string(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const std::optional<size_t> found_key = find_json_key(content, key);
    if (!found_key.has_value()) {
        return std::nullopt;
    }
    const size_t key_pos = *found_key;

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
    const std::optional<size_t> found_key = find_json_key(content, key);
    if (!found_key.has_value()) {
        return std::nullopt;
    }
    const size_t key_pos = *found_key;

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

std::optional<std::string> extract_json_array(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const std::optional<size_t> found_key = find_json_key(content, key);
    if (!found_key.has_value()) {
        return std::nullopt;
    }
    const size_t key_pos = *found_key;

    const size_t colon_pos = content.find(':', key_pos + token.size());
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    size_t value_start = colon_pos + 1;
    while (value_start < content.size() &&
           std::isspace(static_cast<unsigned char>(content[value_start])) != 0) {
        ++value_start;
    }
    if (value_start >= content.size() || content[value_start] != '[') {
        return std::nullopt;
    }

    bool in_string = false;
    bool escaped = false;
    int depth = 0;
    for (size_t index = value_start; index < content.size(); ++index) {
        const char ch = content[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (in_string && ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return content.substr(value_start + 1, index - value_start - 1);
            }
        }
    }
    return std::nullopt;
}

std::vector<std::string> extract_json_object_array_values(const std::string& array_content) {
    std::vector<std::string> values;
    bool in_string = false;
    bool escaped = false;
    int depth = 0;
    size_t object_start = std::string::npos;

    for (size_t index = 0; index < array_content.size(); ++index) {
        const char ch = array_content[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (in_string && ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if (ch == '{') {
            if (depth == 0) {
                object_start = index;
            }
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && object_start != std::string::npos) {
                values.push_back(array_content.substr(object_start, index - object_start + 1));
                object_start = std::string::npos;
            }
        }
    }

    return values;
}

std::vector<std::string> extract_json_string_array_values(const std::string& array_content) {
    std::vector<std::string> values;
    bool in_string = false;
    bool escaped = false;
    std::string current;

    for (const char ch : array_content) {
        if (!in_string) {
            if (ch == '"') {
                in_string = true;
                current.clear();
            }
            continue;
        }

        if (escaped) {
            switch (ch) {
                case 'n': current.push_back('\n'); break;
                case 'r': current.push_back('\r'); break;
                case 't': current.push_back('\t'); break;
                default: current.push_back(ch); break;
            }
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
        } else if (ch == '"') {
            values.push_back(current);
            in_string = false;
        } else {
            current.push_back(ch);
        }
    }

    return values;
}

std::optional<timing_event_type> parse_timing_type(const std::string& value) {
    if (value == "bpm") {
        return timing_event_type::bpm;
    }
    if (value == "meter") {
        return timing_event_type::meter;
    }
    return std::nullopt;
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

std::vector<timing_event> parse_song_timing_events(const std::string& content,
                                                   const fs::path& song_json_path,
                                                   std::vector<std::string>& errors) {
    std::vector<timing_event> events;
    const std::optional<std::string> timing_array = extract_json_array(content, "timingEvents");
    if (!timing_array.has_value()) {
        if (find_json_key(content, "timingEvents").has_value()) {
            errors.push_back("timingEvents must be an array in " + path_utils::to_utf8(song_json_path));
        }
        return events;
    }

    const std::vector<std::string> objects = extract_json_object_array_values(*timing_array);
    if (objects.empty() && !trim(*timing_array).empty()) {
        errors.push_back("timingEvents must contain objects in " + path_utils::to_utf8(song_json_path));
        return events;
    }
    for (const std::string& object : objects) {
        const std::optional<std::string> type_token = extract_json_string(object, "type");
        const std::optional<std::string> tick_token = extract_json_number_token(object, "tick");
        if (!type_token.has_value() || !tick_token.has_value()) {
            errors.push_back("timingEvents entries must include type and tick in " + path_utils::to_utf8(song_json_path));
            continue;
        }

        const std::optional<timing_event_type> type = parse_timing_type(*type_token);
        const std::optional<int> tick = parse_int(*tick_token);
        if (!type.has_value() || !tick.has_value() || *tick < 0) {
            errors.push_back("timingEvents entries have invalid type or tick in " + path_utils::to_utf8(song_json_path));
            continue;
        }

        timing_event event;
        event.type = *type;
        event.tick = *tick;
        if (*type == timing_event_type::bpm) {
            const std::optional<std::string> bpm_token = extract_json_number_token(object, "bpm");
            const std::optional<float> bpm = bpm_token.has_value() ? parse_float(*bpm_token) : std::nullopt;
            if (!bpm.has_value() || *bpm <= 0.0f) {
                errors.push_back("bpm timingEvents entries must include positive bpm in " + path_utils::to_utf8(song_json_path));
                continue;
            }
            event.bpm = *bpm;
        } else {
            const std::optional<std::string> numerator_token = extract_json_number_token(object, "numerator");
            const std::optional<std::string> denominator_token = extract_json_number_token(object, "denominator");
            const std::optional<int> numerator = numerator_token.has_value() ? parse_int(*numerator_token) : std::nullopt;
            const std::optional<int> denominator = denominator_token.has_value() ? parse_int(*denominator_token) : std::nullopt;
            if (!numerator.has_value() || !denominator.has_value() || *numerator <= 0 || *denominator <= 0) {
                errors.push_back("meter timingEvents entries must include positive numerator and denominator in " +
                                 path_utils::to_utf8(song_json_path));
                continue;
            }
            event.numerator = *numerator;
            event.denominator = *denominator;
        }
        events.push_back(event);
    }

    return events;
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
    const std::optional<std::string> genre = extract_json_string(content, "genre");
    const std::optional<std::string> genres = extract_json_array(content, "genres");
    const std::optional<std::string> keywords = extract_json_array(content, "keywords");
    const std::optional<std::string> audio_file = extract_json_string(content, "audioFile");
    const std::optional<std::string> jacket_file = extract_json_string(content, "jacketFile");
    const std::optional<std::string> difficulty_bpm = extract_json_number_token(content, "baseBpm");
    const std::optional<std::string> offset = extract_json_number_token(content, "offset");
    const std::optional<std::string> duration_sec = extract_json_number_token(content, "durationSec");
    const std::optional<std::string> preview_start_ms = extract_json_number_token(content, "previewStartMs");
    const std::optional<std::string> song_version = extract_json_number_token(content, "songVersion");
    const std::optional<std::string> chart_count = extract_json_number_token(content, "chartCount");

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

    if (genre.has_value()) {
        meta.genre = *genre;
    }
    if (genres.has_value()) {
        meta.genres = extract_json_string_array_values(*genres);
    }
    if (meta.genre.empty() && !meta.genres.empty()) {
        meta.genre = meta.genres.front();
    }
    if (meta.genres.empty() && !meta.genre.empty()) {
        meta.genres.push_back(meta.genre);
    }
    if (keywords.has_value()) {
        meta.keywords = extract_json_string_array_values(*keywords);
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

    if (duration_sec.has_value()) {
        const std::optional<float> parsed = parse_float(*duration_sec);
        if (!parsed.has_value()) {
            errors.push_back("durationSec must be a number in " + path_utils::to_utf8(song_json_path));
        } else {
            meta.duration_seconds = *parsed;
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

    if (offset.has_value()) {
        const std::optional<int> parsed = parse_int(*offset);
        if (!parsed.has_value()) {
            errors.push_back("offset must be an integer in " + path_utils::to_utf8(song_json_path));
        } else {
            meta.offset = *parsed;
            meta.has_offset = true;
        }
    }

    meta.timing_events = parse_song_timing_events(content, song_json_path, errors);

    if (chart_count.has_value()) {
        const std::optional<int> parsed = parse_int(*chart_count);
        if (parsed.has_value()) {
            meta.chart_count = std::max(0, *parsed);
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
        return result;
    }

    for (const fs::directory_entry& entry : fs::directory_iterator(root)) {
        if (!entry.is_directory()) {
            continue;
        }

        const fs::path song_dir = entry.path();
        const fs::path song_json_path = song_dir / "song.json";
        if (!can_read_file(song_json_path)) {
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
    if (!can_read_file(song_json_path)) {
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
    if (std::optional<managed_content_storage::package_manifest> manifest =
            managed_content_storage::read_manifest(song_dir);
        manifest.has_value() && managed_content_storage::has_encrypted_assets(*manifest)) {
        for (const managed_content_storage::chart_manifest_entry& chart : manifest->charts) {
            if (!chart.local_chart_id.empty()) {
                song.chart_paths.push_back(path_utils::to_utf8(
                    managed_content_storage::chart_file_path(song_dir, chart.local_chart_id)));
            }
        }
        std::sort(song.chart_paths.begin(), song.chart_paths.end());
        song.chart_paths.erase(std::unique(song.chart_paths.begin(), song.chart_paths.end()), song.chart_paths.end());
    }

    result.songs.push_back(std::move(song));
    return result;
}

chart_parse_result song_loader::load_chart(const std::string& path) {
    const managed_content_storage::managed_file_read_result managed =
        managed_content_storage::read_managed_file(path_utils::from_utf8(path));
    if (managed.managed) {
        if (!managed.success) {
            chart_parse_result result;
            result.success = false;
            result.errors.push_back(managed.error_message.empty()
                ? "Failed to decrypt managed chart file: " + path
                : managed.error_message);
            return result;
        }
        return chart_parser::parse_text(std::string(managed.bytes.begin(), managed.bytes.end()), path);
    }
    return chart_parser::parse(path);
}
