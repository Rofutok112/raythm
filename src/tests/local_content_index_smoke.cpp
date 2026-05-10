#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "title/local_content_index.h"

namespace {
bool set_local_app_data(const std::filesystem::path& path) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", path.string().c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", path.string().c_str(), 1) == 0;
#endif
}
}

int main() {
    const std::filesystem::path appdata_root =
        std::filesystem::temp_directory_path() / "raythm-local-content-index-smoke";
    std::error_code ec;
    std::filesystem::remove_all(appdata_root, ec);
    if (!set_local_app_data(appdata_root)) {
        std::cerr << "failed to set LOCALAPPDATA\n";
        return EXIT_FAILURE;
    }

    local_content_index::put_song_binding({
        .server_url = "https://server.example",
        .local_song_id = "local-song",
        .remote_song_id = "remote-song",
        .origin = local_content_index::online_origin::owned_upload,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "local-chart",
        .remote_chart_id = "remote-chart",
        .remote_song_id = "remote-song",
        .remote_chart_version = 7,
        .origin = local_content_index::online_origin::downloaded,
    });

    const auto song_by_local =
        local_content_index::find_song_by_local("https://server.example", "local-song");
    const auto song_by_remote =
        local_content_index::find_song_by_remote("https://server.example", "remote-song");
    const auto chart_by_local =
        local_content_index::find_chart_by_local("https://server.example", "local-chart");
    const auto chart_by_remote =
        local_content_index::find_chart_by_remote("https://server.example", "remote-chart");

    bool ok = true;
    ok = song_by_local.has_value() && song_by_local->remote_song_id == "remote-song" && ok;
    ok = song_by_remote.has_value() && song_by_remote->local_song_id == "local-song" && ok;
    ok = chart_by_local.has_value() && chart_by_local->remote_chart_id == "remote-chart" && ok;
    ok = chart_by_remote.has_value() && chart_by_remote->local_chart_id == "local-chart" && ok;
    ok = chart_by_local.has_value() &&
         chart_by_local->remote_chart_version == 7 &&
         chart_by_local->origin == local_content_index::online_origin::downloaded && ok;

    std::filesystem::remove_all(appdata_root, ec);
    if (!ok) {
        std::cerr << "local_content_index smoke test failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "local_content_index smoke test passed\n";
    return EXIT_SUCCESS;
}
