#pragma once

#include <string>

#include "ranking_service.h"

namespace song_select {

struct state;

struct selection_key {
    std::string song_id;
    std::string chart_id;
    ranking_service::source source = ranking_service::source::local;

    [[nodiscard]] bool operator==(const selection_key& other) const {
        return song_id == other.song_id &&
            chart_id == other.chart_id &&
            source == other.source;
    }

    [[nodiscard]] bool operator!=(const selection_key& other) const {
        return !(*this == other);
    }
};

inline selection_key song_media_key_for(const selection_key& key) {
    return selection_key{key.song_id, "", ranking_service::source::local};
}

inline selection_key song_media_key_for_song_id(const std::string& song_id) {
    return selection_key{song_id, "", ranking_service::source::local};
}

selection_key selection_key_for_state(const state& state);

}  // namespace song_select
