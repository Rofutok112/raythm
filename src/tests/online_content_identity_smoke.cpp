#include "online_content_identity.h"

#include <cassert>

int main() {
    std::optional<online_content::chart_identity> identity;
    assert(!online_content::is_queueable(identity));

    identity = online_content::chart_identity{
        .server_url = "https://dev-api.raythm.net",
        .remote_song_id = "song-1",
        .remote_chart_id = "chart-1",
        .content_source = online_content::source::community,
        .remote_chart_version = 3,
    };
    assert(online_content::is_queueable(identity));

    identity->remote_chart_id.clear();
    assert(!online_content::is_queueable(identity));

    assert(online_content::source_from_status(content_status::official) == online_content::source::official);
    assert(online_content::source_from_status(content_status::community) == online_content::source::community);
    assert(!online_content::source_from_status(content_status::local).has_value());
    return 0;
}
