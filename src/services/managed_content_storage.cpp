#include "services/managed_content_storage.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <random>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

#include "app_paths.h"
#include "content_cache_paths.h"
#include "network/json_helpers.h"
#include "path_utils.h"
#include "updater/update_verify.h"

namespace managed_content_storage {
namespace {
namespace fs = std::filesystem;
namespace json = network::json;

constexpr const char* kDefaultEncryptionScheme = "raythm-dev-sha256-stream-v1";
constexpr const char* kEncryptedDirectoryName = ".encrypted";

content_cache_paths::song_cache_key_parts song_key_parts(const song_identity& identity) {
    return {
        .server_url = identity.server_url,
        .remote_song_id = identity.remote_song_id,
        .song_version = identity.song_version,
        .revision_id = identity.revision_id,
    };
}

content_cache_paths::chart_cache_key_parts chart_key_parts(const chart_identity& identity) {
    return {
        .server_url = identity.server_url,
        .remote_song_id = identity.remote_song_id,
        .remote_chart_id = identity.remote_chart_id,
        .song_version = identity.song_version,
        .chart_version = identity.chart_version,
        .revision_id = identity.revision_id,
    };
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

std::vector<unsigned char> read_binary_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(input),
                                      std::istreambuf_iterator<char>());
}

bool write_binary_file(const fs::path& path,
                       const std::vector<unsigned char>& bytes,
                       std::string& error_message) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error_message = "Failed to open encrypted managed package file for writing.";
        return false;
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    if (!output.good()) {
        error_message = "Failed to write encrypted managed package file.";
        return false;
    }
    return true;
}

std::string utc_timestamp_now() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

bool is_within_root(const fs::path& path, const fs::path& root) {
    std::error_code ec;
    const fs::path normalized_path = fs::weakly_canonical(path, ec);
    if (ec) {
        return false;
    }
    const fs::path normalized_root = fs::weakly_canonical(root, ec);
    if (ec) {
        return false;
    }

    auto path_it = normalized_path.begin();
    auto root_it = normalized_root.begin();
    for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *root_it) {
            return false;
        }
    }
    return true;
}

bool is_within_root_lexical(const fs::path& path, const fs::path& root) {
    std::error_code ec;
    const fs::path normalized_path = fs::absolute(path, ec).lexically_normal();
    if (ec) {
        return false;
    }

    ec.clear();
    const fs::path normalized_root = fs::absolute(root, ec).lexically_normal();
    if (ec) {
        return false;
    }

    auto path_it = normalized_path.begin();
    auto root_it = normalized_root.begin();
    for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *root_it) {
            return false;
        }
    }
    return true;
}

std::string normalize_relative_path(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return fs::path(value).lexically_normal().generic_string();
}

std::optional<std::string> safe_relative_path(std::string value) {
    value = normalize_relative_path(std::move(value));
    if (value.empty() || value == ".") {
        return std::nullopt;
    }

    const fs::path path(value);
    if (path.has_root_path() || path.has_root_name()) {
        return std::nullopt;
    }
    for (const fs::path& part : path) {
        if (part.generic_string() == "..") {
            return std::nullopt;
        }
    }
    return value;
}

std::string encrypted_relative_path_for(const std::string& logical_relative_path) {
    const std::optional<std::string> normalized = safe_relative_path(logical_relative_path);
    if (!normalized.has_value()) {
        return {};
    }
    return (fs::path(kEncryptedDirectoryName) / fs::path(*normalized)).generic_string() + ".renc";
}

std::string bytes_sha256_hex(const std::vector<unsigned char>& bytes) {
    return updater::compute_sha256_hex(std::string_view(
        reinterpret_cast<const char*>(bytes.data()),
        bytes.size()));
}

std::string bytes_sha256_hex(std::string_view bytes) {
    return updater::compute_sha256_hex(bytes);
}

std::vector<unsigned char> hex_to_bytes(const std::string& hex) {
    auto nibble = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        }
        if (ch >= 'A' && ch <= 'F') {
            return ch - 'A' + 10;
        }
        return -1;
    };

    std::vector<unsigned char> bytes;
    if (hex.size() % 2 != 0) {
        return bytes;
    }
    bytes.reserve(hex.size() / 2);
    for (size_t index = 0; index < hex.size(); index += 2) {
        const int high = nibble(hex[index]);
        const int low = nibble(hex[index + 1]);
        if (high < 0 || low < 0) {
            bytes.clear();
            return bytes;
        }
        bytes.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return bytes;
}

std::string random_nonce_hex() {
    std::array<unsigned char, 16> nonce{};
    std::random_device random;
    for (unsigned char& byte : nonce) {
        byte = static_cast<unsigned char>(random());
    }
    return bytes_sha256_hex(std::string_view(reinterpret_cast<const char*>(nonce.data()), nonce.size())).substr(0, 32);
}

