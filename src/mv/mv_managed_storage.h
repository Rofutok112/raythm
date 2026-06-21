#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "managed_content_storage.h"
#include "mv/composition/mv_composition.h"
#include "online_content_identity.h"

namespace mv::managed_storage {

struct mv_identity {
    online_content::source source = online_content::source::community;
    std::string server_url;
    std::string remote_song_id;
    std::string remote_mv_id;
    int song_version = 0;
    int mv_version = 0;
    std::string revision_id;
    std::string package_id;
    std::string mv_hash;
    std::string mv_fingerprint;
    std::string remote_mv_hash;
    std::string remote_mv_fingerprint;
    content_unlock_meta unlock;
};

struct asset_file_manifest {
    std::string id;
    managed_content_storage::encrypted_asset_metadata asset;
};

struct package_manifest {
    mv_identity mv;
    std::string local_mv_id;
    std::string local_song_id;
    std::string key_id;
    int content_key_version = 0;
    std::string encryption_scheme;
    std::string license_expires_at;
    std::string offline_license_expires_at;
    bool license_revoked = false;
    content_unlock_meta unlock;
    std::string created_at;
    std::string updated_at;
    managed_content_storage::encrypted_asset_metadata mv_json_asset;
    managed_content_storage::encrypted_asset_metadata composition_asset;
    std::vector<asset_file_manifest> asset_files;
};

struct composition_read_result {
    bool success = false;
    composition::mv_composition composition;
    std::vector<std::string> errors;
};

struct edit_access_result {
    bool editable = true;
    std::string reason;
};

std::string local_mv_id(const mv_identity& identity);
std::filesystem::path package_directory(const mv_identity& identity);
std::filesystem::path manifest_path(const mv_identity& identity);
std::filesystem::path manifest_path(const std::filesystem::path& package_directory);

std::optional<package_manifest> read_manifest(const std::filesystem::path& package_directory);
bool write_manifest(package_manifest manifest, std::string& error_message);
bool write_manifest_at(package_manifest manifest,
                       const std::filesystem::path& package_directory,
                       std::string& error_message);

bool write_composition(package_manifest& manifest,
                       const composition::mv_composition& composition,
                       std::string& error_message);
composition_read_result read_composition(const package_manifest& manifest,
                                         const std::filesystem::path& package_directory);

bool write_mv_json_asset(package_manifest& manifest,
                         std::string_view mv_json,
                         std::string& error_message);
managed_content_storage::managed_file_read_result read_mv_json_asset(const package_manifest& manifest,
                                                                     const std::filesystem::path& package_directory);
bool write_asset_file(package_manifest& manifest,
                      const std::string& asset_id,
                      const std::string& logical_path,
                      std::string_view plaintext,
                      std::string& error_message);
managed_content_storage::managed_file_read_result read_asset_file(const package_manifest& manifest,
                                                                  const std::filesystem::path& package_directory,
                                                                  const std::string& asset_id);
edit_access_result can_edit(const package_manifest& manifest);
std::vector<std::filesystem::path> list_package_directories(online_content::source source);

}  // namespace mv::managed_storage
