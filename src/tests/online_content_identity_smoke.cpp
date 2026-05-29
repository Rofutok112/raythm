#include "online_content_identity.h"
#include "content_lifecycle.h"

#include <cassert>
#include <optional>

int main() {
    std::optional<online_content::chart_identity> identity;
    assert(!online_content::is_queueable(identity));

    identity = online_content::chart_identity{
        .server_url = "https://dev-api.raythm.net",
        .remote_song_id = "song-1",
        .remote_chart_id = "chart-1",
        .content_source = online_content::source::community,
        .remote_chart_version = 3,
        .can_edit = true,
        .lifecycle_status = "active",
        .review_status = "pending_review",
    };
    assert(online_content::is_queueable(identity));
    assert(identity->can_edit == std::optional<bool>(true));
    assert(identity->lifecycle_status == "active");
    assert(identity->review_status == "pending_review");
    assert(content_lifecycle::display_label(identity->review_status, identity->lifecycle_status) == "PENDING REVIEW");
    assert(content_lifecycle::lifecycle_is_active(identity->lifecycle_status));
    assert(content_lifecycle::display_label("pending_review", "archived") == "ARCHIVED");
    assert(online_content::explicit_edit_permission(*identity) == std::optional<bool>(true));
    assert(online_content::can_edit_with_owned_fallback(identity->can_edit, false));

    identity->remote_chart_id.clear();
    assert(!online_content::is_queueable(identity));
    identity->can_edit = false;
    assert(!online_content::can_edit_with_owned_fallback(identity->can_edit, true));
    identity->can_edit.reset();
    assert(online_content::can_edit_with_owned_fallback(identity->can_edit, true));
    assert(!online_content::can_edit_with_owned_fallback(identity->can_edit, false));

    assert(online_content::source_from_status(content_status::official) == online_content::source::official);
    assert(online_content::source_from_status(content_status::community) == online_content::source::community);
    assert(!online_content::source_from_status(content_status::local).has_value());
    return 0;
}
