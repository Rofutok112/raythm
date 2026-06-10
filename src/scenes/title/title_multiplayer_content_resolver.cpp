#include "title/title_multiplayer_content_resolver.h"

#include <optional>

#include "network/server_environment.h"
#include "services/online_content_availability.h"
#include "song_select/song_select_navigation.h"

namespace title {

local_chart_match find_online_chart_match(const song_select::state& state,
                                          const local_content_index::snapshot& index,
                                          const std::string& server_url,
                                          const std::string& remote_song_id,
                                          const std::string& remote_chart_id,
                                          int remote_chart_version) {
    const online_content_availability::resolved_song song =
        online_content_availability::resolve_song(
            state.songs,
            index,
            {
                .server_url = server_environment::normalize_url(server_url),
                .remote_song_id = remote_song_id,
            },
            content_status::community);
    const online_content_availability::resolved_chart chart =
        online_content_availability::resolve_chart(
            state.songs,
            index,
            song,
            {
                .server_url = server_environment::normalize_url(server_url),
                .remote_song_id = remote_song_id,
                .remote_chart_id = remote_chart_id,
                .remote_chart_version = remote_chart_version,
            },
            content_status::community);
    return {song.local_song, chart.local_chart};
}

void prepare_multiplayer_queue_state(multiplayer::state& multiplayer_state,
                                     const song_select::state& play_state,
                                     const local_content_index::snapshot& multiplayer_local_index,
                                     bool& queue_selected_chart_on_multiplayer_return) {
    const song_select::song_entry* song = song_select::selected_song(play_state);
    const auto filtered = song_select::filtered_charts_for_selected_song(play_state);
    const song_select::chart_option* chart = song_select::selected_chart_for(play_state, filtered);
    const std::string room_server_url = server_environment::normalize_url(multiplayer_state.auth.server_url);
    std::optional<online_content::chart_identity> queue_identity;
    const bool online_chart_routes = chart != nullptr && song_select::can_use_online_chart_routes(*chart);
    if (online_chart_routes &&
        online_content::is_queueable(chart->online_identity) &&
        server_environment::normalize_url(chart->online_identity->server_url) == room_server_url) {
        queue_identity = chart->online_identity;
    }
    if (!queue_identity.has_value() && online_chart_routes) {
        for (const online_content::chart_identity& link : chart->remote_links) {
            if (online_content::is_queueable(link) &&
                server_environment::normalize_url(link.server_url) == room_server_url) {
                queue_identity = link;
                break;
            }
        }
    }

    multiplayer_state.queue_candidate_available = queue_identity.has_value();
    if (song != nullptr && chart != nullptr) {
        multiplayer_state.queue_candidate_song_title = song->song.meta.title;
        multiplayer_state.queue_candidate_chart_name = chart->meta.difficulty;
        if (queue_identity.has_value()) {
            multiplayer_state.queue_candidate_remote_song_id = queue_identity->remote_song_id;
            multiplayer_state.queue_candidate_remote_chart_id = queue_identity->remote_chart_id;
            multiplayer_state.queue_candidate_remote_chart_version = queue_identity->remote_chart_version;
            multiplayer_state.queue_candidate_message = "Selected chart can be queued.";
        } else if (online_chart_routes &&
                   (chart->online_identity.has_value() || !chart->remote_links.empty())) {
            multiplayer_state.queue_candidate_remote_song_id.clear();
            multiplayer_state.queue_candidate_remote_chart_id.clear();
            multiplayer_state.queue_candidate_remote_chart_version = 0;
            multiplayer_state.queue_candidate_message = "Selected chart belongs to another server.";
        } else {
            multiplayer_state.queue_candidate_remote_song_id.clear();
            multiplayer_state.queue_candidate_remote_chart_id.clear();
            multiplayer_state.queue_candidate_remote_chart_version = 0;
            multiplayer_state.queue_candidate_message = "Selected chart is local-only.";
        }
    } else {
        multiplayer_state.queue_candidate_song_title.clear();
        multiplayer_state.queue_candidate_chart_name.clear();
        multiplayer_state.queue_candidate_remote_song_id.clear();
        multiplayer_state.queue_candidate_remote_chart_id.clear();
        multiplayer_state.queue_candidate_remote_chart_version = 0;
        multiplayer_state.queue_candidate_message = "Select an online chart from Play.";
    }

    multiplayer_state.current_queue_chart_installed = false;
    multiplayer_state.installed_queue_item_ids.clear();
    if (multiplayer_state.current_room.has_value()) {
        for (const multiplayer::room_queue_item& item : multiplayer_state.current_room->queue) {
            const local_chart_match match =
                find_online_chart_match(
                    play_state, multiplayer_local_index, room_server_url, item.song_id, item.chart_id);
            const bool installed = match.song != nullptr && match.chart != nullptr;
            if (installed) {
                multiplayer_state.installed_queue_item_ids.push_back(item.id);
            }
            if (&item == &multiplayer_state.current_room->queue.front()) {
                multiplayer_state.current_queue_chart_installed = installed;
            }
        }
    }

    if (queue_selected_chart_on_multiplayer_return && multiplayer_state.current_room.has_value()) {
        queue_selected_chart_on_multiplayer_return = false;
        if (multiplayer_state.queue_candidate_available) {
            multiplayer_state.status_message = "Adding selected chart...";
            multiplayer_state.command = multiplayer::ui_command::add_selected_chart;
        } else {
            multiplayer_state.status_message = multiplayer_state.queue_candidate_message;
        }
    }
}

}  // namespace title
