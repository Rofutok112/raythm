#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "online_content_identity.h"

namespace managed_content_storage {

struct song_identity {
    online_content::source source = online_content::source::community;
    std::string server_url;
    std::string remote_song_id;
    int song_version = 0;
    std::string revision_id;
    std::string package_id;
};

struct chart_identity {
    online_content::source source = online_content::source::community;
    std::string server_url;
    std::string remote_song_id;
    std::string remote_chart_id;
    int song_version = 0;
    int chart_version = 0;
    std::string revision_id;
    std::string chart_hash;
    std::string chart_fingerprint;
    std::string remote_chart_hash;
    std::string remote_chart_fingerprint;
};

struct chart_manifest_entry {
    std::string local_chart_id;
    std::string remote_chart_id;
    int chart_version = 0;
    std::string revision_id;
    std::string chart_hash;
    std::string chart_fingerprint;
    std::string remote_chart_hash;
    std::string remote_chart_fingerprint;
};

struct package_manifest {
    song_identity song;
    std::string local_song_id;
    std::string song_json_hash;
    std::string song_json_fingerprint;
    std::string audio_hash;
    std::string jacket_hash;
    std::string remote_song_json_hash;
    std::string remote_song_json_fingerprint;
    std::string remote_audio_hash;
    std::string remote_jacket_hash;
    std::string created_at;
    std::string updated_at;
    std::vector<chart_manifest_entry> charts;
};

std::string local_song_id(const song_identity& identity);
std::string local_chart_id(const chart_identity& identity);

std::filesystem::path song_directory(const song_identity& identity);
std::filesystem::path chart_file_path(const chart_identity& identity);
std::filesystem::path chart_file_path(const std::filesystem::path& song_directory,
                                      const std::string& local_chart_id);
std::filesystem::path manifest_path(const song_identity& identity);
std::filesystem::path manifest_path(const std::filesystem::path& song_directory);

std::optional<package_manifest> read_manifest(const std::filesystem::path& song_directory);
bool write_manifest(package_manifest manifest, std::string& error_message);
void upsert_chart(package_manifest& manifest, const chart_identity& identity);

std::vector<std::filesystem::path> list_package_directories(online_content::source source);
bool is_within_content_cache(const std::filesystem::path& path);

}  // namespace managed_content_storage
