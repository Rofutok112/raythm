#include "title/multiplayer_content_resolver.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "title/local_content_index.h"

namespace title_multiplayer_content {
namespace {

const song_select::song_entry* find_local_song(const std::vector<song_select::song_entry>& local_songs,
                                               const std::string& song_id) {
    if (song_id.empty()) {
        return nullptr;
    }
    for (const song_select::song_entry& song : local_songs) {
        if (song.song.meta.song_id == song_id) {
            return &song;
        }
    }
    return nullptr;
}

const song_select::chart_option* find_local_chart(const song_select::song_entry& song,
                                                  const std::string& chart_id) {
    if (chart_id.empty()) {
        return nullptr;
    }
    for (const song_select::chart_option& chart : song.charts) {
        if (chart.meta.chart_id == chart_id) {
            return &chart;
        }
    }
    return nullptr;
}

bool is_multiplayer_eligible_status(content_status status) {
    return status == content_status::official ||
           status == content_status::community ||
           status == content_status::update;
}

resolved_chart resolve_chart(const std::string& server_url,
                             const std::string& remote_song_id,
                             const std::string& remote_chart_id,
                             int remote_chart_version,
                             const std::vector<song_select::song_entry>& local_songs,
                             const local_content_index::snapshot& index) {
    resolved_chart result;
    result.remote_song_id = remote_song_id;
    result.remote_chart_id = remote_chart_id;
    result.local_song_id = remote_song_id;
    result.local_chart_id = remote_chart_id;

    if (!server_url.empty()) {
        if (const auto song_binding =
                local_content_index::find_song_by_remote(index, server_url, remote_song_id)) {
            result.local_song_id = song_binding->local_song_id;
        }
        if (const auto chart_binding =
                local_content_index::find_chart_by_remote(index, server_url, remote_chart_id)) {
            result.local_chart_id = chart_binding->local_chart_id;
            if (!chart_binding->remote_song_id.empty() && result.local_song_id == remote_song_id) {
                if (const auto song_binding =
                        local_content_index::find_song_by_remote(index, server_url, chart_binding->remote_song_id)) {
                    result.local_song_id = song_binding->local_song_id;
                }
            }
            result.update_available =
                remote_chart_version > chart_binding->remote_chart_version &&
                chart_binding->remote_chart_version > 0;
        }
    }

    const song_select::song_entry* local_song = find_local_song(local_songs, result.local_song_id);
    if (local_song == nullptr && result.local_song_id != remote_song_id) {
        local_song = find_local_song(local_songs, remote_song_id);
    }
    result.song_installed = local_song != nullptr;

    const song_select::chart_option* local_chart = local_song != nullptr
        ? find_local_chart(*local_song, result.local_chart_id)
        : nullptr;
    if (local_chart == nullptr && local_song != nullptr && result.local_chart_id != remote_chart_id) {
        local_chart = find_local_chart(*local_song, remote_chart_id);
    }
    result.chart_installed =
        local_chart != nullptr && is_multiplayer_eligible_status(local_chart->status);
    result.update_available = result.chart_installed && result.update_available;

    return result;
}

std::optional<local_content_index::online_song_binding> find_song_binding_by_local(
    const local_content_index::snapshot& index,
    const std::string& server_url,
    const std::string& local_song_id) {
    if (!server_url.empty()) {
        if (const auto binding = local_content_index::find_song_by_local(index, server_url, local_song_id)) {
            return binding;
        }
    }
    for (const local_content_index::online_song_binding& binding : index.songs) {
        if (binding.local_song_id == local_song_id) {
            return binding;
        }
    }
    return std::nullopt;
}

std::optional<local_content_index::online_chart_binding> find_chart_binding_by_local(
    const local_content_index::snapshot& index,
    const std::string& server_url,
    const std::string& local_chart_id) {
    if (!server_url.empty()) {
        if (const auto binding = local_content_index::find_chart_by_local(index, server_url, local_chart_id)) {
            return binding;
        }
    }
    for (const local_content_index::online_chart_binding& binding : index.charts) {
        if (binding.local_chart_id == local_chart_id) {
            return binding;
        }
    }
    return std::nullopt;
}

const multiplayer_client::online_chart* find_remote_chart(
    const multiplayer_client::online_song& remote_song,
    const std::string& remote_chart_id) {
    const auto chart = std::find_if(remote_song.charts.begin(), remote_song.charts.end(), [&](const auto& item) {
        return item.chart_id == remote_chart_id;
    });
    return chart == remote_song.charts.end() ? nullptr : &*chart;
}

}  // namespace

std::vector<multiplayer_client::online_song> select_playable_online_content(
    const std::vector<multiplayer_client::online_song>& remote_songs,
    const std::string& server_url,
    const std::vector<song_select::song_entry>& local_songs) {
    std::vector<multiplayer_client::online_song> selected;
    const local_content_index::snapshot index = local_content_index::load_snapshot();

    for (const multiplayer_client::online_song& remote_song : remote_songs) {
        multiplayer_client::online_song playable_song = remote_song;
        playable_song.local_song_id.clear();
        playable_song.installed = false;
        playable_song.update_available = false;
        playable_song.charts.clear();

        for (const song_select::song_entry& local_song : local_songs) {
            const auto song_binding =
                find_song_binding_by_local(index, server_url, local_song.song.meta.song_id);

            for (const song_select::chart_option& local_chart : local_song.charts) {
                if (!is_multiplayer_eligible_status(local_chart.status)) {
                    continue;
                }
                const auto chart_binding =
                    find_chart_binding_by_local(index, server_url, local_chart.meta.chart_id);
                const std::string bound_remote_chart_id =
                    chart_binding.has_value() && !chart_binding->remote_chart_id.empty()
                        ? chart_binding->remote_chart_id
                        : local_chart.meta.chart_id;
                const std::string bound_remote_song_id =
                    chart_binding.has_value() && !chart_binding->remote_song_id.empty()
                        ? chart_binding->remote_song_id
                        : song_binding.has_value() && !song_binding->remote_song_id.empty()
                            ? song_binding->remote_song_id
                            : local_song.song.meta.song_id;
                if (bound_remote_song_id != remote_song.song_id) {
                    continue;
                }
                const multiplayer_client::online_chart* remote_chart =
                    find_remote_chart(remote_song, bound_remote_chart_id);
                if (remote_chart == nullptr) {
                    continue;
                }

                multiplayer_client::online_chart playable_chart = *remote_chart;
                playable_chart.local_chart_id = local_chart.meta.chart_id;
                playable_chart.installed = true;
                playable_chart.update_available =
                    chart_binding.has_value() &&
                    chart_binding->remote_chart_version > 0 &&
                    remote_chart->chart_version > chart_binding->remote_chart_version;
                playable_song.local_song_id = local_song.song.meta.song_id;
                playable_song.installed = true;
                playable_song.update_available =
                    playable_song.update_available || playable_chart.update_available;
                playable_song.charts.push_back(std::move(playable_chart));
            }
        }

        if (!playable_song.charts.empty()) {
            selected.push_back(std::move(playable_song));
        }
    }

    return selected;
}

void annotate_online_content(std::vector<multiplayer_client::online_song>& songs,
                             const std::string& server_url,
                             const std::vector<song_select::song_entry>& local_songs) {
    if (server_url.empty()) {
        return;
    }

    const local_content_index::snapshot index = local_content_index::load_snapshot();
    for (multiplayer_client::online_song& song : songs) {
        bool song_installed = false;
        bool any_chart_update = false;

        for (multiplayer_client::online_chart& chart : song.charts) {
            const resolved_chart resolved =
                resolve_chart(server_url, song.song_id, chart.chart_id, chart.chart_version, local_songs, index);
            song.local_song_id = resolved.local_song_id;
            chart.local_chart_id = resolved.local_chart_id;
            chart.installed = resolved.chart_installed;
            chart.update_available = resolved.update_available;
            song_installed = song_installed || resolved.song_installed;
            any_chart_update = any_chart_update || resolved.update_available;
        }

        if (song.charts.empty()) {
            const resolved_chart resolved =
                resolve_chart(server_url, song.song_id, "", 0, local_songs, index);
            song.local_song_id = resolved.local_song_id;
            song_installed = resolved.song_installed;
        }

        song.installed = song_installed;
        song.update_available = any_chart_update;
    }
}

std::optional<resolved_chart> selected_chart(const std::vector<multiplayer_client::online_song>& songs,
                                             int selected_song_index,
                                             int selected_chart_index) {
    if (songs.empty()) {
        return std::nullopt;
    }
    const int song_index = std::clamp(selected_song_index, 0, static_cast<int>(songs.size()) - 1);
    const multiplayer_client::online_song& song = songs[static_cast<size_t>(song_index)];
    if (song.charts.empty()) {
        return std::nullopt;
    }
    const int chart_index = std::clamp(selected_chart_index, 0, static_cast<int>(song.charts.size()) - 1);
    const multiplayer_client::online_chart& chart = song.charts[static_cast<size_t>(chart_index)];
    return resolved_chart{
        .remote_song_id = song.song_id,
        .remote_chart_id = chart.chart_id,
        .local_song_id = song.local_song_id.empty() ? song.song_id : song.local_song_id,
        .local_chart_id = chart.local_chart_id.empty() ? chart.chart_id : chart.local_chart_id,
        .song_installed = song.installed,
        .chart_installed = chart.installed,
        .update_available = chart.update_available,
    };
}

std::optional<resolved_chart> resolve_room_chart(const std::string& server_url,
                                                 const multiplayer_client::room_settings& settings,
                                                 const std::vector<song_select::song_entry>& local_songs) {
    if (settings.selected_song_id.empty() || settings.selected_chart_id.empty()) {
        return std::nullopt;
    }
    const local_content_index::snapshot index = local_content_index::load_snapshot();
    return resolve_chart(server_url,
                         settings.selected_song_id,
                         settings.selected_chart_id,
                         0,
                         local_songs,
                         index);
}

}  // namespace title_multiplayer_content
