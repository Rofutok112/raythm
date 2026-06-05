#include "services/content_authorization_service.h"

namespace content_authorization_service {
namespace {

constexpr long long kAllowedPermissionTtlSeconds = 15 * 60;
constexpr long long kDeniedPermissionTtlSeconds = 5 * 60;

}  // namespace

bool same_permission_subject(const permission_key& left, const permission_key& right) {
    return left.server_url == right.server_url &&
           left.type == right.type &&
           left.remote_id == right.remote_id &&
           left.user_id == right.user_id &&
           !left.server_url.empty() &&
           !left.remote_id.empty() &&
           !left.user_id.empty();
}

bool can_use_cached_permission(const permission_entry& entry,
                               const permission_key& current_user_subject,
                               long long current_unix_seconds) {
    const long long ttl = permission_ttl_seconds(entry.can_edit);
    if (ttl <= 0 || entry.fetched_at_unix_seconds <= 0 || current_unix_seconds <= 0) {
        return false;
    }
    if (entry.fetched_at_unix_seconds > current_unix_seconds) {
        return false;
    }
    return entry.can_edit.has_value() &&
           same_permission_subject(entry.key, current_user_subject) &&
           current_unix_seconds - entry.fetched_at_unix_seconds <= ttl;
}

long long permission_ttl_seconds(const std::optional<bool>& can_edit) {
    if (!can_edit.has_value()) {
        return 0;
    }
    return *can_edit ? kAllowedPermissionTtlSeconds : kDeniedPermissionTtlSeconds;
}

fresh_permission_result classify_fresh_permission(bool request_success,
                                                  bool can_edit,
                                                  bool server_denied) {
    if (server_denied) {
        return fresh_permission_result::denied;
    }
    if (!request_success) {
        return fresh_permission_result::unavailable;
    }
    return can_edit ? fresh_permission_result::allowed : fresh_permission_result::denied;
}

}  // namespace content_authorization_service