std::string fallback_key_id_for(const package_manifest& manifest) {
    const std::string material = manifest.song.server_url + "\n" +
        manifest.song.remote_song_id + "\n" +
        manifest.local_song_id + "\n" +
        manifest.song.revision_id;
    return "dev-" + updater::compute_sha256_hex(std::string_view(material)).substr(0, 16);
}

void ensure_encryption_metadata(package_manifest& manifest) {
    if (manifest.local_song_id.empty()) {
        manifest.local_song_id = local_song_id(manifest.song);
    }
    if (manifest.song.package_id.empty()) {
        manifest.song.package_id = manifest.local_song_id;
    }
    if (manifest.key_id.empty()) {
        manifest.key_id = fallback_key_id_for(manifest);
    }
    if (manifest.content_key_version <= 0) {
        manifest.content_key_version = 1;
    }
    if (manifest.encryption_scheme.empty()) {
        manifest.encryption_scheme = kDefaultEncryptionScheme;
    }
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

bool license_allows_decrypt(const package_manifest& manifest, std::string& error_message) {
    if (manifest.license_revoked) {
        error_message = "Managed content license has been revoked.";
        return false;
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
            error_message = "Managed content license has expired.";
            return false;
        }
    }
    return true;
}

bool dev_license_fallback_enabled() {
#ifdef RAYTHM_SERVER_ENV
    return std::string_view(RAYTHM_SERVER_ENV) != "production";
#else
    return true;
#endif
}

std::optional<std::string> dev_content_key_hex(const package_manifest& manifest, std::string& error_message) {
    if (manifest.encryption_scheme != kDefaultEncryptionScheme) {
        error_message = "Managed content encryption scheme is not supported by this client.";
        return std::nullopt;
    }
    if (!license_allows_decrypt(manifest, error_message)) {
        return std::nullopt;
    }
    if (!dev_license_fallback_enabled()) {
        error_message = "Managed content server license key provider is required for this build.";
        return std::nullopt;
    }

    const std::string material = manifest.song.server_url + "\n" +
        manifest.song.remote_song_id + "\n" +
        manifest.song.package_id + "\n" +
        manifest.key_id + "\n" +
        std::to_string(std::max(1, manifest.content_key_version));
    return updater::compute_sha256_hex(std::string_view(material));
}

std::vector<unsigned char> xor_crypt(std::string_view input,
                                     const std::string& key_hex,
                                     const std::string& nonce_hex) {
    std::vector<unsigned char> output(input.size());
    std::uint64_t counter = 0;
    size_t cursor = 0;
    while (cursor < input.size()) {
        const std::string block_material = key_hex + ":" + nonce_hex + ":" + std::to_string(counter++);
        const std::vector<unsigned char> block =
            hex_to_bytes(updater::compute_sha256_hex(std::string_view(block_material)));
        if (block.empty()) {
            break;
        }
        for (size_t index = 0; index < block.size() && cursor < input.size(); ++index, ++cursor) {
            output[cursor] = static_cast<unsigned char>(
                static_cast<unsigned char>(input[cursor]) ^ block[index]);
        }
    }
    return output;
}

encrypted_asset_metadata parse_asset(const std::string& object) {
    encrypted_asset_metadata asset;
    asset.logical_path = normalize_relative_path(json::extract_string(object, "logicalPath").value_or(""));
    asset.encrypted_path = normalize_relative_path(json::extract_string(object, "encryptedPath").value_or(""));
    asset.nonce = json::extract_string(object, "nonce").value_or(
        json::extract_string(object, "iv").value_or(""));
    asset.ciphertext_hash = json::extract_string(object, "ciphertextHash").value_or(
        json::extract_string(object, "encryptedHash").value_or(""));
    asset.content_hash = json::extract_string(object, "contentHash").value_or(
        json::extract_string(object, "plainContentHash").value_or(""));
    asset.size_bytes = static_cast<std::uintmax_t>(std::max(0, json::extract_int(object, "sizeBytes").value_or(0)));
    return asset;
}

void write_asset(std::ostream& output, const encrypted_asset_metadata& asset, int indent) {
    const std::string pad(static_cast<size_t>(indent), ' ');
    output << "{\n";
    output << pad << "  \"logicalPath\": \"" << json::escape_string(asset.logical_path) << "\",\n";
    output << pad << "  \"encryptedPath\": \"" << json::escape_string(asset.encrypted_path) << "\",\n";
    output << pad << "  \"nonce\": \"" << json::escape_string(asset.nonce) << "\",\n";
    output << pad << "  \"ciphertextHash\": \"" << json::escape_string(asset.ciphertext_hash) << "\",\n";
    output << pad << "  \"contentHash\": \"" << json::escape_string(asset.content_hash) << "\",\n";
    output << pad << "  \"sizeBytes\": " << asset.size_bytes << "\n";
    output << pad << "}";
}

bool asset_empty(const encrypted_asset_metadata& asset) {
    return asset.logical_path.empty() || asset.encrypted_path.empty() || asset.nonce.empty();
}

