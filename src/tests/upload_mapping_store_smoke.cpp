#include "app_paths.h"
#include "title/local_content_index.h"
#include "title/upload_mapping_store.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#ifdef _WIN32
#include <stdlib.h>
#endif

namespace fs = std::filesystem;

bool set_local_app_data(const fs::path& path) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", path.string().c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", path.string().c_str(), 1) == 0;
#endif
}

int main() {
    const fs::path temp_root = fs::temp_directory_path() / "raythm-upload-mapping-smoke";
    std::error_code ec;
    fs::remove_all(temp_root, ec);
    if (!set_local_app_data(temp_root)) {
        std::cerr << "failed to set LOCALAPPDATA\n";
        return 1;
    }

    title_upload_mapping::store mappings;
    title_upload_mapping::put_song(mappings, "https://server.example", "local-song", "remote-song",
                                   title_upload_mapping::mapping_origin::owned_upload);
    title_upload_mapping::put_song(mappings, "https://server.example", "downloaded-song", "remote-downloaded",
                                   title_upload_mapping::mapping_origin::downloaded);
    title_upload_mapping::put_chart(mappings, "https://server.example", "local-chart", "local-song",
                                    "remote-chart", "remote-song",
                                    title_upload_mapping::mapping_origin::owned_upload);
    if (!title_upload_mapping::save(mappings)) {
        std::cerr << "failed to save mappings\n";
        return 1;
    }

    const title_upload_mapping::store loaded = title_upload_mapping::load();
    if (title_upload_mapping::find_remote_song_id(loaded, "https://server.example", "local-song") !=
        std::optional<std::string>("remote-song")) {
        std::cerr << "remote song mapping missing\n";
        return 1;
    }
    if (title_upload_mapping::find_local_song_id(loaded, "https://server.example", "remote-song") !=
        std::optional<std::string>("local-song")) {
        std::cerr << "local song mapping missing\n";
        return 1;
    }
    if (title_upload_mapping::find_song_origin(loaded, "https://server.example", "downloaded-song") !=
        std::optional<title_upload_mapping::mapping_origin>(title_upload_mapping::mapping_origin::downloaded)) {
        std::cerr << "song mapping origin missing\n";
        return 1;
    }
    if (title_upload_mapping::find_remote_chart_id(loaded, "https://server.example", "local-chart") !=
        std::optional<std::string>("remote-chart")) {
        std::cerr << "remote chart mapping missing\n";
        return 1;
    }
    if (title_upload_mapping::find_local_chart_id(loaded, "https://server.example", "remote-chart") !=
        std::optional<std::string>("local-chart")) {
        std::cerr << "local chart mapping missing\n";
        return 1;
    }
    if (title_upload_mapping::find_chart_origin(loaded, "https://server.example", "local-chart") !=
        std::optional<title_upload_mapping::mapping_origin>(title_upload_mapping::mapping_origin::owned_upload)) {
        std::cerr << "chart mapping origin missing\n";
        return 1;
    }

    fs::remove_all(temp_root, ec);
    fs::create_directories(app_paths::app_data_root(), ec);
    {
        std::ofstream legacy(app_paths::upload_mapping_path(), std::ios::binary | std::ios::trunc);
        legacy << "# raythm upload mappings v1\n"
               << "[songs]\n"
               << "https://server.example\tlegacy-local-song\tlegacy-remote-song\n"
               << "[charts]\n"
               << "https://server.example\tlegacy-local-chart\tlegacy-local-song\tlegacy-remote-chart\tlegacy-remote-song\n";
    }

    const title_upload_mapping::store legacy_loaded = title_upload_mapping::load();
    if (title_upload_mapping::find_song_origin(legacy_loaded, "https://server.example", "legacy-local-song") !=
        std::optional<title_upload_mapping::mapping_origin>(title_upload_mapping::mapping_origin::owned_upload)) {
        std::cerr << "legacy song mapping should migrate as owned upload\n";
        return 1;
    }
    if (title_upload_mapping::find_chart_origin(legacy_loaded, "https://server.example", "legacy-local-chart") !=
        std::optional<title_upload_mapping::mapping_origin>(title_upload_mapping::mapping_origin::owned_upload)) {
        std::cerr << "legacy chart mapping should migrate as owned upload\n";
        return 1;
    }

    local_content_index::put_song_binding({
        .server_url = "https://server.example",
        .local_song_id = "facade-local-song",
        .remote_song_id = "facade-remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_song_binding({
        .server_url = "https://mirror.example",
        .local_song_id = "facade-local-song",
        .remote_song_id = "mirror-remote-song",
        .origin = local_content_index::online_origin::linked,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "facade-local-chart",
        .local_song_id = "facade-local-song",
        .remote_chart_id = "facade-remote-chart",
        .remote_song_id = "facade-remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://mirror.example",
        .local_chart_id = "facade-local-chart",
        .local_song_id = "facade-local-song",
        .remote_chart_id = "mirror-remote-chart",
        .remote_song_id = "mirror-remote-song",
        .origin = local_content_index::online_origin::linked,
    });
    local_content_index::link_chart_to_song("facade-local-chart", "facade-local-song");

    const std::optional<local_content_index::online_song_binding> facade_song =
        local_content_index::find_song_by_local("https://server.example", "facade-local-song");
    if (!facade_song.has_value() || facade_song->remote_song_id != "facade-remote-song" ||
        facade_song->origin != local_content_index::online_origin::downloaded) {
        std::cerr << "facade song mapping missing\n";
        return 1;
    }

    const std::optional<local_content_index::online_chart_binding> facade_chart =
        local_content_index::find_chart_by_remote("https://server.example", "facade-remote-chart");
    if (!facade_chart.has_value() || facade_chart->local_chart_id != "facade-local-chart" ||
        facade_chart->remote_song_id != "facade-remote-song") {
        std::cerr << "facade chart mapping missing\n";
        return 1;
    }

    if (local_content_index::linked_song_for_chart("facade-local-chart") !=
        std::optional<std::string>("facade-local-song")) {
        std::cerr << "facade chart identity missing\n";
        return 1;
    }

    local_content_index::remove_chart_bindings("facade-local-chart");
    if (local_content_index::find_chart_by_local("https://server.example", "facade-local-chart").has_value() ||
        local_content_index::find_chart_by_local("https://mirror.example", "facade-local-chart").has_value()) {
        std::cerr << "facade chart mapping should be removed from all servers\n";
        return 1;
    }

    local_content_index::remove_song_bindings("facade-local-song");
    if (local_content_index::find_song_by_local("https://server.example", "facade-local-song").has_value() ||
        local_content_index::find_song_by_local("https://mirror.example", "facade-local-song").has_value()) {
        std::cerr << "facade song mapping should be removed from all servers\n";
        return 1;
    }

    fs::remove_all(temp_root, ec);
    std::cout << "upload_mapping_store smoke test passed\n";
    return 0;
}
