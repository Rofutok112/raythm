#pragma once

#include <string>

namespace song_select {

struct last_played_selection {
    std::string song_id;
    std::string chart_id;
};

last_played_selection load_last_played_selection();
bool save_last_played_selection(const std::string& song_id, const std::string& chart_id);

}  // namespace song_select
