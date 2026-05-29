#include "services/online_content_availability.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {

song_select::song_entry make_song(std::string song_id) {
    song_select::song_entry song;
    song.song.meta.song_id = std::move(song_id);
    song.song.meta.song_version = 1;
    return song;
}

song_select::chart_option make_chart(std::string song_id, std::string chart_id) {
    song_select::chart_option chart;
    chart.meta.song_id = std::move(song_id);
    chart.meta.chart_id = std::move(chart_id);
    chart.meta.chart_version = 1;
    return chart;
}

}  // namespace

int main() {
    constexpr const char* kServer = "https://server.example";

    std::vector<song_select::song_entry> local_songs;
    local_songs.push_back(make_song("remote-song"));
    local_songs.back().charts.push_back(make_chart("remote-song", "remote-chart"));

    local_content_index::snapshot empty_index;
    const online_content_availability::song_ref remote_song{
        .server_url = kServer,
        .remote_song_id = "remote-song",
        .remote_song_version = 2,
    };
    const online_content_availability::chart_ref remote_chart{
        .server_url = kServer,
        .remote_song_id = "remote-song",
        .remote_chart_id = "remote-chart",
        .remote_chart_version = 2,
    };

    const auto id_collision_song = online_content_availability::resolve_song(
        local_songs, empty_index, remote_song, content_status::community);
    assert(!id_collision_song.installed);

    local_songs.front().online_identity = online_content::song_identity{
        .server_url = kServer,
        .remote_song_id = "remote-song",
        .content_source = online_content::source::community,
    };
    local_songs.front().charts.front().online_identity = online_content::chart_identity{
        .server_url = kServer,
        .remote_song_id = "remote-song",
        .remote_chart_id = "remote-chart",
        .content_source = online_content::source::community,
        .remote_chart_version = 1,
    };
    local_songs.front().managed_manifest = song_select::managed_song_manifest_metadata{
        .song_json_hash = "local-rewritten-song-json-sha",
        .song_json_fingerprint = "song-fp-a",
        .audio_hash = "audio-a",
        .jacket_hash = "jacket-a",
        .remote_song_json_hash = "remote-song-json-sha",
        .remote_song_json_fingerprint = "song-fp-a",
        .remote_audio_hash = "audio-a",
        .remote_jacket_hash = "jacket-a",
    };
    local_songs.front().charts.front().managed_manifest = song_select::managed_chart_manifest_metadata{
        .chart_hash = "local-rewritten-chart-sha",
        .chart_fingerprint = "chart-fp-a",
        .remote_chart_hash = "remote-chart-sha",
        .remote_chart_fingerprint = "chart-fp-a",
    };

    const auto identity_song = online_content_availability::resolve_song(
        local_songs, empty_index, remote_song, content_status::community);
    assert(identity_song.installed);
    assert(identity_song.identity_matched);
    assert(identity_song.update_available);

    const auto identity_chart = online_content_availability::resolve_chart(
        local_songs, empty_index, identity_song, remote_chart, content_status::community);
    assert(identity_chart.installed);
    assert(identity_chart.identity_matched);
    assert(identity_chart.update_available);

    const auto same_hash_song = online_content_availability::resolve_song(
        local_songs,
        empty_index,
        {
            .server_url = kServer,
            .remote_song_id = "remote-song",
            .remote_song_version = 1,
            .song_json_fingerprint = "song-fp-a",
            .audio_hash = "audio-a",
            .jacket_hash = "jacket-a",
        },
        content_status::community);
    assert(same_hash_song.installed);
    assert(!same_hash_song.update_available);

    const auto same_raw_song = online_content_availability::resolve_song(
        local_songs,
        empty_index,
        {
            .server_url = kServer,
            .remote_song_id = "remote-song",
            .remote_song_version = 1,
            .song_json_hash = "remote-song-json-sha",
            .audio_hash = "audio-a",
            .jacket_hash = "jacket-a",
        },
        content_status::community);
    assert(same_raw_song.installed);
    assert(!same_raw_song.update_available);

    const auto changed_hash_song = online_content_availability::resolve_song(
        local_songs,
        empty_index,
        {
            .server_url = kServer,
            .remote_song_id = "remote-song",
            .remote_song_version = 1,
            .song_json_fingerprint = "song-fp-b",
            .audio_hash = "audio-a",
            .jacket_hash = "jacket-a",
        },
        content_status::community);
    assert(changed_hash_song.installed);
    assert(changed_hash_song.update_available);

    const auto same_raw_chart = online_content_availability::resolve_chart(
        local_songs,
        empty_index,
        same_hash_song,
        {
            .server_url = kServer,
            .remote_song_id = "remote-song",
            .remote_chart_id = "remote-chart",
            .remote_chart_version = 1,
            .chart_hash = "remote-chart-sha",
        },
        content_status::community);
    assert(same_raw_chart.installed);
    assert(!same_raw_chart.update_available);

    const auto changed_hash_chart = online_content_availability::resolve_chart(
        local_songs,
        empty_index,
        same_hash_song,
        {
            .server_url = kServer,
            .remote_song_id = "remote-song",
            .remote_chart_id = "remote-chart",
            .remote_chart_version = 1,
            .chart_fingerprint = "chart-fp-b",
        },
        content_status::community);
    assert(changed_hash_chart.installed);
    assert(changed_hash_chart.update_available);

    local_songs.clear();
    local_songs.push_back(make_song("local-song"));
    local_songs.back().charts.push_back(make_chart("local-song", "local-chart"));
    local_content_index::snapshot binding_index;
    binding_index.songs.push_back({
        .server_url = kServer,
        .local_song_id = "local-song",
        .remote_song_id = "remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    binding_index.charts.push_back({
        .server_url = kServer,
        .local_chart_id = "local-chart",
        .remote_chart_id = "remote-chart",
        .remote_song_id = "remote-song",
        .remote_chart_version = 1,
        .origin = local_content_index::online_origin::downloaded,
    });

    const auto binding_song = online_content_availability::resolve_song(
        local_songs, binding_index, remote_song, content_status::community);
    assert(!binding_song.installed);
    const auto binding_chart = online_content_availability::resolve_chart(
        local_songs, binding_index, binding_song, remote_chart, content_status::community);
    assert(!binding_chart.installed);

    local_songs.back().storage = storage_policy::managed_package;
    local_songs.back().status = content_status::community;
    local_songs.back().source_status = content_status::community;
    local_songs.back().charts.back().storage = storage_policy::managed_package;
    local_songs.back().charts.back().status = content_status::community;
    local_songs.back().charts.back().source_status = content_status::community;

    const auto managed_binding_song = online_content_availability::resolve_song(
        local_songs, binding_index, remote_song, content_status::community);
    assert(managed_binding_song.installed);
    assert(managed_binding_song.binding_matched);
    const auto managed_binding_chart = online_content_availability::resolve_chart(
        local_songs, binding_index, managed_binding_song, remote_chart, content_status::community);
    assert(managed_binding_chart.installed);
    assert(managed_binding_chart.binding_matched);

    local_songs.clear();
    const auto stale_only_song = online_content_availability::resolve_song(
        local_songs, binding_index, remote_song, content_status::community);
    assert(!stale_only_song.installed);

    return 0;
}
