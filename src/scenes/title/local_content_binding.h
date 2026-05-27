#pragma once

#include <optional>
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
    std::optional<bool> can_edit;
    std::string lifecycle_status;
};

struct chart_binding {
    std::string server_url;
    std::string local_chart_id;
    std::string remote_chart_id;
    std::string remote_song_id;
    int remote_chart_version = 0;
    origin origin = origin::owned_upload;
    std::optional<bool> can_edit;
    std::string lifecycle_status;
};

struct store {
    std::vector<song_binding> songs;
    std::vector<chart_binding> charts;
};

inline bool can_edit_remote(const std::optional<bool>& can_edit, origin binding_origin) {
    return can_edit.value_or(binding_origin == origin::owned_upload);
}

}  // namespace local_content_binding
