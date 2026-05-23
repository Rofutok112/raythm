#pragma once

#include <optional>
#include <string>
#include <vector>

#include "network/multiplayer_client.h"
#include "song_select/song_select_state.h"
#include "title/multiplayer_content_resolver.h"

namespace title_multiplayer_content {

class multiplayer_playable_catalog {
public:
    void replace(std::vector<multiplayer_client::online_song> songs,
                 std::string server_url,
                 const std::vector<song_select::song_entry>& local_songs);
    void clear();

    [[nodiscard]] bool empty() const;
    [[nodiscard]] size_t size() const;
    [[nodiscard]] const std::string& server_url() const;
    [[nodiscard]] const std::vector<multiplayer_client::online_song>& songs() const;
    [[nodiscard]] const multiplayer_client::online_song* song_at(int song_index) const;
    [[nodiscard]] const multiplayer_client::online_chart* chart_at(int song_index, int chart_index) const;

    [[nodiscard]] int clamped_song_index(int song_index) const;
    [[nodiscard]] int clamped_chart_index(int song_index, int chart_index) const;
    [[nodiscard]] multiplayer_client::room_settings selected_room_settings(int song_index, int chart_index) const;
    [[nodiscard]] std::optional<resolved_chart> selected_chart(int song_index, int chart_index) const;
    [[nodiscard]] bool selected_chart_installed(int song_index, int chart_index) const;

private:
    std::vector<multiplayer_client::online_song> songs_;
    std::string server_url_;
};

}  // namespace title_multiplayer_content