std::optional<fs::path> validated_encrypted_asset_path(const fs::path& song_directory,
                                                       const encrypted_asset_metadata& asset,
                                                       std::string& error_message) {
    const std::optional<std::string> logical_rel = safe_relative_path(asset.logical_path);
    const std::optional<std::string> encrypted_rel = safe_relative_path(asset.encrypted_path);
    if (!logical_rel.has_value() || !encrypted_rel.has_value()) {
        error_message = "Managed package asset path is invalid.";
        return std::nullopt;
    }

    const fs::path encrypted_relative(*encrypted_rel);
    auto it = encrypted_relative.begin();
    if (it == encrypted_relative.end() || it->generic_string() != kEncryptedDirectoryName) {
        error_message = "Managed package encrypted asset path is outside the encrypted store.";
        return std::nullopt;
    }

    const fs::path encrypted_path = song_directory / encrypted_relative;
    const fs::path encrypted_root = song_directory / kEncryptedDirectoryName;
    if (!is_within_root_lexical(encrypted_path, encrypted_root)) {
        error_message = "Managed package encrypted asset path escapes the package.";
        return std::nullopt;
    }
    return encrypted_path.lexically_normal();
}

bool path_matches_asset(const fs::path& package_dir,
                        const encrypted_asset_metadata& asset,
                        const fs::path& path) {
    if (asset_empty(asset)) {
        return false;
    }

    const std::optional<std::string> logical_rel = safe_relative_path(asset.logical_path);
    const std::optional<std::string> encrypted_rel = safe_relative_path(asset.encrypted_path);
    if (!logical_rel.has_value() || !encrypted_rel.has_value()) {
        return false;
    }

    std::error_code ec;
    fs::path normalized_path = fs::weakly_canonical(path, ec);
    if (ec) {
        ec.clear();
        normalized_path = fs::absolute(path, ec).lexically_normal();
        if (ec) {
            return false;
        }
    }
    const fs::path logical = fs::absolute(package_dir / fs::path(*logical_rel), ec).lexically_normal();
    if (!ec && normalized_path == logical) {
        return true;
    }
    ec.clear();
    const fs::path encrypted = fs::absolute(package_dir / fs::path(*encrypted_rel), ec).lexically_normal();
    return !ec && normalized_path == encrypted;
}

std::optional<std::pair<fs::path, package_manifest>> manifest_for_file(const fs::path& path) {
    std::error_code ec;
    fs::path cursor = fs::is_directory(path, ec) ? path : path.parent_path();
    for (int depth = 0; depth < 6 && !cursor.empty(); ++depth) {
        if (fs::exists(manifest_path(cursor), ec)) {
            if (std::optional<package_manifest> manifest = read_manifest(cursor)) {
                return std::make_pair(cursor, *manifest);
            }
        }
        if (cursor == cursor.parent_path()) {
            break;
        }
        cursor = cursor.parent_path();
    }
    return std::nullopt;
}

std::optional<std::string> replace_chart_id_line(std::string content, const std::string& chart_id) {
    size_t line_start = 0;
    while (line_start <= content.size()) {
        const size_t line_end = content.find('\n', line_start);
        const size_t raw_end = line_end == std::string::npos ? content.size() : line_end;
        size_t value_end = raw_end;
        if (value_end > line_start && content[value_end - 1] == '\r') {
            --value_end;
        }

        constexpr std::string_view kPrefix = "chartId=";
        if (value_end - line_start >= kPrefix.size() &&
            std::string_view(content.data() + line_start, kPrefix.size()) == kPrefix) {
            content.replace(line_start, value_end - line_start, std::string(kPrefix) + chart_id);
            return content;
        }

        if (line_end == std::string::npos) {
            break;
        }
        line_start = line_end + 1;
    }
    return std::nullopt;
}

}  // namespace

std::string local_song_id(const song_identity& identity) {
    return content_cache_paths::song_cache_key(song_key_parts(identity));
}

std::string local_chart_id(const chart_identity& identity) {
    return content_cache_paths::chart_cache_key(chart_key_parts(identity));
}

fs::path song_directory(const song_identity& identity) {
    return content_cache_paths::song_dir(identity.source, song_key_parts(identity));
}

fs::path chart_file_path(const chart_identity& identity) {
    return content_cache_paths::chart_path(identity.source, chart_key_parts(identity));
}

fs::path chart_file_path(const fs::path& song_directory, const std::string& local_chart_id) {
    return song_directory / "charts" / (local_chart_id + ".rchart");
}

fs::path manifest_path(const song_identity& identity) {
    return content_cache_paths::managed_package_manifest_path(identity.source, song_key_parts(identity));
}

fs::path manifest_path(const fs::path& song_directory) {
    return song_directory / "managed-package.json";
}

