#pragma once

#include <optional>
#include <string>

namespace content_authorization_service {

enum class content_type {
    song,
    chart,
};

struct permission_key {
    std::string server_url;
    content_type type = content_type::song;
    std::string remote_id;
    std::string user_id;
};

struct permission_entry {
    permission_key key;
    std::optional<bool> can_edit;
    long long fetched_at_unix_seconds = 0;
};

enum class fresh_permission_result {
    allowed,
    denied,
    unavailable,
};

bool same_permission_subject(const permission_key& left, const permission_key& right);
bool can_use_cached_permission(const permission_entry& entry,
                               const permission_key& current_user_subject,
                               long long current_unix_seconds);
long long permission_ttl_seconds(const std::optional<bool>& can_edit);
fresh_permission_result classify_fresh_permission(bool request_success,
                                                  bool can_edit,
                                                  bool server_denied);

}  // namespace content_authorization_service
