#include "updater/update_version.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "updater/update_paths.h"

namespace {

std::optional<int> parse_version_component(const std::string& token) {
    if (token.empty()) {
        return std::nullopt;
    }

    for (const unsigned char ch : token) {
        if (!std::isdigit(ch)) {
            return std::nullopt;
        }
    }

    try {
        return std::stoi(token);
    } catch (...) {
        return std::nullopt;
    }
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<std::string> extract_string(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    const size_t colon_pos = content.find(':', key_pos + token.size());
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    size_t start = colon_pos + 1;
    while (start < content.size() && std::isspace(static_cast<unsigned char>(content[start]))) {
        ++start;
    }

    if (start >= content.size() || content[start] != '"') {
        return std::nullopt;
    }
    ++start;

    std::string result;
    for (size_t i = start; i < content.size(); ++i) {
        if (content[i] == '"') {
            return result;
        }
        if (content[i] == '\\' && i + 1 < content.size()) {
            result += content[++i];
        } else {
            result += content[i];
        }
    }

    return std::nullopt;
}

}  // namespace

namespace updater {

std::optional<semantic_version> parse_semantic_version(const std::string& value) {
    std::string normalized = value;
    if (!normalized.empty() && (normalized.front() == 'v' || normalized.front() == 'V')) {
        normalized.erase(normalized.begin());
    }

    std::istringstream stream(normalized);
    std::string token;
    semantic_version parsed;

    if (!std::getline(stream, token, '.')) {
        return std::nullopt;
    }
    const std::optional<int> major = parse_version_component(token);
    if (!major.has_value()) {
        return std::nullopt;
    }
    parsed.major = *major;

    if (!std::getline(stream, token, '.')) {
        return std::nullopt;
    }
    const std::optional<int> minor = parse_version_component(token);
    if (!minor.has_value()) {
        return std::nullopt;
    }
    parsed.minor = *minor;

    if (!std::getline(stream, token, '.')) {
        return std::nullopt;
    }
    const std::optional<int> patch = parse_version_component(token);
    if (!patch.has_value()) {
        return std::nullopt;
    }
    parsed.patch = *patch;

    if (std::getline(stream, token, '.')) {
        return std::nullopt;
    }

    return parsed;
}

std::string to_string(const semantic_version& version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch);
}

bool is_newer_version(const semantic_version& candidate, const semantic_version& current) {
    return candidate > current;
}

std::optional<installed_version_info> load_installed_version() {
    const std::string content = read_file(version_file_path());
    if (content.empty()) {
        return std::nullopt;
    }

    const std::optional<std::string> version_text = extract_string(content, "version");
    if (!version_text.has_value()) {
        return std::nullopt;
    }

    const std::optional<semantic_version> version = parse_semantic_version(*version_text);
    if (!version.has_value()) {
        return std::nullopt;
    }

    return installed_version_info{*version};
}

bool save_installed_version(const installed_version_info& info) {
    ensure_update_directories();

    std::ofstream output(version_file_path());
    if (!output.is_open()) {
        return false;
    }

    output << "{\n";
    output << "  \"version\": \"" << to_string(info.version) << "\"\n";
    output << "}\n";
    return true;
}

void ensure_installed_version_file(const semantic_version& fallback_version) {
    if (load_installed_version().has_value()) {
        return;
    }

    save_installed_version(installed_version_info{fallback_version});
}

}  // namespace updater