std::optional<package_manifest> read_manifest(const fs::path& song_directory) {
    const std::string content = read_file(manifest_path(song_directory));
    if (content.empty()) {
        return std::nullopt;
    }

    const std::optional<online_content::source> source =
        online_content::source_from_string(json::extract_string(content, "contentSource").value_or(
            json::extract_string(content, "source").value_or("")));
    if (!source.has_value()) {
        return std::nullopt;
    }

    package_manifest manifest;
    manifest.song.source = *source;
    manifest.song.server_url = json::extract_string(content, "serverUrl").value_or("");
    manifest.song.remote_song_id = json::extract_string(content, "remoteSongId").value_or("");
    manifest.song.song_version = json::extract_int(content, "songVersion").value_or(0);
    manifest.song.revision_id = json::extract_string(content, "revisionId").value_or("");
    manifest.song.package_id = json::extract_string(content, "packageId").value_or("");
    manifest.local_song_id = json::extract_string(content, "localSongId").value_or("");
    manifest.key_id = json::extract_string(content, "keyId").value_or("");
    manifest.content_key_version = json::extract_int(content, "contentKeyVersion").value_or(0);
    manifest.encryption_scheme = json::extract_string(content, "encryptionScheme").value_or("");
    if (const std::optional<std::string> license = json::extract_object(content, "license")) {
        manifest.license_expires_at = json::extract_string(*license, "expiresAt").value_or("");
        manifest.offline_license_expires_at = json::extract_string(*license, "offlineExpiresAt").value_or("");
        manifest.license_revoked = json::extract_bool(*license, "revoked").value_or(false);
    } else {
        manifest.license_expires_at = json::extract_string(content, "licenseExpiresAt").value_or("");
        manifest.offline_license_expires_at = json::extract_string(content, "offlineLicenseExpiresAt").value_or("");
        manifest.license_revoked = json::extract_bool(content, "licenseRevoked").value_or(false);
    }
    manifest.song_json_hash = json::extract_string(content, "songJsonHash").value_or("");
    manifest.song_json_fingerprint = json::extract_string(content, "songJsonFingerprint").value_or("");
    manifest.audio_hash = json::extract_string(content, "audioHash").value_or("");
    manifest.jacket_hash = json::extract_string(content, "jacketHash").value_or("");
    manifest.remote_song_json_hash = json::extract_string(content, "remoteSongJsonHash").value_or("");
    manifest.remote_song_json_fingerprint = json::extract_string(content, "remoteSongJsonFingerprint").value_or("");
    manifest.remote_audio_hash = json::extract_string(content, "remoteAudioHash").value_or("");
    manifest.remote_jacket_hash = json::extract_string(content, "remoteJacketHash").value_or("");
    manifest.created_at = json::extract_string(content, "createdAt").value_or("");
    manifest.updated_at = json::extract_string(content, "updatedAt").value_or("");

    if (const std::optional<std::string> assets = json::extract_object(content, "assets")) {
        if (const std::optional<std::string> song_json_asset = json::extract_object(*assets, "songJson")) {
            manifest.song_json_asset = parse_asset(*song_json_asset);
        }
        if (const std::optional<std::string> audio_asset = json::extract_object(*assets, "audio")) {
            manifest.audio_asset = parse_asset(*audio_asset);
        }
        if (const std::optional<std::string> jacket_asset = json::extract_object(*assets, "jacket")) {
            manifest.jacket_asset = parse_asset(*jacket_asset);
        }
    }

    if (manifest.song.server_url.empty() || manifest.song.remote_song_id.empty()) {
        return std::nullopt;
    }
    if (manifest.local_song_id.empty()) {
        manifest.local_song_id = local_song_id(manifest.song);
    }

    if (const std::optional<std::string> charts = json::extract_array(content, "charts")) {
        for (const std::string& object : json::extract_objects_from_array(*charts)) {
            chart_manifest_entry chart;
            chart.local_chart_id = json::extract_string(object, "localChartId").value_or("");
            chart.remote_chart_id = json::extract_string(object, "remoteChartId").value_or("");
            chart.chart_version = json::extract_int(object, "chartVersion").value_or(0);
            chart.revision_id = json::extract_string(object, "revisionId").value_or("");
            chart.chart_hash = json::extract_string(object, "chartHash").value_or("");
            chart.chart_fingerprint = json::extract_string(object, "chartFingerprint").value_or("");
            chart.remote_chart_hash = json::extract_string(object, "remoteChartHash").value_or("");
            chart.remote_chart_fingerprint = json::extract_string(object, "remoteChartFingerprint").value_or("");
            if (const std::optional<std::string> asset = json::extract_object(object, "asset")) {
                chart.encrypted_chart = parse_asset(*asset);
            }
            if (!chart.local_chart_id.empty() && !chart.remote_chart_id.empty()) {
                manifest.charts.push_back(std::move(chart));
            }
        }
    }

    return manifest;
}

