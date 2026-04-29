#pragma once

#include <optional>
#include <string>

#include "title/upload_mapping_store.h"

namespace local_content_database {

title_upload_mapping::store load_mappings();
std::optional<title_upload_mapping::song_mapping_entry> find_song_by_local(const std::string& server_url,
                                                                           const std::string& local_song_id);
std::optional<title_upload_mapping::song_mapping_entry> find_song_by_remote(const std::string& server_url,
                                                                            const std::string& remote_song_id);
std::optional<title_upload_mapping::chart_mapping_entry> find_chart_by_local(const std::string& server_url,
                                                                             const std::string& local_chart_id);
std::optional<title_upload_mapping::chart_mapping_entry> find_chart_by_remote(const std::string& server_url,
                                                                              const std::string& remote_chart_id);

void put_song(const title_upload_mapping::song_mapping_entry& binding);
void put_chart(const title_upload_mapping::chart_mapping_entry& binding);

void remove_song(const std::string& server_url, const std::string& local_song_id);
void remove_chart(const std::string& server_url, const std::string& local_chart_id);
void remove_song_bindings(const std::string& local_song_id);
void remove_chart_bindings(const std::string& local_chart_id);

}  // namespace local_content_database
