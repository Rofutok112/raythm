#include "mv_storage.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

#include "core/app_paths.h"
#include "core/uuid_util.h"
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

std::string escape_json_string(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += ch; break;
        }
    }
    return result;
}

std::optional<mv::mv_package> load_package_directory(const fs::path& directory) {
    const fs::path json_path = directory / "mv.json";
    const std::string content = read_file(json_path);
    if (content.empty()) {
        return std::nullopt;
    }

    mv::mv_metadata meta;
    const auto mv_id = extract_json_string(content, "mvId");
    const auto song_id = extract_json_string(content, "songId");
    const auto name = extract_json_string(content, "name");
    const auto author = extract_json_string(content, "author");
    const auto script_file = extract_json_string(content, "scriptFile");

    if (!mv_id.has_value() || !song_id.has_value() || !name.has_value()) {
        return std::nullopt;
    }

    meta.mv_id = trim(*mv_id);
    meta.song_id = trim(*song_id);
    meta.name = *name;
    meta.author = author.value_or("");
    if (script_file.has_value() && !trim(*script_file).empty()) {
        meta.script_file = trim(*script_file);
    }

    if (meta.mv_id.empty() || meta.song_id.empty()) {
        return std::nullopt;
    }

    return mv::mv_package{meta, path_utils::to_utf8(directory)};
}

}  // namespace

namespace mv {

std::vector<mv_package> load_all_packages() {
    std::vector<mv_package> packages;
    const fs::path root = app_paths::mvs_root();
    if (!fs::exists(root) || !fs::is_directory(root)) {
        return packages;
    }

    for (const auto& entry : fs::directory_iterator(root)) {
        if (!entry.is_directory()) {
            continue;
        }

        const auto package = load_package_directory(entry.path());
        if (package.has_value()) {
            packages.push_back(*package);
        }
    }

    std::sort(packages.begin(), packages.end(), [](const mv_package& left, const mv_package& right) {
        if (left.meta.song_id != right.meta.song_id) {
            return left.meta.song_id < right.meta.song_id;
        }
        if (left.meta.name != right.meta.name) {
            return left.meta.name < right.meta.name;
        }
        return left.meta.mv_id < right.meta.mv_id;
    });
    return packages;
}

std::optional<mv_package> find_first_package_for_song(const std::string& song_id) {
    const std::vector<mv_package> packages = load_all_packages();
    for (const mv_package& package : packages) {
        if (package.meta.song_id == song_id) {
            return package;
        }
    }
    return std::nullopt;
}

mv_package make_default_package_for_song(const song_meta& song) {
    mv_metadata meta;
    meta.mv_id = generate_uuid();
    meta.song_id = song.song_id;
    meta.name = song.title.empty() ? "New MV" : song.title + " MV";
    meta.author.clear();
    meta.script_file = "script.rmv";
    return {meta, path_utils::to_utf8(app_paths::mv_dir(meta.mv_id))};
}

std::filesystem::path script_path(const mv_package& package) {
    const fs::path dir = path_utils::from_utf8(package.directory);
    const std::string filename = package.meta.script_file.empty() ? "script.rmv" : package.meta.script_file;
    return dir / path_utils::from_utf8(filename);
}

bool write_mv_json(const mv_metadata& meta, const std::string& directory) {
    std::filesystem::create_directories(path_utils::from_utf8(directory));

    const std::filesystem::path json_path = path_utils::from_utf8(directory) / "mv.json";
    std::ofstream out(json_path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "{\n";
    out << "  \"mvId\": \"" << escape_json_string(meta.mv_id) << "\",\n";
    out << "  \"songId\": \"" << escape_json_string(meta.song_id) << "\",\n";
    out << "  \"name\": \"" << escape_json_string(meta.name) << "\",\n";
    out << "  \"author\": \"" << escape_json_string(meta.author) << "\",\n";
    out << "  \"scriptFile\": \"" << escape_json_string(meta.script_file.empty() ? "script.rmv" : meta.script_file) << "\"\n";
    out << "}\n";
    return out.good();
}

std::string load_script(const mv_package& package) {
    return read_file(script_path(package));
}

bool save_script(const mv_package& package, const std::string& script) {
    const fs::path path = script_path(package);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << script;
    return out.good();
}

}  // namespace mv
