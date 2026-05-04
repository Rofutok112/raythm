#include "network/server_environment.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "app_paths.h"

namespace {

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
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

namespace server_environment {

std::string normalize_url(const std::string& server_url) {
    std::string normalized = trim(server_url);
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

std::string active_server_url_from_settings() {
    const std::string content = read_file(app_paths::settings_path());
    const environment env = parse(extract_string(content, "serverEnvironment").value_or(""));
    return normalize_url(configured_url(env));
}

}  // namespace server_environment
