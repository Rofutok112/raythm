#include "title/multiplayer_content_resolver.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "title/online_download_remote_client.h"

namespace title_multiplayer_content {

online_content_result load_online_content(const std::string& server_url,
                                          const std::vector<song_select::song_entry>& local_songs) {
    online_content_result result;
    const title_online_view::remote_catalog_fetch_result catalog =
        title_online_view::fetch_remote_catalog(server_url);
    result.success = catalog.success;
    result.maintenance = catalog.maintenance;
    result.message = catalog.error_message;
    result.retry_after = catalog.retry_after;
    result.server_url = catalog.server_url.empty() ? server_url : catalog.server_url;
    if (!catalog.success) {
        return result;
    }

    std::vector<multiplayer_client::online_song> remote_songs;
    remote_songs.reserve(catalog.songs.size());
    for (const title_online_view::remote_song_payload& remote_song : catalog.songs) {
        multiplayer_client::online_song song;
        song.song_id = remote_song.id;
        song.title = remote_song.title;
        song.artist = remote_song.artist;
        remote_songs.push_back(std::move(song));
    }

    for (const title_online_view::remote_chart_payload& remote_chart : catalog.charts) {
        if (remote_chart.song_id.empty()) {
            continue;
        }

        auto song_it = std::find_if(remote_songs.begin(), remote_songs.end(), [&](const auto& song) {
            return song.song_id == remote_chart.song_id;
        });
        if (song_it == remote_songs.end()) {
            multiplayer_client::online_song song;
            song.song_id = remote_chart.song_id;
            remote_songs.push_back(std::move(song));
            song_it = std::prev(remote_songs.end());
        }

        song_it->charts.push_back({
            .chart_id = remote_chart.id,
            .difficulty_name = remote_chart.difficulty_name,
            .key_count = remote_chart.key_count,
            .level = static_cast<int>(remote_chart.level),
            .chart_version = remote_chart.chart_version,
        });
    }

    result.songs = select_playable_online_content(remote_songs, result.server_url, local_songs);
    result.message = result.songs.empty()
        ? "No downloaded online charts are playable in multiplayer."
        : "Online content loaded.";
    return result;
}

}  // namespace title_multiplayer_content
