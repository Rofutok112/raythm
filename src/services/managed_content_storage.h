#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "online_content_identity.h"

namespace managed_content_storage {

struct encrypted_asset_metadata {
    std::string logical_path;
    std::string encrypted_path;
    std::string nonce;
    std::string ciphertext_hash;
    std::string content_hash;
    std::uintmax_t size_bytes = 0;
};

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
    encrypted_asset_metadata encrypted_chart;
};

struct package_manifest {
    song_identity song;
    std::string local_song_id;
    std::string key_id;
    int content_key_version = 0;
    std::string encryption_scheme;
    std::string license_expires_at;
    std::string offline_license_expires_at;
    bool license_revoked = false;
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
    encrypted_asset_metadata song_json_asset;
    encrypted_asset_metadata audio_asset;
    encrypted_asset_metadata jacket_asset;
    std::vector<chart_manifest_entry> charts;
};

struct managed_file_read_result {
    bool managed = false;
    bool success = false;
    std::vector<unsigned char> bytes;
    std::string error_message;
};

struct managed_file_write_result {
    bool managed = false;
    bool success = false;
    std::string error_message;
};

struct managed_chart_file_info {
    bool managed = false;
    std::string local_chart_id;
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

const char* default_encryption_scheme();
bool has_encrypted_assets(const package_manifest& manifest);
std::filesystem::path encrypted_asset_path(const std::filesystem::path& song_directory,
                                           const std::string& logical_relative_path);
std::filesystem::path encrypted_asset_path(const std::filesystem::path& song_directory,
                                           const encrypted_asset_metadata& asset);
bool write_encrypted_asset(package_manifest& manifest,
                           const std::filesystem::path& song_directory,
                           const std::string& logical_relative_path,
                           std::string_view plaintext,
                           encrypted_asset_metadata& asset,
                           std::string& error_message);
managed_file_read_result read_encrypted_asset(const package_manifest& manifest,
                                              const std::filesystem::path& song_directory,
                                              const encrypted_asset_metadata& asset);
managed_file_read_result read_managed_file(const std::filesystem::path& file_path);
managed_chart_file_info describe_managed_chart_file(const std::filesystem::path& file_path);
managed_file_write_result write_managed_chart_file(const std::filesystem::path& file_path,
                                                   std::string_view plaintext,
                                                   std::string_view chart_fingerprint_hash = {});

std::vector<std::filesystem::path> list_package_directories(online_content::source source);
bool is_within_content_cache(const std::filesystem::path& path);

}  // namespace managed_content_storage
