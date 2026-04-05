#pragma once

#include <compare>
#include <optional>
#include <string>

namespace updater {

struct semantic_version {
    int major = 0;
    int minor = 0;
    int patch = 0;

    auto operator<=>(const semantic_version&) const = default;
};

struct installed_version_info {
    semantic_version version;
};

std::optional<semantic_version> parse_semantic_version(const std::string& value);
std::string to_string(const semantic_version& version);
bool is_newer_version(const semantic_version& candidate, const semantic_version& current);

std::optional<installed_version_info> load_installed_version();
bool save_installed_version(const installed_version_info& info);
void ensure_installed_version_file(const semantic_version& fallback_version);

}  // namespace updater
