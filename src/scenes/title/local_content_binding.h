#pragma once

#include <string>
#include <vector>

namespace local_content_binding {

enum class origin {
    owned_upload,
    downloaded,
    linked,
};

struct song_binding {
    std::string server_url;
    std::string local_song_id;
    std::string remote_song_id;
    origin origin = origin::owned_upload;
};

struct chart_binding {
    std::string server_url;
    std::string local_chart_id;
    std::string remote_chart_id;
    std::string remote_song_id;
    origin origin = origin::owned_upload;
};

struct store {
    std::vector<song_binding> songs;
    std::vector<chart_binding> charts;
};

}  // namespace local_content_binding

