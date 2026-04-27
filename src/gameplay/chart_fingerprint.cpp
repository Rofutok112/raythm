#include "chart_fingerprint.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "updater/update_verify.h"

namespace {

std::string normalize_newlines(std::string_view content) {
    std::string normalized;
    normalized.reserve(content.size());

    for (size_t index = 0; index < content.size(); ++index) {
        const char ch = content[index];
        if (ch == '\r' && index + 1 < content.size() && content[index + 1] == '\n') {
            normalized.push_back('\n');
            ++index;
            continue;
        }
        normalized.push_back(ch);
    }

    return normalized;
}

std::vector<std::string> split_preserve_trailing_empty(std::string_view content) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= content.size()) {
        const size_t end = content.find('\n', start);
        if (end == std::string_view::npos) {
            lines.emplace_back(content.substr(start));
            break;
        }
        lines.emplace_back(content.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

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

std::string trim_end(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

namespace chart_fingerprint {

std::string build(std::string_view content) {
    const std::string normalized = normalize_newlines(content);
    const std::vector<std::string> lines = split_preserve_trailing_empty(normalized);
    std::vector<std::string> kept_lines;
    kept_lines.reserve(lines.size());

    bool in_metadata = false;
    for (const std::string& line : lines) {
        const std::string trimmed = trim(line);

        if (trimmed == "[Metadata]") {
            in_metadata = true;
            kept_lines.push_back(trim_end(line));
            continue;
        }

        if (in_metadata && trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']') {
            in_metadata = false;
        }

        if (in_metadata) {
            const size_t separator_index = line.find('=');
            if (separator_index != std::string::npos) {
                const std::string key = trim(std::string_view(line).substr(0, separator_index));
                if (key == "chartId" || key == "songId") {
                    continue;
                }
            }
        }

        kept_lines.push_back(trim_end(line));
    }

    std::string result;
    for (size_t index = 0; index < kept_lines.size(); ++index) {
        if (index > 0) {
            result.push_back('\n');
        }
        result += kept_lines[index];
    }
    return result;
}

std::optional<std::string> compute_sha256_hex(const std::filesystem::path& chart_path) {
    const std::string content = read_text_file(chart_path);
    if (content.empty()) {
        return std::nullopt;
    }
    const std::string fingerprint = build(content);
    return updater::compute_sha256_hex(std::string_view(fingerprint));
}

}  // namespace chart_fingerprint
