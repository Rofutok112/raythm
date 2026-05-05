#pragma once

#include <optional>
#include <string>
#include <vector>

#include "title/local_content_binding.h"

namespace local_content_index {

using online_origin = local_content_binding::origin;

struct online_song_binding {
    std::string server_url;
    std::string local_song_id;
    std::string remote_song_id;
    online_origin origin = online_origin::owned_upload;
};

struct online_chart_binding {
    std::string server_url;
    std::string local_chart_id;
    std::string remote_chart_id;
    std::string remote_song_id;
    online_origin origin = online_origin::owned_upload;
};

struct snapshot {
    std::vector<online_song_binding> songs;
    std::vector<online_chart_binding> charts;
};

snapshot load_snapshot();
std::optional<online_song_binding> find_song_by_local(const snapshot& index,
                                                      const std::string& server_url,
                                                      const std::string& local_song_id);
std::optional<online_song_binding> find_song_by_remote(const snapshot& index,
                                                       const std::string& server_url,
                                                       const std::string& remote_song_id);
std::optional<online_chart_binding> find_chart_by_local(const snapshot& index,
                                                        const std::string& server_url,
                                                        const std::string& local_chart_id);
std::optional<online_chart_binding> find_chart_by_remote(const snapshot& index,
                                                         const std::string& server_url,
                                                         const std::string& remote_chart_id);
std::optional<online_song_binding> find_song_by_local(const std::string& server_url,
                                                      const std::string& local_song_id);
std::optional<online_song_binding> find_song_by_remote(const std::string& server_url,
                                                       const std::string& remote_song_id);
std::optional<online_chart_binding> find_chart_by_local(const std::string& server_url,
                                                        const std::string& local_chart_id);
std::optional<online_chart_binding> find_chart_by_remote(const std::string& server_url,
                                                         const std::string& remote_chart_id);

void put_song_binding(const online_song_binding& binding);
void put_chart_binding(const online_chart_binding& binding);
void remove_song_binding(const std::string& server_url, const std::string& local_song_id);
void remove_chart_binding(const std::string& server_url, const std::string& local_chart_id);
void remove_song_bindings(const std::string& local_song_id);
void remove_chart_bindings(const std::string& local_chart_id);

}  // namespace local_content_index