bool write_manifest_at_path(package_manifest manifest,
                            const fs::path& path,
                            std::string& error_message) {
    if (manifest.song.server_url.empty() || manifest.song.remote_song_id.empty()) {
        error_message = "Managed content manifest is missing remote song identity.";
        return false;
    }
    if (manifest.local_song_id.empty()) {
        manifest.local_song_id = local_song_id(manifest.song);
    }
    if (has_encrypted_assets(manifest)) {
        ensure_encryption_metadata(manifest);
    }
    if (manifest.created_at.empty()) {
        manifest.created_at = utc_timestamp_now();
    }
    manifest.updated_at = utc_timestamp_now();

    fs::create_directories(path.parent_path());
    const fs::path temp_path = path.parent_path() / (path.filename().string() + ".tmp");
    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error_message = "Failed to open managed content manifest for writing.";
        return false;
    }

    output << "{\n";
    output << "  \"schemaVersion\": 3,\n";
    output << "  \"contentSource\": \"" << online_content::source_label(manifest.song.source) << "\",\n";
    output << "  \"serverUrl\": \"" << json::escape_string(manifest.song.server_url) << "\",\n";
    output << "  \"remoteSongId\": \"" << json::escape_string(manifest.song.remote_song_id) << "\",\n";
    output << "  \"localSongId\": \"" << json::escape_string(manifest.local_song_id) << "\",\n";
    output << "  \"songVersion\": " << std::max(0, manifest.song.song_version) << ",\n";
    output << "  \"revisionId\": \"" << json::escape_string(manifest.song.revision_id) << "\",\n";
    output << "  \"packageId\": \"" << json::escape_string(manifest.song.package_id) << "\",\n";
    output << "  \"keyId\": \"" << json::escape_string(manifest.key_id) << "\",\n";
    output << "  \"contentKeyVersion\": " << std::max(0, manifest.content_key_version) << ",\n";
    output << "  \"encryptionScheme\": \"" << json::escape_string(manifest.encryption_scheme) << "\",\n";
    output << "  \"license\": {"
           << "\"expiresAt\": \"" << json::escape_string(manifest.license_expires_at) << "\", "
           << "\"offlineExpiresAt\": \"" << json::escape_string(manifest.offline_license_expires_at) << "\", "
           << "\"revoked\": " << (manifest.license_revoked ? "true" : "false")
           << "},\n";
    output << "  \"songJsonHash\": \"" << json::escape_string(manifest.song_json_hash) << "\",\n";
    output << "  \"songJsonFingerprint\": \"" << json::escape_string(manifest.song_json_fingerprint) << "\",\n";
    output << "  \"audioHash\": \"" << json::escape_string(manifest.audio_hash) << "\",\n";
    output << "  \"jacketHash\": \"" << json::escape_string(manifest.jacket_hash) << "\",\n";
    output << "  \"remoteSongJsonHash\": \"" << json::escape_string(manifest.remote_song_json_hash) << "\",\n";
    output << "  \"remoteSongJsonFingerprint\": \"" << json::escape_string(manifest.remote_song_json_fingerprint) << "\",\n";
    output << "  \"remoteAudioHash\": \"" << json::escape_string(manifest.remote_audio_hash) << "\",\n";
    output << "  \"remoteJacketHash\": \"" << json::escape_string(manifest.remote_jacket_hash) << "\",\n";
    output << "  \"createdAt\": \"" << json::escape_string(manifest.created_at) << "\",\n";
    output << "  \"updatedAt\": \"" << json::escape_string(manifest.updated_at) << "\",\n";
    output << "  \"assets\": {\n";
    output << "    \"songJson\": ";
    write_asset(output, manifest.song_json_asset, 4);
    output << ",\n";
    output << "    \"audio\": ";
    write_asset(output, manifest.audio_asset, 4);
    output << ",\n";
    output << "    \"jacket\": ";
    write_asset(output, manifest.jacket_asset, 4);
    output << "\n";
    output << "  },\n";
    output << "  \"charts\": [\n";
    for (size_t index = 0; index < manifest.charts.size(); ++index) {
        const chart_manifest_entry& chart = manifest.charts[index];
        output << "    {\n";
        output << "      \"remoteChartId\": \"" << json::escape_string(chart.remote_chart_id) << "\",\n";
        output << "      \"localChartId\": \"" << json::escape_string(chart.local_chart_id) << "\",\n";
        output << "      \"chartVersion\": " << std::max(0, chart.chart_version) << ",\n";
        output << "      \"revisionId\": \"" << json::escape_string(chart.revision_id) << "\",\n";
        output << "      \"chartHash\": \"" << json::escape_string(chart.chart_hash) << "\",\n";
        output << "      \"chartFingerprint\": \"" << json::escape_string(chart.chart_fingerprint) << "\",\n";
        output << "      \"remoteChartHash\": \"" << json::escape_string(chart.remote_chart_hash) << "\",\n";
        output << "      \"remoteChartFingerprint\": \"" << json::escape_string(chart.remote_chart_fingerprint) << "\",\n";
        output << "      \"asset\": ";
        write_asset(output, chart.encrypted_chart, 6);
        output << "\n";
        output << "    }";
        if (index + 1 < manifest.charts.size()) {
            output << ",";
        }
        output << "\n";
    }
    output << "  ]\n";
    output << "}\n";

    if (!output.good()) {
        error_message = "Failed to write managed content manifest.";
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
            error_message = "Failed to replace managed content manifest.";
            return false;
        }
    }
    return true;
}

