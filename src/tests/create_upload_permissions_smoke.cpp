#include "title/create_upload_permissions.h"

#include <cstdlib>
#include <iostream>
#include <string>

#include "online_content_identity.h"

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

local_content_index::online_song_binding song_binding(local_content_index::online_origin origin,
                                                      bool has_remote = true) {
    return {
        .server_url = "https://server.example",
        .local_song_id = "local-song",
        .remote_song_id = has_remote ? "remote-song" : "",
        .origin = origin,
    };
}

local_content_index::online_chart_binding chart_binding(local_content_index::online_origin origin,
                                                        bool has_remote = true) {
    return {
        .server_url = "https://server.example",
        .local_chart_id = "local-chart",
        .remote_chart_id = has_remote ? "remote-chart" : "",
        .remote_song_id = has_remote ? "remote-song" : "",
        .remote_chart_version = 2,
        .origin = origin,
    };
}

}  // namespace

int main() {
    bool ok = true;

    expect(online_content::can_edit_with_owned_fallback(true, false),
           "Expected canEdit=true to allow a non-owned remote update.",
           ok);
    expect(!online_content::can_edit_with_owned_fallback(false, true),
           "Expected canEdit=false to override owned_upload fallback.",
           ok);
    expect(online_content::can_edit_with_owned_fallback(std::nullopt, true),
           "Expected missing canEdit to fall back to owned_upload.",
           ok);
    expect(!online_content::can_edit_with_owned_fallback(std::nullopt, false),
           "Expected missing canEdit without owned_upload to deny direct update.",
           ok);

    const auto owned_unknown = song_binding(local_content_index::online_origin::owned_upload);
    const auto downloaded_linked = song_binding(local_content_index::online_origin::downloaded);

    expect(title_create_upload_permissions::can_start_song_upload(true, false, std::nullopt),
           "Expected local unlinked songs to remain uploadable.",
           ok);
    expect(title_create_upload_permissions::can_start_song_upload(true, false, downloaded_linked),
           "Expected linked songs to expose update for a fresh permission check.",
           ok);
    expect(title_create_upload_permissions::can_start_song_upload(true, false, owned_unknown),
           "Expected missing canEdit owned songs to preserve old update behavior.",
           ok);

    const auto chart_linked = chart_binding(local_content_index::online_origin::downloaded);
    const auto owned_chart_unknown = chart_binding(local_content_index::online_origin::owned_upload);

    expect(title_create_upload_permissions::can_start_chart_upload(true, false, downloaded_linked, std::nullopt),
           "Expected linked songs to allow new chart upload for a fresh permission check.",
           ok);
    expect(title_create_upload_permissions::can_start_chart_upload(true, false, downloaded_linked, chart_linked),
           "Expected linked charts to expose update for a fresh permission check.",
           ok);
    expect(title_create_upload_permissions::can_start_chart_upload(true, false, std::nullopt, chart_linked),
           "Expected linked charts with a remote song target to update without a local song binding.",
           ok);
    expect(title_create_upload_permissions::can_start_chart_upload(true, false, downloaded_linked, owned_chart_unknown),
           "Expected missing canEdit owned charts to preserve old update behavior.",
           ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "create_upload_permissions smoke test passed\n";
    return EXIT_SUCCESS;
}
