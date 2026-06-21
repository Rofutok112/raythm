#include "mv/mv_managed_storage.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>

#include "content_cache_paths.h"
#include "mv/composition/mv_composition_serializer.h"
#include "network/json_helpers.h"
#include "path_utils.h"

namespace mv::managed_storage {
namespace {
namespace fs = std::filesystem;
namespace json = network::json;

constexpr const char* kManifestName = "managed-package.json";
constexpr const char* kCompositionLogicalPath = "composition.rmvcomp";
constexpr const char* kMvJsonLogicalPath = "mv.json";

content_cache_paths::mv_cache_key_parts key_parts(const mv_identity& identity) {
    return {
        .server_url = identity.server_url,
        .remote_song_id = identity.remote_song_id,
        .remote_mv_id = identity.remote_mv_id,
        .song_version = identity.song_version,
        .mv_version = identity.mv_version,
        .revision_id = identity.revision_id,
    };
}

const char* source_string(online_content::source source) {
    return source == online_content::source::official ? "official" : "community";
}

online_content::source parse_source(const std::string& value) {
    return value == "official" ? online_content::source::official : online_content::source::community;
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool is_package_relative_path(std::string_view path) {
    if (path.empty()) {
        return false;
    }
    if (path.size() >= 2 && path[1] == ':') {
        return false;
    }
    const fs::path parsed = path_utils::from_utf8(std::string(path));
    if (parsed.is_absolute()) {
        return false;
    }
    for (const auto& part : parsed) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

std::string utc_timestamp_now() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&utc, &now);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::optional<std::chrono::system_clock::time_point> parse_iso_utc_time(const std::string& iso_utc) {
    if (iso_utc.empty()) {
        return std::nullopt;
    }
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(iso_utc.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return std::nullopt;
    }
    std::tm utc{};
    utc.tm_year = year - 1900;
    utc.tm_mon = month - 1;
    utc.tm_mday = day;
    utc.tm_hour = hour;
    utc.tm_min = minute;
    utc.tm_sec = second;
#ifdef _WIN32
    const std::time_t raw = _mkgmtime(&utc);
#else
    const std::time_t raw = timegm(&utc);
#endif
    if (raw == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }
    return std::chrono::system_clock::from_time_t(raw);
}

void write_unlock(std::ostream& output, const content_unlock_meta& unlock, int indent) {
    const std::string pad(static_cast<size_t>(std::max(0, indent)), ' ');
    output << pad << "{"
           << "\"unlockState\": \"" << json::escape_string(unlock.unlock_state) << "\", "
           << "\"locked\": " << (unlock.locked ? "true" : "false") << ", "
           << "\"canDownload\": " << (unlock.can_download ? "true" : "false") << ", "
           << "\"canPlay\": " << (unlock.can_play ? "true" : "false") << ", "
           << "\"lockReason\": \"" << json::escape_string(unlock.lock_reason) << "\", "
           << "\"unlockRuleCount\": " << std::max(0, unlock.unlock_rule_count)
           << "}";
}

content_unlock_meta parse_unlock(const std::string& object) {
    content_unlock_meta unlock;
    unlock.unlock_state = json::extract_string(object, "unlockState").value_or(unlock.unlock_state);
    unlock.locked = json::extract_bool(object, "locked").value_or(unlock.locked);
    unlock.can_download = json::extract_bool(object, "canDownload").value_or(unlock.can_download);
    unlock.can_play = json::extract_bool(object, "canPlay").value_or(unlock.can_play);
    unlock.lock_reason = json::extract_string(object, "lockReason").value_or("");
    unlock.unlock_rule_count = std::max(0, json::extract_int(object, "unlockRuleCount").value_or(0));
    return unlock;
}

managed_content_storage::encrypted_asset_metadata parse_asset(const std::string& object) {
    managed_content_storage::encrypted_asset_metadata asset;
    asset.logical_path = json::extract_string(object, "logicalPath").value_or("");
    asset.encrypted_path = json::extract_string(object, "encryptedPath").value_or("");
    asset.nonce = json::extract_string(object, "nonce").value_or("");
    asset.ciphertext_hash = json::extract_string(object, "ciphertextHash").value_or("");
    asset.content_hash = json::extract_string(object, "contentHash").value_or("");
    asset.size_bytes = static_cast<std::uintmax_t>(std::max(0, json::extract_int(object, "sizeBytes").value_or(0)));
    return asset;
}

void write_asset(std::ostream& output,
                 const managed_content_storage::encrypted_asset_metadata& asset,
                 int indent) {
    const std::string pad(static_cast<size_t>(std::max(0, indent)), ' ');
    output << "{\n";
    output << pad << "  \"logicalPath\": \"" << json::escape_string(asset.logical_path) << "\",\n";
    output << pad << "  \"encryptedPath\": \"" << json::escape_string(asset.encrypted_path) << "\",\n";
    output << pad << "  \"nonce\": \"" << json::escape_string(asset.nonce) << "\",\n";
    output << pad << "  \"ciphertextHash\": \"" << json::escape_string(asset.ciphertext_hash) << "\",\n";
    output << pad << "  \"contentHash\": \"" << json::escape_string(asset.content_hash) << "\",\n";
    output << pad << "  \"sizeBytes\": " << asset.size_bytes << "\n";
    output << pad << "}";
}

managed_content_storage::package_manifest key_manifest_for(const package_manifest& manifest) {
    managed_content_storage::package_manifest key_manifest;
    key_manifest.song.source = manifest.mv.source;
    key_manifest.song.server_url = manifest.mv.server_url;
    key_manifest.song.remote_song_id = manifest.mv.remote_song_id;
    key_manifest.song.song_version = manifest.mv.song_version;
    key_manifest.song.revision_id = manifest.mv.revision_id;
    key_manifest.song.package_id = manifest.mv.package_id.empty()
        ? manifest.local_mv_id
        : manifest.mv.package_id;
    key_manifest.local_song_id = manifest.local_mv_id;
    key_manifest.key_id = manifest.key_id;
    key_manifest.content_key_version = manifest.content_key_version;
    key_manifest.encryption_scheme = manifest.encryption_scheme;
    key_manifest.license_expires_at = manifest.license_expires_at;
    key_manifest.offline_license_expires_at = manifest.offline_license_expires_at;
    key_manifest.license_revoked = manifest.license_revoked;
    key_manifest.unlock = manifest.unlock;
    return key_manifest;
}

void ensure_manifest_defaults(package_manifest& manifest) {
    if (manifest.local_mv_id.empty()) {
        manifest.local_mv_id = local_mv_id(manifest.mv);
    }
    if (manifest.mv.package_id.empty()) {
        manifest.mv.package_id = manifest.local_mv_id;
    }
    if (manifest.local_song_id.empty()) {
        manifest.local_song_id = manifest.mv.remote_song_id;
    }
    if (manifest.encryption_scheme.empty()) {
        manifest.encryption_scheme = managed_content_storage::default_encryption_scheme();
    }
    if (manifest.content_key_version <= 0) {
        manifest.content_key_version = 1;
    }
}

void copy_key_metadata(const managed_content_storage::package_manifest& key_manifest,
                       package_manifest& manifest) {
    manifest.key_id = key_manifest.key_id;
    manifest.content_key_version = key_manifest.content_key_version;
    manifest.encryption_scheme = key_manifest.encryption_scheme;
}

bool write_logical_asset(package_manifest& manifest,
                         const fs::path& directory,
                         const std::string& logical_path,
                         std::string_view plaintext,
                         managed_content_storage::encrypted_asset_metadata& asset,
                         std::string& error_message) {
    ensure_manifest_defaults(manifest);
    managed_content_storage::package_manifest key_manifest = key_manifest_for(manifest);
    if (!managed_content_storage::write_encrypted_asset(
            key_manifest, directory, logical_path, plaintext, asset, error_message)) {
        return false;
    }
    copy_key_metadata(key_manifest, manifest);
    manifest.updated_at = utc_timestamp_now();
    if (manifest.created_at.empty()) {
        manifest.created_at = manifest.updated_at;
    }
    return true;
}

bool write_manifest_to_path(package_manifest manifest,
                            const fs::path& path,
                            std::string& error_message) {
    ensure_manifest_defaults(manifest);
    if (manifest.created_at.empty()) {
        manifest.created_at = utc_timestamp_now();
    }
    manifest.updated_at = utc_timestamp_now();

    fs::create_directories(path.parent_path());
    const fs::path temp_path = path.string() + ".tmp";
    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error_message = "Failed to open MV managed package manifest for writing.";
        return false;
    }

    output << "{\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"contentType\": \"mv\",\n";
    output << "  \"contentSource\": \"" << source_string(manifest.mv.source) << "\",\n";
    output << "  \"serverUrl\": \"" << json::escape_string(manifest.mv.server_url) << "\",\n";
    output << "  \"remoteSongId\": \"" << json::escape_string(manifest.mv.remote_song_id) << "\",\n";
    output << "  \"remoteMvId\": \"" << json::escape_string(manifest.mv.remote_mv_id) << "\",\n";
    output << "  \"localMvId\": \"" << json::escape_string(manifest.local_mv_id) << "\",\n";
    output << "  \"localSongId\": \"" << json::escape_string(manifest.local_song_id) << "\",\n";
    output << "  \"songVersion\": " << std::max(0, manifest.mv.song_version) << ",\n";
    output << "  \"mvVersion\": " << std::max(0, manifest.mv.mv_version) << ",\n";
    output << "  \"revisionId\": \"" << json::escape_string(manifest.mv.revision_id) << "\",\n";
    output << "  \"packageId\": \"" << json::escape_string(manifest.mv.package_id) << "\",\n";
    output << "  \"keyId\": \"" << json::escape_string(manifest.key_id) << "\",\n";
    output << "  \"contentKeyVersion\": " << std::max(1, manifest.content_key_version) << ",\n";
    output << "  \"encryptionScheme\": \"" << json::escape_string(manifest.encryption_scheme) << "\",\n";
    output << "  \"licenseExpiresAt\": \"" << json::escape_string(manifest.license_expires_at) << "\",\n";
    output << "  \"offlineLicenseExpiresAt\": \"" << json::escape_string(manifest.offline_license_expires_at) << "\",\n";
    output << "  \"licenseRevoked\": " << (manifest.license_revoked ? "true" : "false") << ",\n";
    output << "  \"mvHash\": \"" << json::escape_string(manifest.mv.mv_hash) << "\",\n";
    output << "  \"mvFingerprint\": \"" << json::escape_string(manifest.mv.mv_fingerprint) << "\",\n";
    output << "  \"remoteMvHash\": \"" << json::escape_string(manifest.mv.remote_mv_hash) << "\",\n";
    output << "  \"remoteMvFingerprint\": \"" << json::escape_string(manifest.mv.remote_mv_fingerprint) << "\",\n";
    output << "  \"unlock\": ";
    write_unlock(output, manifest.unlock, 0);
    output << ",\n";
    output << "  \"createdAt\": \"" << json::escape_string(manifest.created_at) << "\",\n";
    output << "  \"updatedAt\": \"" << json::escape_string(manifest.updated_at) << "\",\n";
    output << "  \"assets\": {\n";
    output << "    \"mvJson\": ";
    write_asset(output, manifest.mv_json_asset, 4);
    output << ",\n";
    output << "    \"composition\": ";
    write_asset(output, manifest.composition_asset, 4);
    output << ",\n";
    output << "    \"files\": [\n";
    for (size_t index = 0; index < manifest.asset_files.size(); ++index) {
        const asset_file_manifest& file = manifest.asset_files[index];
        output << "      {\n";
        output << "        \"id\": \"" << json::escape_string(file.id) << "\",\n";
        output << "        \"asset\": ";
        write_asset(output, file.asset, 8);
        output << "\n";
        output << "      }";
        if (index + 1 < manifest.asset_files.size()) {
            output << ",";
        }
        output << "\n";
    }
    output << "    ]\n";
    output << "  }\n";
    output << "}\n";

    if (!output.good()) {
        error_message = "Failed to write MV managed package manifest.";
        return false;
    }
    output.close();

    std::error_code ec;
    fs::rename(temp_path, path, ec);
    if (ec) {
        ec.clear();
        fs::remove(path, ec);
        ec.clear();
        fs::rename(temp_path, path, ec);
        if (ec) {
            error_message = "Failed to replace MV managed package manifest.";
            return false;
        }
    }
    return true;
}

}  // namespace

std::string local_mv_id(const mv_identity& identity) {
    return content_cache_paths::mv_cache_key(key_parts(identity));
}

std::filesystem::path package_directory(const mv_identity& identity) {
    return content_cache_paths::mv_dir(identity.source, key_parts(identity));
}

std::filesystem::path manifest_path(const mv_identity& identity) {
    return content_cache_paths::mv_managed_package_manifest_path(identity.source, key_parts(identity));
}

std::filesystem::path manifest_path(const std::filesystem::path& package_directory) {
    return package_directory / kManifestName;
}

std::optional<package_manifest> read_manifest(const std::filesystem::path& package_directory) {
    const std::string content = read_file(manifest_path(package_directory));
    if (content.empty()) {
        return std::nullopt;
    }
    if (json::extract_string(content, "contentType").value_or("mv") != "mv") {
        return std::nullopt;
    }

    package_manifest manifest;
    manifest.mv.source = parse_source(json::extract_string(content, "contentSource").value_or("community"));
    manifest.mv.server_url = json::extract_string(content, "serverUrl").value_or("");
    manifest.mv.remote_song_id = json::extract_string(content, "remoteSongId").value_or("");
    manifest.mv.remote_mv_id = json::extract_string(content, "remoteMvId").value_or("");
    manifest.local_mv_id = json::extract_string(content, "localMvId").value_or("");
    manifest.local_song_id = json::extract_string(content, "localSongId").value_or("");
    manifest.mv.song_version = json::extract_int(content, "songVersion").value_or(0);
    manifest.mv.mv_version = json::extract_int(content, "mvVersion").value_or(0);
    manifest.mv.revision_id = json::extract_string(content, "revisionId").value_or("");
    manifest.mv.package_id = json::extract_string(content, "packageId").value_or("");
    manifest.key_id = json::extract_string(content, "keyId").value_or("");
    manifest.content_key_version = json::extract_int(content, "contentKeyVersion").value_or(0);
    manifest.encryption_scheme = json::extract_string(content, "encryptionScheme").value_or("");
    manifest.license_expires_at = json::extract_string(content, "licenseExpiresAt").value_or("");
    manifest.offline_license_expires_at = json::extract_string(content, "offlineLicenseExpiresAt").value_or("");
    manifest.license_revoked = json::extract_bool(content, "licenseRevoked").value_or(false);
    manifest.mv.mv_hash = json::extract_string(content, "mvHash").value_or("");
    manifest.mv.mv_fingerprint = json::extract_string(content, "mvFingerprint").value_or("");
    manifest.mv.remote_mv_hash = json::extract_string(content, "remoteMvHash").value_or("");
    manifest.mv.remote_mv_fingerprint = json::extract_string(content, "remoteMvFingerprint").value_or("");
    manifest.created_at = json::extract_string(content, "createdAt").value_or("");
    manifest.updated_at = json::extract_string(content, "updatedAt").value_or("");
    if (const std::optional<std::string> unlock = json::extract_object(content, "unlock")) {
        manifest.unlock = parse_unlock(*unlock);
    }
    manifest.mv.unlock = manifest.unlock;

    if (manifest.local_mv_id.empty() &&
        !manifest.mv.server_url.empty() &&
        !manifest.mv.remote_song_id.empty() &&
        !manifest.mv.remote_mv_id.empty()) {
        manifest.local_mv_id = local_mv_id(manifest.mv);
    }
    if (manifest.mv.package_id.empty()) {
        manifest.mv.package_id = manifest.local_mv_id;
    }

    if (const std::optional<std::string> assets = json::extract_object(content, "assets")) {
        if (const std::optional<std::string> mv_json = json::extract_object(*assets, "mvJson")) {
            manifest.mv_json_asset = parse_asset(*mv_json);
        }
        if (const std::optional<std::string> composition = json::extract_object(*assets, "composition")) {
            manifest.composition_asset = parse_asset(*composition);
        }
        if (const std::optional<std::string> files = json::extract_array(*assets, "files")) {
            for (const std::string& object : json::extract_objects_from_array(*files)) {
                asset_file_manifest file;
                file.id = json::extract_string(object, "id").value_or("");
                if (const std::optional<std::string> asset = json::extract_object(object, "asset")) {
                    file.asset = parse_asset(*asset);
                }
                if (!file.id.empty()) {
                    manifest.asset_files.push_back(file);
                }
            }
        }
    }
    return manifest;
}

bool write_manifest(package_manifest manifest, std::string& error_message) {
    return write_manifest_to_path(std::move(manifest), manifest_path(manifest.mv), error_message);
}

bool write_manifest_at(package_manifest manifest,
                       const std::filesystem::path& package_directory,
                       std::string& error_message) {
    return write_manifest_to_path(std::move(manifest), manifest_path(package_directory), error_message);
}

bool write_composition(package_manifest& manifest,
                       const composition::mv_composition& composition,
                       std::string& error_message) {
    ensure_manifest_defaults(manifest);
    const edit_access_result edit_access = can_edit(manifest);
    if (!edit_access.editable) {
        error_message = edit_access.reason.empty()
            ? "Managed MV package is not editable."
            : edit_access.reason;
        return false;
    }
    composition::mv_composition normalized = composition;
    normalized.composition_id = manifest.local_mv_id;
    const std::string serialized = composition::serialize(normalized);
    if (!write_logical_asset(manifest,
                             package_directory(manifest.mv),
                             kCompositionLogicalPath,
                             serialized,
                             manifest.composition_asset,
                             error_message)) {
        return false;
    }
    manifest.mv.mv_hash = manifest.composition_asset.content_hash;
    manifest.mv.mv_fingerprint = composition::fingerprint(normalized);
    return write_manifest_at(manifest, package_directory(manifest.mv), error_message);
}

composition_read_result read_composition(const package_manifest& manifest,
                                         const std::filesystem::path& package_directory) {
    composition_read_result result;
    const managed_content_storage::package_manifest key_manifest = key_manifest_for(manifest);
    const managed_content_storage::managed_file_read_result read =
        managed_content_storage::read_encrypted_asset(key_manifest, package_directory, manifest.composition_asset);
    if (!read.success) {
        result.errors.push_back(read.error_message.empty()
            ? "Failed to decrypt managed MV composition."
            : read.error_message);
        return result;
    }
    const std::string text(read.bytes.begin(), read.bytes.end());
    composition::parse_result parsed = composition::parse(text);
    result.success = parsed.success;
    result.composition = std::move(parsed.composition);
    result.errors = std::move(parsed.errors);
    return result;
}

bool write_mv_json_asset(package_manifest& manifest,
                         std::string_view mv_json,
                         std::string& error_message) {
    if (!write_logical_asset(manifest,
                             package_directory(manifest.mv),
                             kMvJsonLogicalPath,
                             mv_json,
                             manifest.mv_json_asset,
                             error_message)) {
        return false;
    }
    return write_manifest_at(manifest, package_directory(manifest.mv), error_message);
}

managed_content_storage::managed_file_read_result read_mv_json_asset(
    const package_manifest& manifest,
    const std::filesystem::path& package_directory) {
    const managed_content_storage::package_manifest key_manifest = key_manifest_for(manifest);
    return managed_content_storage::read_encrypted_asset(key_manifest, package_directory, manifest.mv_json_asset);
}

bool write_asset_file(package_manifest& manifest,
                      const std::string& asset_id,
                      const std::string& logical_path,
                      std::string_view plaintext,
                      std::string& error_message) {
    const edit_access_result edit_access = can_edit(manifest);
    if (!edit_access.editable) {
        error_message = edit_access.reason.empty()
            ? "Managed MV package is not editable."
            : edit_access.reason;
        return false;
    }
    if (asset_id.empty()) {
        error_message = "Managed MV asset id is empty.";
        return false;
    }
    if (!is_package_relative_path(logical_path)) {
        error_message = "Managed MV asset logical path must be package-relative.";
        return false;
    }
    asset_file_manifest* target = nullptr;
    for (asset_file_manifest& file : manifest.asset_files) {
        if (file.id == asset_id) {
            target = &file;
            break;
        }
    }
    if (target == nullptr) {
        manifest.asset_files.push_back({.id = asset_id});
        target = &manifest.asset_files.back();
    }
    if (!write_logical_asset(manifest,
                             package_directory(manifest.mv),
                             logical_path,
                             plaintext,
                             target->asset,
                             error_message)) {
        return false;
    }
    return write_manifest_at(manifest, package_directory(manifest.mv), error_message);
}

managed_content_storage::managed_file_read_result read_asset_file(
    const package_manifest& manifest,
    const std::filesystem::path& package_directory,
    const std::string& asset_id) {
    for (const asset_file_manifest& file : manifest.asset_files) {
        if (file.id == asset_id) {
            const managed_content_storage::package_manifest key_manifest = key_manifest_for(manifest);
            return managed_content_storage::read_encrypted_asset(key_manifest, package_directory, file.asset);
        }
    }
    managed_content_storage::managed_file_read_result result;
    result.managed = true;
    result.error_message = "Managed MV asset was not found in the package manifest.";
    return result;
}

edit_access_result can_edit(const package_manifest& manifest) {
    if (manifest.license_revoked) {
        return {.editable = false, .reason = "Managed MV license has been revoked."};
    }
    const auto now = std::chrono::system_clock::now();
    const std::optional<std::chrono::system_clock::time_point> online_expiry =
        parse_iso_utc_time(manifest.license_expires_at);
    const std::optional<std::chrono::system_clock::time_point> offline_expiry =
        parse_iso_utc_time(manifest.offline_license_expires_at);
    if (online_expiry.has_value() || offline_expiry.has_value()) {
        const bool online_valid = online_expiry.has_value() && *online_expiry > now;
        const bool offline_valid = offline_expiry.has_value() && *offline_expiry > now;
        if (!online_valid && !offline_valid) {
            return {.editable = false, .reason = "Managed MV license has expired."};
        }
    }
    if (content_unlock_is_locked(manifest.unlock)) {
        std::string reason = manifest.unlock.lock_reason.empty()
            ? "Managed MV is locked."
            : manifest.unlock.lock_reason;
        return {.editable = false, .reason = std::move(reason)};
    }
    return {};
}

std::vector<std::filesystem::path> list_package_directories(online_content::source source) {
    std::vector<fs::path> result;
    const fs::path root = content_cache_paths::source_root(source) / "mvs";
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return result;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_directory(ec)) {
            result.push_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

}  // namespace mv::managed_storage
