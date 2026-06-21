#include "content_cache_paths.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#include "app_paths.h"

namespace {
namespace fs = std::filesystem;

bool set_local_app_data(const fs::path& path) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", path.string().c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", path.string().c_str(), 1) == 0;
#endif
}

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

bool is_safe_path_component(const std::string& value) {
    return value.find('/') == std::string::npos &&
           value.find('\\') == std::string::npos &&
           value.find(':') == std::string::npos &&
           value.find('?') == std::string::npos &&
           value.find('*') == std::string::npos &&
           value.find('"') == std::string::npos &&
           value.find('<') == std::string::npos &&
           value.find('>') == std::string::npos &&
           value.find('|') == std::string::npos;
}

}  // namespace

int main() {
    const fs::path temp_local_app_data = fs::temp_directory_path() / "raythm-content-cache-paths-smoke";
    std::error_code ec;
    fs::remove_all(temp_local_app_data, ec);
    fs::create_directories(temp_local_app_data, ec);
    if (ec) {
        std::cerr << "Failed to prepare temporary LOCALAPPDATA root\n";
        return EXIT_FAILURE;
    }
    if (!set_local_app_data(temp_local_app_data)) {
        std::cerr << "Failed to update LOCALAPPDATA for smoke test\n";
        return EXIT_FAILURE;
    }

    bool ok = true;
    app_paths::ensure_directories();

    const fs::path root = temp_local_app_data / "raythm";
    expect(app_paths::songs_root() == root / "songs",
           "Expected songs_root to remain the local plain workspace.",
           ok);
    expect(app_paths::content_cache_root() == root / "content-cache",
           "Expected content cache root under app data.",
           ok);
    expect(app_paths::community_content_cache_root() == root / "content-cache" / "community",
           "Expected community cache root under content-cache.",
           ok);
    expect(app_paths::official_content_cache_root() == root / "content-cache" / "official",
           "Expected official cache root under content-cache.",
           ok);
    expect(fs::is_directory(app_paths::community_content_cache_root()),
           "Expected community cache root to be created.",
           ok);
    expect(fs::is_directory(app_paths::official_content_cache_root()),
           "Expected official cache root to be created.",
           ok);

    const content_cache_paths::song_cache_key_parts song_a{
        .server_url = "https://server.example/api",
        .remote_song_id = "song/with:unsafe?chars",
        .song_version = 3,
    };
    const content_cache_paths::song_cache_key_parts song_a_again = song_a;
    const content_cache_paths::song_cache_key_parts song_other_server{
        .server_url = "https://other.example/api",
        .remote_song_id = song_a.remote_song_id,
        .song_version = song_a.song_version,
    };
    const content_cache_paths::song_cache_key_parts song_other_version{
        .server_url = song_a.server_url,
        .remote_song_id = song_a.remote_song_id,
        .song_version = 4,
    };

    const std::string song_key = content_cache_paths::song_cache_key(song_a);
    expect(song_key == content_cache_paths::song_cache_key(song_a_again),
           "Expected identical remote song identity to produce a stable key.",
           ok);
    expect(song_key != content_cache_paths::song_cache_key(song_other_server),
           "Expected server URL to be part of the song cache key.",
           ok);
    expect(song_key != content_cache_paths::song_cache_key(song_other_version),
           "Expected song version to be part of the song cache key.",
           ok);
    expect(is_safe_path_component(song_key),
           "Expected song cache key to avoid path separators and reserved characters.",
           ok);

    const content_cache_paths::chart_cache_key_parts chart_a{
        .server_url = song_a.server_url,
        .remote_song_id = song_a.remote_song_id,
        .remote_chart_id = "chart/with:unsafe?chars",
        .song_version = song_a.song_version,
        .chart_version = 7,
    };
    const content_cache_paths::chart_cache_key_parts chart_other_id{
        .server_url = chart_a.server_url,
        .remote_song_id = chart_a.remote_song_id,
        .remote_chart_id = "other-chart",
        .song_version = chart_a.song_version,
        .chart_version = chart_a.chart_version,
    };
    const content_cache_paths::chart_cache_key_parts chart_other_version{
        .server_url = chart_a.server_url,
        .remote_song_id = chart_a.remote_song_id,
        .remote_chart_id = chart_a.remote_chart_id,
        .song_version = chart_a.song_version,
        .chart_version = 8,
    };

    const std::string chart_key = content_cache_paths::chart_cache_key(chart_a);
    expect(is_safe_path_component(chart_key),
           "Expected chart cache key to avoid path separators and reserved characters.",
           ok);
    expect(chart_key != content_cache_paths::chart_cache_key(chart_other_id),
           "Expected remote chart ID to be part of the chart cache key.",
           ok);
    expect(chart_key != content_cache_paths::chart_cache_key(chart_other_version),
           "Expected chart version to be part of the chart cache key.",
           ok);

    expect(content_cache_paths::source_root(online_content::source::community) ==
               app_paths::community_content_cache_root(),
           "Expected community source root to use the community cache.",
           ok);
    expect(content_cache_paths::source_root(online_content::source::official) ==
               app_paths::official_content_cache_root(),
           "Expected official source root to use the official cache.",
           ok);
    expect(content_cache_paths::song_dir(online_content::source::community, song_a).parent_path().filename() ==
               "songs",
           "Expected managed song directories to live below a songs bucket in the content cache.",
           ok);
    expect(content_cache_paths::managed_package_manifest_path(online_content::source::community, song_a).filename() ==
               "managed-package.json",
           "Expected managed package manifest path to use managed-package.json.",
           ok);
    expect(content_cache_paths::chart_path(online_content::source::official, chart_a).extension() == ".rchart",
           "Expected chart cache path to preserve the chart file extension.",
           ok);

    const content_cache_paths::mv_cache_key_parts mv_a{
        .server_url = song_a.server_url,
        .remote_song_id = song_a.remote_song_id,
        .remote_mv_id = "mv/with:unsafe?chars",
        .song_version = song_a.song_version,
        .mv_version = 2,
        .revision_id = "mv-rev-2",
    };
    const content_cache_paths::mv_cache_key_parts mv_other_id{
        .server_url = mv_a.server_url,
        .remote_song_id = mv_a.remote_song_id,
        .remote_mv_id = "other-mv",
        .song_version = mv_a.song_version,
        .mv_version = mv_a.mv_version,
        .revision_id = mv_a.revision_id,
    };
    const content_cache_paths::mv_cache_key_parts mv_other_version{
        .server_url = mv_a.server_url,
        .remote_song_id = mv_a.remote_song_id,
        .remote_mv_id = mv_a.remote_mv_id,
        .song_version = mv_a.song_version,
        .mv_version = 3,
        .revision_id = mv_a.revision_id,
    };
    const std::string mv_key = content_cache_paths::mv_cache_key(mv_a);
    expect(mv_key.rfind("mv_", 0) == 0,
           "Expected MV cache key to use the mv_ namespace.",
           ok);
    expect(is_safe_path_component(mv_key),
           "Expected MV cache key to avoid path separators and reserved characters.",
           ok);
    expect(mv_key != content_cache_paths::mv_cache_key(mv_other_id),
           "Expected remote MV ID to be part of the MV cache key.",
           ok);
    expect(mv_key != content_cache_paths::mv_cache_key(mv_other_version),
           "Expected MV version to be part of the MV cache key.",
           ok);
    expect(content_cache_paths::mv_dir(online_content::source::community, mv_a).parent_path().filename() == "mvs",
           "Expected managed MV directories to live below an mvs bucket in the content cache.",
           ok);
    expect(content_cache_paths::mv_managed_package_manifest_path(online_content::source::community, mv_a).filename() ==
               "managed-package.json",
           "Expected managed MV manifest path to use managed-package.json.",
           ok);
    expect(content_cache_paths::mv_dir(online_content::source::community, mv_a).string().find("charts") ==
               std::string::npos,
           "Expected managed MV paths to stay independent from chart storage.",
           ok);

    fs::remove_all(temp_local_app_data, ec);
    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "content_cache_paths smoke test passed\n";
    return EXIT_SUCCESS;
}