bool write_manifest(package_manifest manifest, std::string& error_message) {
    const fs::path path = manifest_path(manifest.song);
    return write_manifest_at_path(std::move(manifest), path, error_message);
}

void upsert_chart(package_manifest& manifest, const chart_identity& identity) {
    chart_manifest_entry next{
        .local_chart_id = local_chart_id(identity),
        .remote_chart_id = identity.remote_chart_id,
        .chart_version = identity.chart_version,
        .revision_id = identity.revision_id,
        .chart_hash = identity.chart_hash,
        .chart_fingerprint = identity.chart_fingerprint,
        .remote_chart_hash = identity.remote_chart_hash,
        .remote_chart_fingerprint = identity.remote_chart_fingerprint,
    };

    const auto existing = std::find_if(manifest.charts.begin(), manifest.charts.end(),
                                       [&](const chart_manifest_entry& chart) {
                                           return chart.remote_chart_id == next.remote_chart_id ||
                                                  chart.local_chart_id == next.local_chart_id;
                                       });
    if (existing == manifest.charts.end()) {
        manifest.charts.push_back(std::move(next));
    } else {
        if (asset_empty(next.encrypted_chart)) {
            next.encrypted_chart = existing->encrypted_chart;
        }
        *existing = std::move(next);
    }
}

package_relocation_result relocate_package_source(const fs::path& song_directory,
                                                  online_content::source target_source,
                                                  std::string& error_message) {
    package_relocation_result result;
    std::optional<package_manifest> manifest = read_manifest(song_directory);
    if (!manifest.has_value()) {
        error_message = "Managed content manifest was not found.";
        return result;
    }

    const fs::path current_dir = song_directory.lexically_normal();
    if (manifest->song.source == target_source) {
        result.success = true;
        result.song_directory = current_dir;
        return result;
    }

    manifest->song.source = target_source;
    const fs::path target_dir = managed_content_storage::song_directory(manifest->song);
    std::error_code ec;
    if (fs::equivalent(current_dir, target_dir, ec)) {
        if (!write_manifest_at_path(*manifest, manifest_path(current_dir), error_message)) {
            return result;
        }
        result.success = true;
        result.song_directory = current_dir;
        return result;
    }

    ec.clear();
    if (fs::exists(target_dir, ec)) {
        error_message = "Managed content target package already exists.";
        return result;
    }

    fs::create_directories(target_dir.parent_path(), ec);
    if (ec) {
        error_message = "Failed to prepare managed content target directory.";
        return result;
    }

    ec.clear();
    fs::rename(current_dir, target_dir, ec);
    if (ec) {
        error_message = "Failed to relocate managed content package.";
        return result;
    }

    if (!write_manifest_at_path(*manifest, manifest_path(target_dir), error_message)) {
        return result;
    }

    result.success = true;
    result.relocated = true;
    result.song_directory = target_dir;
    return result;
}

chart_promotion_result promote_plain_chart_to_managed(const fs::path& song_directory,
                                                      const chart_identity& identity,
                                                      const fs::path& plain_chart_path,
                                                      bool remove_plain_file,
                                                      std::string& error_message) {
    chart_promotion_result result;
    const std::string chart_text = read_file(plain_chart_path);
    if (chart_text.empty()) {
        error_message = "Plain chart file was not found.";
        return result;
    }

    std::optional<package_manifest> manifest = read_manifest(song_directory);
    if (!manifest.has_value()) {
        error_message = "Managed content manifest was not found.";
        return result;
    }

    const fs::path original_song_dir = song_directory.lexically_normal();
    fs::path target_song_dir = original_song_dir;
    fs::path relocated_plain_path = plain_chart_path;
    if (manifest->song.source != identity.source) {
        package_relocation_result relocation =
            relocate_package_source(original_song_dir, identity.source, error_message);
        if (!relocation.success) {
            return result;
        }
        target_song_dir = relocation.song_directory;
        manifest = read_manifest(target_song_dir);
        if (!manifest.has_value()) {
            error_message = "Managed content manifest was not found after relocation.";
            return result;
        }

        std::error_code rel_ec;
        const fs::path relative_chart_path = fs::relative(plain_chart_path, original_song_dir, rel_ec);
        const bool relative_inside = !rel_ec &&
            !relative_chart_path.empty() &&
            std::none_of(relative_chart_path.begin(), relative_chart_path.end(), [](const fs::path& part) {
                return part == "..";
            });
        if (relative_inside) {
            relocated_plain_path = target_song_dir / relative_chart_path;
        }
    }

    chart_identity normalized_identity = identity;
    normalized_identity.source = manifest->song.source;
    if (normalized_identity.server_url.empty()) {
        normalized_identity.server_url = manifest->song.server_url;
    }
    if (normalized_identity.remote_song_id.empty()) {
        normalized_identity.remote_song_id = manifest->song.remote_song_id;
    }
    if (normalized_identity.song_version <= 0) {
        normalized_identity.song_version = manifest->song.song_version;
    }
    const std::string local_id = local_chart_id(normalized_identity);
    const std::optional<std::string> normalized_chart_text = replace_chart_id_line(chart_text, local_id);
    if (!normalized_chart_text.has_value()) {
        error_message = "Failed to prepare promoted chart data.";
        return result;
    }

    upsert_chart(*manifest, normalized_identity);

    chart_manifest_entry* target_chart = nullptr;
    for (chart_manifest_entry& chart : manifest->charts) {
        if (chart.local_chart_id == local_id) {
            target_chart = &chart;
            break;
        }
    }
    if (target_chart == nullptr) {
        error_message = "Failed to register managed chart.";
        return result;
    }

    const std::string logical_path =
        path_utils::to_utf8(fs::path("charts") / (local_id + ".rchart"));
    if (!write_encrypted_asset(*manifest,
                               target_song_dir,
                               logical_path,
                               *normalized_chart_text,
                               target_chart->encrypted_chart,
                               error_message)) {
        return result;
    }
    target_chart->chart_hash = target_chart->encrypted_chart.content_hash;
    if (!normalized_identity.chart_fingerprint.empty()) {
        target_chart->chart_fingerprint = normalized_identity.chart_fingerprint;
    }

    if (!write_manifest_at_path(*manifest, manifest_path(target_song_dir), error_message)) {
        return result;
    }

    if (remove_plain_file) {
        std::error_code ec;
        fs::remove(relocated_plain_path, ec);
        if (ec) {
            error_message = "Failed to remove the promoted plain chart file.";
            return result;
        }
    }

    result.success = true;
    result.song_directory = target_song_dir;
    result.chart_path = chart_file_path(target_song_dir, local_id);
    result.local_chart_id = local_id;
    return result;
}

