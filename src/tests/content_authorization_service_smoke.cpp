#include "services/content_authorization_service.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

}  // namespace

int main() {
    bool ok = true;

    using content_authorization_service::content_type;
    using content_authorization_service::fresh_permission_result;
    using content_authorization_service::permission_entry;
    using content_authorization_service::permission_key;

    const permission_key alice_song{
        .server_url = "https://server.example",
        .type = content_type::song,
        .remote_id = "song-1",
        .user_id = "alice",
    };
    const permission_key bob_song{
        .server_url = "https://server.example",
        .type = content_type::song,
        .remote_id = "song-1",
        .user_id = "bob",
    };
    const permission_key alice_chart{
        .server_url = "https://server.example",
        .type = content_type::chart,
        .remote_id = "song-1",
        .user_id = "alice",
    };

    expect(content_authorization_service::same_permission_subject(alice_song, alice_song),
           "Expected identical permission subjects to match.",
           ok);
    expect(!content_authorization_service::same_permission_subject(alice_song, bob_song),
           "Expected permission cache subjects to be isolated by user id.",
           ok);
    expect(!content_authorization_service::same_permission_subject(alice_song, alice_chart),
           "Expected permission cache subjects to be isolated by content type.",
           ok);

    const permission_entry cached_denial{
        .key = alice_song,
        .can_edit = false,
        .fetched_at_unix_seconds = 42,
    };
    expect(content_authorization_service::can_use_cached_permission(cached_denial, alice_song, 43),
           "Expected cached permission to be usable for the same user/content subject.",
           ok);
    expect(!content_authorization_service::can_use_cached_permission(cached_denial, bob_song, 43),
           "Expected cached permission to be unusable after account switch.",
           ok);
    expect(!content_authorization_service::can_use_cached_permission(cached_denial, alice_song, 42 + 5 * 60 + 1),
           "Expected denied permission cache to expire after its short TTL.",
           ok);
    expect(!content_authorization_service::can_use_cached_permission(cached_denial, alice_song, 41),
           "Expected future-dated permission cache entries to be ignored.",
           ok);

    const permission_entry cached_allow{
        .key = alice_song,
        .can_edit = true,
        .fetched_at_unix_seconds = 42,
    };
    expect(content_authorization_service::can_use_cached_permission(cached_allow, alice_song, 42 + 15 * 60),
           "Expected allowed permission cache to be usable through its TTL boundary.",
           ok);
    expect(!content_authorization_service::can_use_cached_permission(cached_allow, alice_song, 42 + 15 * 60 + 1),
           "Expected allowed permission cache to expire after its TTL.",
           ok);

    const permission_entry unknown{
        .key = alice_song,
        .can_edit = std::nullopt,
        .fetched_at_unix_seconds = 42,
    };
    expect(!content_authorization_service::can_use_cached_permission(unknown, alice_song, 43),
           "Expected missing canEdit to remain unknown, not an authorization result.",
           ok);
    expect(content_authorization_service::classify_fresh_permission(true, true, false) ==
               fresh_permission_result::allowed,
           "Expected a successful fresh canEdit=true check to allow edits.",
           ok);
    expect(content_authorization_service::classify_fresh_permission(true, false, false) ==
               fresh_permission_result::denied,
           "Expected a fresh canEdit=false server result to deny edits.",
           ok);
    expect(content_authorization_service::classify_fresh_permission(false, false, true) ==
               fresh_permission_result::denied,
           "Expected a 403 fresh permission check to be treated as a server denial.",
           ok);
    expect(content_authorization_service::classify_fresh_permission(false, false, false) ==
               fresh_permission_result::unavailable,
           "Expected failed permission fetches without server denial to remain unavailable.",
           ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "content_authorization_service smoke test passed\n";
    return EXIT_SUCCESS;
}
