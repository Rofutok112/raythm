#include "title/create_upload_permissions.h"

#include <cstdlib>
#include <iostream>
#include <optional>
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
                                                      std::optional<bool> can_edit) {
    return {
        .server_url = "https://server.example",
        .local_song_id = "local-song",
        .remote_song_id = "remote-song",
        .origin = origin,
        .can_edit = can_edit,
    };
}

local_content_index::online_chart_binding chart_binding(local_content_index::online_origin origin,
                                                        std::optional<bool> can_edit) {
    return {
        .server_url = "https://server.example",
        .local_chart_id = "local-chart",
        .remote_chart_id = "remote-chart",
        .remote_song_id = "remote-song",
        .remote_chart_version = 2,
        .origin = origin,
        .can_edit = can_edit,
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

    const auto owned_unknown = song_binding(local_content_index::online_origin::owned_upload, std::nullopt);
    const auto downloaded_editable = song_binding(local_content_index::online_origin::downloaded, true);
    const auto downloaded_denied = song_binding(local_content_index::online_origin::downloaded, false);
    const auto downloaded_unknown = song_binding(local_content_index::online_origin::downloaded, std::nullopt);

    expect(title_create_upload_permissions::can_edit_remote(owned_unknown),
           "Expected owned_upload with no canEdit to stay editable.",
           ok);
    expect(title_create_upload_permissions::can_edit_remote(downloaded_editable),
           "Expected canEdit=true downloaded song to be editable.",
           ok);
    expect(!title_create_upload_permissions::can_edit_remote(downloaded_denied),
           "Expected canEdit=false downloaded song to be non-editable.",
           ok);
    expect(!title_create_upload_permissions::can_edit_remote(downloaded_unknown),
           "Expected missing canEdit without owned_upload to deny direct remote edit.",
           ok);

    expect(title_create_upload_permissions::can_start_song_upload(true, false, std::nullopt),
           "Expected local unlinked songs to remain uploadable.",
           ok);
    expect(title_create_upload_permissions::can_start_song_upload(true, false, downloaded_editable),
           "Expected canEdit=true linked songs to expose update.",
           ok);
    expect(!title_create_upload_permissions::can_start_song_upload(true, false, downloaded_denied),
           "Expected canEdit=false linked songs to hide update.",
           ok);
    expect(title_create_upload_permissions::can_start_song_upload(true, false, owned_unknown),
           "Expected missing canEdit owned songs to preserve old update behavior.",
           ok);
    expect(title_create_upload_permissions::can_start_song_upload(true, false, downloaded_unknown),
           "Expected missing canEdit linked songs to start upload for a permission check.",
           ok);

    const auto chart_editable = chart_binding(local_content_index::online_origin::downloaded, true);
    const auto chart_denied = chart_binding(local_content_index::online_origin::downloaded, false);
    const auto chart_unknown = chart_binding(local_content_index::online_origin::downloaded, std::nullopt);
    const auto owned_chart_unknown = chart_binding(local_content_index::online_origin::owned_upload, std::nullopt);

    expect(title_create_upload_permissions::can_start_chart_upload(true, false, downloaded_editable, std::nullopt),
           "Expected canEdit=true linked songs to allow new chart upload.",
           ok);
    expect(!title_create_upload_permissions::can_start_chart_upload(true, false, downloaded_denied, std::nullopt),
           "Expected canEdit=false linked songs to block new chart upload.",
           ok);
    expect(title_create_upload_permissions::can_start_chart_upload(true, false, downloaded_unknown, std::nullopt),
           "Expected missing song canEdit to start new chart upload for a permission check.",
           ok);
    expect(title_create_upload_permissions::can_start_chart_upload(true, false, downloaded_editable, chart_editable),
           "Expected canEdit=true linked charts to expose update.",
           ok);
    expect(!title_create_upload_permissions::can_start_chart_upload(true, false, downloaded_editable, chart_denied),
           "Expected canEdit=false linked charts to hide update.",
           ok);
    expect(title_create_upload_permissions::can_start_chart_upload(true, false, downloaded_editable, chart_unknown),
           "Expected missing chart canEdit to start chart update for a permission check.",
           ok);
    expect(title_create_upload_permissions::can_start_chart_upload(true, false, std::nullopt, chart_editable),
           "Expected linked charts with a remote song target to update without a local song binding.",
           ok);
    expect(title_create_upload_permissions::can_start_chart_upload(true, false, std::nullopt, chart_unknown),
           "Expected linked charts with missing canEdit to start upload for a permission check.",
           ok);
    expect(title_create_upload_permissions::can_start_chart_upload(true, false, downloaded_denied, owned_chart_unknown),
           "Expected missing canEdit owned charts to preserve old update behavior.",
           ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "create_upload_permissions smoke test passed\n";
    return EXIT_SUCCESS;
}
