#pragma once

#include <optional>
#include <string>

#include "updater/update_version.h"

namespace updater {

struct release_asset_urls {
    std::string package_url;
    std::string checksum_url;
};

struct latest_release_info {
    semantic_version version;
    std::string tag_name;
    release_asset_urls assets;
};

std::optional<latest_release_info> parse_latest_release_response(const std::string& response_body);
std::optional<latest_release_info> fetch_latest_release_info();

}  // namespace updater
