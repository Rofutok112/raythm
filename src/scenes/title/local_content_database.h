#pragma once

#include <optional>
#include <string>

#include "title/local_content_binding.h"

namespace local_content_database {

local_content_binding::store load_mappings();
std::optional<local_content_binding::song_binding> find_song_by_local(const std::string& server_url,
                                                                      const std::string& local_song_id);
std::optional<local_content_binding::song_binding> find_song_by_remote(const std::string& server_url,
                                                                       const std::string& remote_song_id);
std::optional<local_content_binding::chart_binding> find_chart_by_local(const std::string& server_url,
                                                                        const std::string& local_chart_id);
std::optional<local_content_binding::chart_binding> find_chart_by_remote(const std::string& server_url,
                                                                         const std::string& remote_chart_id);

void put_song(const local_content_binding::song_binding& binding);
void put_chart(const local_content_binding::chart_binding& binding);

void remove_song(const std::string& server_url, const std::string& local_song_id);
void remove_chart(const std::string& server_url, const std::string& local_chart_id);
void remove_song_bindings(const std::string& local_song_id);
void remove_chart_bindings(const std::string& local_chart_id);

}  // namespace local_content_database