const char* default_encryption_scheme() {
    return kDefaultEncryptionScheme;
}

bool has_encrypted_assets(const package_manifest& manifest) {
    if (!asset_empty(manifest.song_json_asset) ||
        !asset_empty(manifest.audio_asset) ||
        !asset_empty(manifest.jacket_asset)) {
        return true;
    }
    return std::any_of(manifest.charts.begin(), manifest.charts.end(), [](const chart_manifest_entry& chart) {
        return !asset_empty(chart.encrypted_chart);
    });
}

fs::path encrypted_asset_path(const fs::path& song_directory,
                              const std::string& logical_relative_path) {
    return song_directory / fs::path(encrypted_relative_path_for(logical_relative_path));
}

fs::path encrypted_asset_path(const fs::path& song_directory,
                              const encrypted_asset_metadata& asset) {
    std::string error_message;
    if (const std::optional<fs::path> path =
            validated_encrypted_asset_path(song_directory, asset, error_message)) {
        return *path;
    }
    return song_directory / kEncryptedDirectoryName / "__invalid__";
}

bool write_encrypted_asset(package_manifest& manifest,
                           const fs::path& song_directory,
                           const std::string& logical_relative_path,
                           std::string_view plaintext,
                           encrypted_asset_metadata& asset,
                           std::string& error_message) {
    if (plaintext.empty()) {
        error_message = "Managed package asset is empty.";
        return false;
    }
    ensure_encryption_metadata(manifest);
    const std::optional<std::string> key_hex = dev_content_key_hex(manifest, error_message);
    if (!key_hex.has_value()) {
        return false;
    }

    const std::optional<std::string> logical_path = safe_relative_path(logical_relative_path);
    if (!logical_path.has_value()) {
        error_message = "Managed package asset path is invalid.";
        return false;
    }

    asset.logical_path = *logical_path;
    asset.encrypted_path = encrypted_relative_path_for(asset.logical_path);
    if (asset.encrypted_path.empty()) {
        error_message = "Managed package encrypted asset path is invalid.";
        return false;
    }
    asset.nonce = random_nonce_hex();
    asset.content_hash = bytes_sha256_hex(plaintext);
    asset.size_bytes = plaintext.size();

    const std::optional<fs::path> target_path =
        validated_encrypted_asset_path(song_directory, asset, error_message);
    if (!target_path.has_value()) {
        return false;
    }

    std::vector<unsigned char> ciphertext = xor_crypt(plaintext, *key_hex, asset.nonce);
    asset.ciphertext_hash = bytes_sha256_hex(ciphertext);
    return write_binary_file(*target_path, ciphertext, error_message);
}

