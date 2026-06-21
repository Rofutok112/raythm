#pragma once

#include <filesystem>
#include <string>

#include "online_content_identity.h"

namespace content_cache_paths {

struct song_cache_key_parts {
    std::string server_url;
    std::string remote_song_id;
    int song_version = 0;
    std::string revision_id;
};

struct chart_cache_key_parts {
    std::string server_url;
    std::string remote_song_id;
    std::string remote_chart_id;
    int song_version = 0;
    int chart_version = 0;
    std::string revision_id;
};

struct mv_cache_key_parts {
    std::string server_url;
    std::string remote_song_id;
    std::string remote_mv_id;
    int song_version = 0;
    int mv_version = 0;
    std::string revision_id;
};

std::string song_cache_key(const song_cache_key_parts& parts);
std::string chart_cache_key(const chart_cache_key_parts& parts);
std::string mv_cache_key(const mv_cache_key_parts& parts);

std::filesystem::path source_root(online_content::source source);
std::filesystem::path song_dir(online_content::source source, const song_cache_key_parts& parts);
std::filesystem::path charts_dir(online_content::source source, const song_cache_key_parts& parts);
std::filesystem::path chart_path(online_content::source source, const chart_cache_key_parts& parts);
std::filesystem::path managed_package_manifest_path(online_content::source source,
                                                    const song_cache_key_parts& parts);
std::filesystem::path mv_dir(online_content::source source, const mv_cache_key_parts& parts);
std::filesystem::path mv_managed_package_manifest_path(online_content::source source,
                                                       const mv_cache_key_parts& parts);

}  // namespace content_cache_paths
