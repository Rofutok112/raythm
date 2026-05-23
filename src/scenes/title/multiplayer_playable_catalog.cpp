#include "title/multiplayer_playable_catalog.h"

#include <algorithm>
#include <utility>

namespace title_multiplayer_content {

void multiplayer_playable_catalog::replace(std::vector<multiplayer_client::online_song> songs,
                                           std::string server_url,
                                           const std::vector<song_select::song_entry>& local_songs) {
    server_url_ = std::move(server_url);
    songs_ = std::move(songs);
    annotate_online_content(songs_, server_url_, local_songs);
}

void multiplayer_playable_catalog::clear() {
    songs_.clear();
    server_url_.clear();
}

bool multiplayer_playable_catalog::empty() const {
    return songs_.empty();
}

size_t multiplayer_playable_catalog::size() const {
    return songs_.size();
}

const std::string& multiplayer_playable_catalog::server_url() const {
    return server_url_;
}

const std::vector<multiplayer_client::online_song>& multiplayer_playable_catalog::songs() const {
    return songs_;
}

const multiplayer_client::online_song* multiplayer_playable_catalog::song_at(int song_index) const {
    if (songs_.empty()) {
        return nullptr;
    }
    const int clamped = clamped_song_index(song_index);
    return &songs_[static_cast<size_t>(clamped)];
}

const multiplayer_client::online_chart* multiplayer_playable_catalog::chart_at(int song_index, int chart_index) const {
    const multiplayer_client::online_song* song = song_at(song_index);
    if (song == nullptr || song->charts.empty()) {
        return nullptr;
    }
    const int clamped = clamped_chart_index(song_index, chart_index);
    return &song->charts[static_cast<size_t>(clamped)];
}

int multiplayer_playable_catalog::clamped_song_index(int song_index) const {
    if (songs_.empty()) {
        return 0;
    }
    return std::clamp(song_index, 0, static_cast<int>(songs_.size()) - 1);
}

int multiplayer_playable_catalog::clamped_chart_index(int song_index, int chart_index) const {
    const multiplayer_client::online_song* song = song_at(song_index);
    if (song == nullptr || song->charts.empty()) {
        return 0;
    }
    return std::clamp(chart_index, 0, static_cast<int>(song->charts.size()) - 1);
}

multiplayer_client::room_settings multiplayer_playable_catalog::selected_room_settings(int song_index,
                                                                                       int chart_index) const {
    multiplayer_client::room_settings settings;
    const multiplayer_client::online_song* song = song_at(song_index);
    if (song == nullptr) {
        return settings;
    }

    settings.selected_song_id = song->song_id;
    const multiplayer_client::online_chart* chart = chart_at(song_index, chart_index);
    if (chart != nullptr) {
        settings.selected_chart_id = chart->chart_id;
        settings.key_count = chart->key_count;
    }
    return settings;
}

std::optional<resolved_chart> multiplayer_playable_catalog::selected_chart(int song_index, int chart_index) const {
    return title_multiplayer_content::selected_chart(songs_, song_index, chart_index);
}

bool multiplayer_playable_catalog::selected_chart_installed(int song_index, int chart_index) const {
    const std::optional<resolved_chart> chart = selected_chart(song_index, chart_index);
    return chart.has_value() && chart->song_installed && chart->chart_installed;
}

}  // namespace title_multiplayer_content