managed_file_read_result read_encrypted_asset(const package_manifest& manifest,
                                              const fs::path& song_directory,
                                              const encrypted_asset_metadata& asset) {
    managed_file_read_result result;
    result.managed = true;
    if (asset_empty(asset)) {
        result.error_message = "Managed package asset is missing encryption metadata.";
        return result;
    }

    std::string error_message;
    const std::optional<std::string> key_hex = dev_content_key_hex(manifest, error_message);
    if (!key_hex.has_value()) {
        result.error_message = error_message.empty() ? "Managed content license was unavailable." : error_message;
        return result;
    }

    const std::optional<fs::path> validated_path =
        validated_encrypted_asset_path(song_directory, asset, result.error_message);
    if (!validated_path.has_value()) {
        return result;
    }
    const fs::path path = *validated_path;
    const std::vector<unsigned char> ciphertext = read_binary_file(path);
    if (ciphertext.empty()) {
        result.error_message = "Managed package encrypted asset is missing.";
        return result;
    }

    const std::string ciphertext_hash = bytes_sha256_hex(ciphertext);
    if (!asset.ciphertext_hash.empty() && ciphertext_hash != asset.ciphertext_hash) {
        result.error_message = "Managed package encrypted asset hash mismatch.";
        return result;
    }

    std::vector<unsigned char> plaintext = xor_crypt(
        std::string_view(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size()),
        *key_hex,
        asset.nonce);
    const std::string content_hash = bytes_sha256_hex(plaintext);
    if (!asset.content_hash.empty() && content_hash != asset.content_hash) {
        result.error_message = "Managed package decrypted asset hash mismatch.";
        return result;
    }

    result.bytes = std::move(plaintext);
    result.success = true;
    return result;
}

managed_file_read_result read_managed_file(const fs::path& file_path) {
    managed_file_read_result result;
    const std::optional<std::pair<fs::path, package_manifest>> found = manifest_for_file(file_path);
    if (!found.has_value()) {
        return result;
    }

    const fs::path& package_dir = found->first;
    const package_manifest& manifest = found->second;
    const encrypted_asset_metadata* asset = nullptr;
    if (path_matches_asset(package_dir, manifest.song_json_asset, file_path)) {
        asset = &manifest.song_json_asset;
    } else if (path_matches_asset(package_dir, manifest.audio_asset, file_path)) {
        asset = &manifest.audio_asset;
    } else if (path_matches_asset(package_dir, manifest.jacket_asset, file_path)) {
        asset = &manifest.jacket_asset;
    } else {
        for (const chart_manifest_entry& chart : manifest.charts) {
            if (path_matches_asset(package_dir, chart.encrypted_chart, file_path)) {
                asset = &chart.encrypted_chart;
                break;
            }
        }
    }

    if (asset == nullptr) {
        return result;
    }
    return read_encrypted_asset(manifest, package_dir, *asset);
}

managed_chart_file_info describe_managed_chart_file(const fs::path& file_path) {
    managed_chart_file_info result;
    const std::optional<std::pair<fs::path, package_manifest>> found = manifest_for_file(file_path);
    if (!found.has_value()) {
        return result;
    }

    const fs::path& package_dir = found->first;
    const package_manifest& manifest = found->second;
    for (const chart_manifest_entry& chart : manifest.charts) {
        if (path_matches_asset(package_dir, chart.encrypted_chart, file_path)) {
            result.managed = true;
            result.local_chart_id = chart.local_chart_id;
            return result;
        }
    }
    return result;
}

managed_file_write_result write_managed_chart_file(const fs::path& file_path,
                                                   std::string_view plaintext,
                                                   std::string_view chart_fingerprint_hash) {
    managed_file_write_result result;
    const std::optional<std::pair<fs::path, package_manifest>> found = manifest_for_file(file_path);
    if (!found.has_value()) {
        return result;
    }

    const fs::path& package_dir = found->first;
    package_manifest manifest = found->second;
    chart_manifest_entry* target_chart = nullptr;
    for (chart_manifest_entry& chart : manifest.charts) {
        if (path_matches_asset(package_dir, chart.encrypted_chart, file_path)) {
            target_chart = &chart;
            break;
        }
    }
    if (target_chart == nullptr) {
        return result;
    }

    result.managed = true;
    const std::string logical_path = target_chart->encrypted_chart.logical_path.empty()
        ? path_utils::to_utf8(chart_file_path(package_dir, target_chart->local_chart_id).lexically_relative(package_dir))
        : target_chart->encrypted_chart.logical_path;
    std::string error_message;
    if (!write_encrypted_asset(manifest,
                               package_dir,
                               logical_path,
                               plaintext,
                               target_chart->encrypted_chart,
                               error_message)) {
        result.error_message = error_message;
        return result;
    }
    target_chart->chart_hash = target_chart->encrypted_chart.content_hash;
    if (!chart_fingerprint_hash.empty()) {
        target_chart->chart_fingerprint = std::string(chart_fingerprint_hash);
    }
    if (!write_manifest_at_path(manifest, manifest_path(package_dir), error_message)) {
        result.error_message = error_message;
        return result;
    }

    result.success = true;
    return result;
}

std::vector<fs::path> list_package_directories(online_content::source source) {
    std::vector<fs::path> result;
    const fs::path root = content_cache_paths::source_root(source) / "songs";
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

bool is_within_content_cache(const fs::path& path) {
    return is_within_root(path, app_paths::content_cache_root());
}

}  // namespace managed_content_storage
