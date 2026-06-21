#include "content_cache_paths.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>

#include "app_paths.h"

namespace content_cache_paths {
namespace {

int stable_version(int version) {
    return std::max(0, version);
}

void append_key_part(std::ostringstream& stream, const std::string& value) {
    stream << value.size() << ':' << value << '|';
}

std::uint64_t fnv1a64(const std::string& value, std::uint64_t seed) {
    std::uint64_t hash = seed;
    for (const unsigned char ch : value) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string hex64(std::uint64_t value) {
    std::ostringstream stream;
    stream << std::hex << std::setw(16) << std::setfill('0') << value;
    return stream.str();
}

std::string stable_digest(const std::string& value) {
    return hex64(fnv1a64(value, 14695981039346656037ull)) +
           hex64(fnv1a64(value, 1099511628211ull));
}

song_cache_key_parts song_parts_from_chart(const chart_cache_key_parts& parts) {
    return {
        .server_url = parts.server_url,
        .remote_song_id = parts.remote_song_id,
        .song_version = parts.song_version,
        .revision_id = parts.revision_id,
    };
}

}  // namespace

std::string song_cache_key(const song_cache_key_parts& parts) {
    std::ostringstream material;
    append_key_part(material, parts.server_url);
    append_key_part(material, parts.remote_song_id);
    material << stable_version(parts.song_version) << '|';
    append_key_part(material, parts.revision_id);
    return "song_" + stable_digest(material.str());
}

std::string chart_cache_key(const chart_cache_key_parts& parts) {
    std::ostringstream material;
    append_key_part(material, parts.server_url);
    append_key_part(material, parts.remote_song_id);
    append_key_part(material, parts.remote_chart_id);
    material << stable_version(parts.song_version) << '|'
             << stable_version(parts.chart_version) << '|';
    append_key_part(material, parts.revision_id);
    return "chart_" + stable_digest(material.str());
}

std::string mv_cache_key(const mv_cache_key_parts& parts) {
    std::ostringstream material;
    append_key_part(material, parts.server_url);
    append_key_part(material, parts.remote_song_id);
    append_key_part(material, parts.remote_mv_id);
    material << stable_version(parts.song_version) << '|'
             << stable_version(parts.mv_version) << '|';
    append_key_part(material, parts.revision_id);
    return "mv_" + stable_digest(material.str());
}

std::filesystem::path source_root(online_content::source source) {
    return source == online_content::source::official
        ? app_paths::official_content_cache_root()
        : app_paths::community_content_cache_root();
}

std::filesystem::path song_dir(online_content::source source, const song_cache_key_parts& parts) {
    return source_root(source) / "songs" / song_cache_key(parts);
}

std::filesystem::path charts_dir(online_content::source source, const song_cache_key_parts& parts) {
    return song_dir(source, parts) / "charts";
}

std::filesystem::path chart_path(online_content::source source, const chart_cache_key_parts& parts) {
    return charts_dir(source, song_parts_from_chart(parts)) / (chart_cache_key(parts) + ".rchart");
}

std::filesystem::path managed_package_manifest_path(online_content::source source,
                                                    const song_cache_key_parts& parts) {
    return song_dir(source, parts) / "managed-package.json";
}

std::filesystem::path mv_dir(online_content::source source, const mv_cache_key_parts& parts) {
    return source_root(source) / "mvs" / mv_cache_key(parts);
}

std::filesystem::path mv_managed_package_manifest_path(online_content::source source,
                                                       const mv_cache_key_parts& parts) {
    return mv_dir(source, parts) / "managed-package.json";
}

}  // namespace content_cache_paths
