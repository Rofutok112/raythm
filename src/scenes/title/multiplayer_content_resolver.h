#pragma once

#include <optional>
#include <string>
#include <vector>

#include "network/multiplayer_client.h"
#include "song_select/song_select_state.h"

namespace title_multiplayer_content {

struct resolved_chart {
    std::string remote_song_id;
    std::string remote_chart_id;
    std::string local_song_id;
    std::string local_chart_id;
    bool song_installed = false;
    bool chart_installed = false;
    bool update_available = false;
};

struct online_content_result {
    bool success = false;
    bool maintenance = false;
    std::string message;
    std::string retry_after;
    std::string server_url;
    std::vector<multiplayer_client::online_song> songs;
};

online_content_result load_online_content(const std::string& server_url,
                                          const std::vector<song_select::song_entry>& local_songs);
std::vector<multiplayer_client::online_song> select_playable_online_content(
    const std::vector<multiplayer_client::online_song>& remote_songs,
    const std::string& server_url,
    const std::vector<song_select::song_entry>& local_songs);

void annotate_online_content(std::vector<multiplayer_client::online_song>& songs,
                             const std::string& server_url,
                             const std::vector<song_select::song_entry>& local_songs);

std::optional<resolved_chart> selected_chart(const std::vector<multiplayer_client::online_song>& songs,
                                             int selected_song_index,
                                             int selected_chart_index);

std::optional<resolved_chart> resolve_room_chart(const std::string& server_url,
                                                 const multiplayer_client::room_settings& settings,
                                                 const std::vector<song_select::song_entry>& local_songs);

}  // namespace title_multiplayer_content
