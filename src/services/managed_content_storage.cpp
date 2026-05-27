#include "services/managed_content_storage.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#include "app_paths.h"
#include "content_cache_paths.h"
#include "network/json_helpers.h"

namespace managed_content_storage {
namespace {
namespace fs = std::filesystem;
namespace json = network::json;

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
        online_content::source_from_string(json::extract_string(content, "source").value_or(""));
    if (!source.has_value()) {
        return std::nullopt;
    }

    package_manifest manifest;
    manifest.song.source = *source;
    manifest.song.server_url = json::extract_string(content, "serverUrl").value_or("");
    manifest.song.remote_song_id = json::extract_string(content, "remoteSongId").value_or("");
    manifest.song.song_version = json::extract_int(content, "songVersion").value_or(0);
    manifest.song.revision_id = json::extract_string(content, "revisionId").value_or("");
    manifest.local_song_id = json::extract_string(content, "localSongId").value_or("");

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
            if (!chart.local_chart_id.empty() && !chart.remote_chart_id.empty()) {
                manifest.charts.push_back(std::move(chart));
            }
        }
    }

    return manifest;
}

bool write_manifest(package_manifest manifest, std::string& error_message) {
    if (manifest.song.server_url.empty() || manifest.song.remote_song_id.empty()) {
        error_message = "Managed content manifest is missing remote song identity.";
        return false;
    }
    if (manifest.local_song_id.empty()) {
        manifest.local_song_id = local_song_id(manifest.song);
    }

    const fs::path path = manifest_path(manifest.song);
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error_message = "Failed to open managed content manifest for writing.";
        return false;
    }

    output << "{\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"source\": \"" << online_content::source_label(manifest.song.source) << "\",\n";
    output << "  \"serverUrl\": \"" << json::escape_string(manifest.song.server_url) << "\",\n";
    output << "  \"remoteSongId\": \"" << json::escape_string(manifest.song.remote_song_id) << "\",\n";
    output << "  \"localSongId\": \"" << json::escape_string(manifest.local_song_id) << "\",\n";
    output << "  \"songVersion\": " << std::max(0, manifest.song.song_version) << ",\n";
    output << "  \"revisionId\": \"" << json::escape_string(manifest.song.revision_id) << "\",\n";
    output << "  \"charts\": [\n";
    for (size_t index = 0; index < manifest.charts.size(); ++index) {
        const chart_manifest_entry& chart = manifest.charts[index];
        output << "    {"
               << "\"remoteChartId\": \"" << json::escape_string(chart.remote_chart_id) << "\", "
               << "\"localChartId\": \"" << json::escape_string(chart.local_chart_id) << "\", "
               << "\"chartVersion\": " << std::max(0, chart.chart_version) << ", "
               << "\"revisionId\": \"" << json::escape_string(chart.revision_id) << "\""
               << "}";
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
    return true;
}

void upsert_chart(package_manifest& manifest, const chart_identity& identity) {
    chart_manifest_entry next{
        .local_chart_id = local_chart_id(identity),
        .remote_chart_id = identity.remote_chart_id,
        .chart_version = identity.chart_version,
        .revision_id = identity.revision_id,
    };

    const auto existing = std::find_if(manifest.charts.begin(), manifest.charts.end(),
                                       [&](const chart_manifest_entry& chart) {
                                           return chart.remote_chart_id == next.remote_chart_id ||
                                                  chart.local_chart_id == next.local_chart_id;
                                       });
    if (existing == manifest.charts.end()) {
        manifest.charts.push_back(std::move(next));
    } else {
        *existing = std::move(next);
    }
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
