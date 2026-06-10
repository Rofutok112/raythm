#pragma once

#include <string>

#include "ranking_service.h"

class title_audio_controller;
class title_play_data_controller;

namespace song_select {
struct state;
}

class title_selection_media_coordinator {
public:
    enum class context {
        none,
        play,
        create,
    };

    void reset();
    void sync_current(song_select::state& state,
                      title_audio_controller& audio_controller,
                      title_play_data_controller& data_controller,
                      context active_context,
                      bool force = false);
    void request_ranking_reload(song_select::state& state,
                                title_play_data_controller& data_controller);

private:
    struct selection_key {
        std::string song_id;
        std::string chart_id;

        [[nodiscard]] bool operator==(const selection_key& other) const {
            return song_id == other.song_id && chart_id == other.chart_id;
        }

        [[nodiscard]] bool operator!=(const selection_key& other) const {
            return !(*this == other);
        }
    };

    static selection_key current_selection_key(const song_select::state& state);
    static ranking_service::source ranking_source_for_selection(const song_select::state& state);

    selection_key preview_key_;
    selection_key ranking_key_;
    ranking_service::source ranking_source_ = ranking_service::source::local;
    bool preview_synced_ = false;
    bool ranking_synced_ = false;
};
