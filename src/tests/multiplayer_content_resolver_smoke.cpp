#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <vector>

#include "title/local_content_index.h"
#include "title/multiplayer_content_resolver.h"
#include "title/multiplayer_playable_catalog.h"

namespace {

bool set_local_app_data(const std::filesystem::path& path) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", path.string().c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", path.string().c_str(), 1) == 0;
#endif
}

}  // namespace

int main() {
    const std::filesystem::path appdata_root =
        std::filesystem::temp_directory_path() / "raythm-multiplayer-content-resolver-smoke";
    std::error_code ec;
    std::filesystem::remove_all(appdata_root, ec);
    assert(set_local_app_data(appdata_root));

    local_content_index::put_song_binding({
        .server_url = "https://server.example",
        .local_song_id = "local-song",
        .remote_song_id = "remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "local-chart",
        .remote_chart_id = "remote-chart",
        .remote_song_id = "remote-song",
        .remote_chart_version = 2,
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_song_binding({
        .server_url = "https://server.example",
        .local_song_id = "pure-local-song",
        .remote_song_id = "stale-remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "pure-local-chart",
        .remote_chart_id = "stale-remote-chart",
        .remote_song_id = "stale-remote-song",
        .remote_chart_version = 1,
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "chart-only-local-chart",
        .remote_chart_id = "chart-only-remote-chart",
        .remote_song_id = "chart-only-remote-song",
        .remote_chart_version = 1,
        .origin = local_content_index::online_origin::downloaded,
    });

    std::vector<song_select::song_entry> local_songs;
    song_select::song_entry local_song;
    local_song.song.meta.song_id = "local-song";
    local_song.song.meta.title = "Local Online";
    local_song.song.meta.artist = "Local Composer";
    local_song.status = content_status::community;
    local_song.source_status = content_status::community;
    song_select::chart_option local_chart;
    local_chart.meta.chart_id = "local-chart";
    local_chart.meta.difficulty = "Local Hard";
    local_chart.meta.key_count = 4;
    local_chart.meta.level = 12.0f;
    local_chart.meta.chart_version = 3;
    local_chart.status = content_status::community;
    local_chart.source_status = content_status::community;
    local_song.charts.push_back(local_chart);
    song_select::chart_option modified_online_chart;
    modified_online_chart.meta.chart_id = "modified-online-chart";
    modified_online_chart.meta.difficulty = "Modified Online";
    modified_online_chart.meta.key_count = 4;
    modified_online_chart.status = content_status::modified;
    modified_online_chart.source_status = content_status::official;
    local_song.charts.push_back(modified_online_chart);
    local_songs.push_back(local_song);

    song_select::song_entry purely_local_song;
    purely_local_song.song.meta.song_id = "pure-local-song";
    purely_local_song.song.meta.title = "Pure Local";
    purely_local_song.song.meta.artist = "Local Artist";
    purely_local_song.status = content_status::modified;
    purely_local_song.source_status = content_status::local;
    song_select::chart_option purely_local_chart;
    purely_local_chart.meta.chart_id = "pure-local-chart";
    purely_local_chart.meta.difficulty = "Local Edit";
    purely_local_chart.meta.key_count = 4;
    purely_local_chart.status = content_status::modified;
    purely_local_chart.source_status = content_status::local;
    purely_local_song.charts.push_back(purely_local_chart);
    local_songs.push_back(purely_local_song);

    song_select::song_entry chart_only_song;
    chart_only_song.song.meta.song_id = "chart-only-local-song";
    chart_only_song.status = content_status::community;
    chart_only_song.source_status = content_status::community;
    song_select::chart_option chart_only_chart;
    chart_only_chart.meta.chart_id = "chart-only-local-chart";
    chart_only_chart.meta.difficulty = "Chart Binding";
    chart_only_chart.meta.key_count = 4;
    chart_only_chart.status = content_status::community;
    chart_only_chart.source_status = content_status::community;
    chart_only_song.charts.push_back(chart_only_chart);
    local_songs.push_back(chart_only_song);

    std::vector<multiplayer_client::online_song> songs;
    multiplayer_client::online_song remote_song;
    remote_song.song_id = "remote-song";
    multiplayer_client::online_chart remote_chart;
    remote_chart.chart_id = "remote-chart";
    remote_chart.chart_version = 3;
    remote_song.charts.push_back(remote_chart);
    songs.push_back(remote_song);

    title_multiplayer_content::annotate_online_content(songs, "https://server.example", local_songs);
    assert(songs[0].installed);
    assert(songs[0].local_song_id == "local-song");
    assert(songs[0].charts[0].installed);
    assert(songs[0].charts[0].local_chart_id == "local-chart");
    assert(songs[0].charts[0].update_available);

    const auto selected = title_multiplayer_content::selected_chart(songs, 0, 0);
    assert(selected.has_value());
    assert(selected->local_song_id == "local-song");
    assert(selected->local_chart_id == "local-chart");
    assert(selected->chart_installed);

    multiplayer_client::room_settings settings;
    settings.selected_song_id = "remote-song";
    settings.selected_chart_id = "remote-chart";
    const auto room_chart =
        title_multiplayer_content::resolve_room_chart("https://server.example", settings, local_songs);
    assert(room_chart.has_value());
    assert(room_chart->local_song_id == "local-song");
    assert(room_chart->local_chart_id == "local-chart");
    assert(room_chart->chart_installed);

    std::vector<multiplayer_client::online_song> remote_catalog;
    multiplayer_client::online_song catalog_song;
    catalog_song.song_id = "remote-song";
    catalog_song.title = "Remote Online";
    catalog_song.artist = "Remote Composer";
    catalog_song.charts.push_back({
        .chart_id = "remote-chart",
        .difficulty_name = "Remote Hard",
        .key_count = 4,
        .level = 12,
        .chart_version = 3,
    });
    catalog_song.charts.push_back({
        .chart_id = "modified-online-chart",
        .difficulty_name = "Modified Online",
        .key_count = 4,
        .level = 10,
        .chart_version = 1,
    });
    remote_catalog.push_back(catalog_song);
    multiplayer_client::online_song stale_song;
    stale_song.song_id = "stale-remote-song";
    stale_song.title = "Stale Remote";
    stale_song.charts.push_back({
        .chart_id = "stale-remote-chart",
        .difficulty_name = "Stale",
        .key_count = 4,
        .level = 9,
        .chart_version = 1,
    });
    remote_catalog.push_back(stale_song);
    multiplayer_client::online_song chart_only_remote_song;
    chart_only_remote_song.song_id = "chart-only-remote-song";
    chart_only_remote_song.title = "Chart Binding Only";
    chart_only_remote_song.charts.push_back({
        .chart_id = "chart-only-remote-chart",
        .difficulty_name = "Chart Binding",
        .key_count = 4,
        .level = 8,
        .chart_version = 1,
    });
    remote_catalog.push_back(chart_only_remote_song);

    std::vector<multiplayer_client::online_song> local_only =
        title_multiplayer_content::select_playable_online_content(
            remote_catalog,
            "https://server.example",
            local_songs);
    assert(local_only.size() == 2);
    assert(local_only[0].song_id == "remote-song");
    assert(local_only[0].local_song_id == "local-song");
    assert(local_only[0].installed);
    assert(local_only[0].charts.size() == 1);
    assert(local_only[0].charts[0].chart_id == "remote-chart");
    assert(local_only[0].charts[0].local_chart_id == "local-chart");
    assert(local_only[0].charts[0].installed);
    assert(local_only[1].song_id == "chart-only-remote-song");
    assert(local_only[1].local_song_id == "chart-only-local-song");
    assert(local_only[1].charts.size() == 1);
    assert(local_only[1].charts[0].chart_id == "chart-only-remote-chart");
    assert(local_only[1].charts[0].local_chart_id == "chart-only-local-chart");

    title_multiplayer_content::multiplayer_playable_catalog playable_catalog;
    playable_catalog.replace(local_only, "https://server.example", local_songs);
    assert(playable_catalog.size() == 2);
    assert(playable_catalog.selected_chart_installed(0, 0));
    const multiplayer_client::room_settings playable_settings =
        playable_catalog.selected_room_settings(0, 0);
    assert(playable_settings.selected_song_id == "remote-song");
    assert(playable_settings.selected_chart_id == "remote-chart");
    assert(playable_settings.key_count == 4);

    std::vector<multiplayer_client::online_song> mismatched_server =
        title_multiplayer_content::select_playable_online_content(
            remote_catalog,
            "https://other-server.example",
            local_songs);
    assert(mismatched_server.size() == 2);
    assert(mismatched_server[0].song_id == "remote-song");
    assert(mismatched_server[0].charts.size() == 1);
    assert(mismatched_server[0].charts[0].chart_id == "remote-chart");

    std::filesystem::remove_all(appdata_root, ec);
    return EXIT_SUCCESS;
}
