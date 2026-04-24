#pragma once

#include <string>

#include "song_select/song_select_state.h"

namespace title_create_upload {

struct upload_result {
    bool success = false;
    bool should_refresh_online_catalog = false;
    std::string message;
};

upload_result upload_song(const song_select::song_entry& song);
upload_result upload_chart(const song_select::song_entry& song,
                           const song_select::chart_option& chart);

}  // namespace title_create_upload
