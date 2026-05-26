#include "content_cache_paths.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

#include "app_paths.h"

namespace content_cache_paths {
namespace {

bool is_safe_key_byte(unsigned char ch) {
    return std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.';
}

std::string encode_component(const std::string& value) {
    std::ostringstream encoded;
    encoded << value.size() << '-';
    for (const unsigned char ch : value) {
        if (is_safe_key_byte(ch)) {
            encoded << static_cast<char>(ch);
            continue;
        }

        encoded << '~'
                << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch)
                << std::dec << std::setw(0);
    }
    return encoded.str();
}

int stable_version(int version) {
    return std::max(0, version);
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
    return "song_server-" + encode_component(parts.server_url) +
           "_remote-song-" + encode_component(parts.remote_song_id) +
           "_song-v-" + std::to_string(stable_version(parts.song_version)) +
           "_rev-" + encode_component(parts.revision_id);
}

std::string chart_cache_key(const chart_cache_key_parts& parts) {
    return "chart_server-" + encode_component(parts.server_url) +
           "_remote-song-" + encode_component(parts.remote_song_id) +
           "_remote-chart-" + encode_component(parts.remote_chart_id) +
           "_song-v-" + std::to_string(stable_version(parts.song_version)) +
           "_chart-v-" + std::to_string(stable_version(parts.chart_version)) +
           "_rev-" + encode_component(parts.revision_id);
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

}  // namespace content_cache_paths
