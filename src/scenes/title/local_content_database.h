#pragma once

#include <optional>
#include <string>

#include "title/local_content_binding.h"

namespace local_content_database {

enum class remote_content_type {
    song,
    chart,
};

struct remote_metadata {
    std::string server_url;
    remote_content_type type = remote_content_type::song;
    std::string remote_id;
    std::string content_source;
    std::string lifecycle_status;
    std::string review_status;
    int remote_version = 0;
    std::string revision_id;
    std::string content_hash;
};

struct account_permission {
    std::string server_url;
    remote_content_type type = remote_content_type::song;
    std::string remote_id;
    std::string user_id;
    std::optional<bool> can_edit;
    long long fetched_at_unix_seconds = 0;
};

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
void put_remote_metadata(const remote_metadata& metadata);
std::optional<remote_metadata> find_remote_metadata(remote_content_type type,
                                                    const std::string& server_url,
                                                    const std::string& remote_id);
void put_account_permission(const account_permission& permission);
std::optional<account_permission> find_account_permission(remote_content_type type,
                                                          const std::string& server_url,
                                                          const std::string& remote_id,
                                                          const std::string& user_id);

void remove_song(const std::string& server_url, const std::string& local_song_id);
void remove_chart(const std::string& server_url, const std::string& local_chart_id);
void remove_song_bindings(const std::string& local_song_id);
void remove_chart_bindings(const std::string& local_chart_id);
void remove_chart_bindings_for_remote_song(const std::string& server_url, const std::string& remote_song_id);
void prune_orphaned_bindings();

}  // namespace local_content_database
