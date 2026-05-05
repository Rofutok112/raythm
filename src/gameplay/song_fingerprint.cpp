#include "song_fingerprint.h"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>

#include "updater/update_verify.h"

namespace {

std::string trim(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return std::string(value.substr(start, end - start));
}

std::string unescape_json_string(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index) {
        const char ch = value[index];
        if (ch != '\\' || index + 1 >= value.size()) {
            result.push_back(ch);
            continue;
        }

        const char escaped = value[++index];
        switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            default:
                result.push_back('\\');
                result.push_back(escaped);
                break;
        }
    }
    return result;
}

std::optional<std::string> extract_json_string(std::string_view content, std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string_view::npos) {
        return std::nullopt;
    }

    const size_t colon_pos = content.find(':', key_pos + token.size());
    if (colon_pos == std::string_view::npos) {
        return std::nullopt;
    }

    size_t value_start = colon_pos + 1;
    while (value_start < content.size() && std::isspace(static_cast<unsigned char>(content[value_start])) != 0) {
        ++value_start;
    }

    if (value_start >= content.size() || content[value_start] != '"') {
        return std::nullopt;
    }

    ++value_start;
    size_t value_end = value_start;
    while (value_end < content.size()) {
        if (content[value_end] == '"' && content[value_end - 1] != '\\') {
            return unescape_json_string(content.substr(value_start, value_end - value_start));
        }
        ++value_end;
    }

    return std::nullopt;
}

std::optional<std::string> extract_json_number_token(std::string_view content, std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string_view::npos) {
        return std::nullopt;
    }

    const size_t colon_pos = content.find(':', key_pos + token.size());
    if (colon_pos == std::string_view::npos) {
        return std::nullopt;
    }

    size_t value_start = colon_pos + 1;
    while (value_start < content.size() && std::isspace(static_cast<unsigned char>(content[value_start])) != 0) {
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

std::optional<double> parse_number(const std::string& value) {
    try {
        size_t parsed = 0;
        const double result = std::stod(value, &parsed);
        if (parsed != value.size()) {
            return std::nullopt;
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
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

std::string format_number(double value) {
    std::ostringstream out;
    out << std::setprecision(15) << value;
    return out.str();
}

std::string canonical_string(std::string_view content) {
    const std::optional<std::string> base_bpm = extract_json_number_token(content, "baseBpm");
    const std::optional<std::string> duration_sec = extract_json_number_token(content, "durationSec");
    const std::optional<std::string> preview_start_ms = extract_json_number_token(content, "previewStartMs");
    const std::optional<std::string> song_version = extract_json_number_token(content, "songVersion");

    std::string preview_ms = "0";
    if (preview_start_ms.has_value()) {
        if (const std::optional<int> parsed = parse_int(*preview_start_ms)) {
            preview_ms = std::to_string(*parsed);
        } else {
            preview_ms = *preview_start_ms;
        }
    }

    std::string canonical_bpm;
    if (base_bpm.has_value()) {
        if (const std::optional<double> parsed = parse_number(*base_bpm)) {
            canonical_bpm = format_number(*parsed);
        } else {
            canonical_bpm = *base_bpm;
        }
    }

    std::string canonical_duration;
    if (duration_sec.has_value()) {
        if (const std::optional<double> parsed = parse_number(*duration_sec)) {
            canonical_duration = std::to_string(static_cast<int>(*parsed));
        } else {
            canonical_duration = *duration_sec;
        }
    }

    std::string canonical_version = "1";
    if (song_version.has_value()) {
        if (const std::optional<int> parsed = parse_int(*song_version)) {
            canonical_version = std::to_string(*parsed);
        } else {
            canonical_version = *song_version;
        }
    }

    std::ostringstream out;
    out << "title=" << extract_json_string(content, "title").value_or("") << '\n'
        << "artist=" << extract_json_string(content, "artist").value_or("") << '\n'
        << "baseBpm=" << canonical_bpm << '\n'
        << "durationSec=" << canonical_duration << '\n'
        << "previewStartMs=" << preview_ms << '\n'
        << "songVersion=" << canonical_version;
    return out.str();
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

namespace song_fingerprint {

std::string build(std::string_view content) {
    return canonical_string(content);
}

std::optional<std::string> compute_sha256_hex(const std::filesystem::path& song_json_path) {
    const std::string content = read_text_file(song_json_path);
    if (content.empty()) {
        return std::nullopt;
    }
    const std::string fingerprint = build(content);
    return updater::compute_sha256_hex(std::string_view(fingerprint));
}

}  // namespace song_fingerprint
