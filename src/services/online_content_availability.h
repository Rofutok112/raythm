#pragma once

#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "scenes/song_select/song_select_state.h"
#include "title/local_content_index.h"

namespace online_content_availability {

struct song_ref {
    std::string server_url;
    std::string remote_song_id;
    int remote_song_version = 0;
};

struct chart_ref {
    std::string server_url;
    std::string remote_song_id;
    std::string remote_chart_id;
    int remote_chart_version = 0;
};

struct resolved_song {
    const song_select::song_entry* local_song = nullptr;
    std::string local_song_id;
    bool installed = false;
    bool update_available = false;
    bool identity_matched = false;
    bool binding_matched = false;
    content_status display_status = content_status::local;
};

struct resolved_chart {
    const song_select::chart_option* local_chart = nullptr;
    std::string local_chart_id;
    bool installed = false;
    bool update_available = false;
    bool identity_matched = false;
    bool binding_matched = false;
    content_status display_status = content_status::local;
};

resolved_song resolve_song(const std::vector<song_select::song_entry>& local_songs,
                           const local_content_index::snapshot& index,
                           const song_ref& remote,
                           content_status remote_source_status);

resolved_chart resolve_chart(const std::vector<song_select::song_entry>& local_songs,
                             const local_content_index::snapshot& index,
                             const resolved_song& resolved_song,
                             const chart_ref& remote,
                             content_status remote_source_status);

}  // namespace online_content_availability
