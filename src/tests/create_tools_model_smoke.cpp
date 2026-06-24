#include "title/create_tools_model.h"

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

const title_create_tools_model::entry* find_entry(
    const title_create_tools_model::view_model& model,
    title_create_tools_model::action command) {
    for (const title_create_tools_model::section& section : model.sections) {
        for (const title_create_tools_model::entry& entry : section.entries) {
            if (entry.command == command) {
                return &entry;
            }
        }
    }
    return nullptr;
}

song_select::song_entry make_song() {
    song_select::song_entry song;
    song.song.meta.song_id = "local-song";
    song.song.meta.title = "Local Song";
    song.source_status = content_status::local;
    return song;
}

song_select::chart_option make_chart() {
    song_select::chart_option chart;
    chart.meta.chart_id = "local-chart";
    chart.meta.difficulty = "Hard";
    chart.source_status = content_status::local;
    return chart;
}

}  // namespace

int main() {
    bool ok = true;

    song_select::song_entry song = make_song();
    song_select::chart_option chart = make_chart();

    title_create_tools_model::view_model local_model =
        title_create_tools_model::build({
            .song = &song,
            .chart = &chart,
            .server_url = "https://server.example",
            .online_status_checking = false,
            .upload_bindings = {},
        });

    const title_create_tools_model::entry* local_song_upload =
        find_entry(local_model, title_create_tools_model::action::upload_song);
    const title_create_tools_model::entry* local_chart_upload =
        find_entry(local_model, title_create_tools_model::action::upload_chart);
    expect(local_song_upload != nullptr && local_song_upload->enabled,
           "Expected local songs to be uploadable.", ok);
    expect(local_chart_upload != nullptr && !local_chart_upload->enabled,
           "Expected charts without a remote song target to be disabled.", ok);
    expect(local_chart_upload != nullptr && local_chart_upload->detail == "Upload song first",
           "Expected chart upload to explain that the song needs a remote target first.", ok);

    local_content_index::snapshot index;
    index.songs.push_back({
        .server_url = "https://server.example",
        .local_song_id = "local-song",
        .remote_song_id = "remote-song",
        .origin = local_content_index::online_origin::owned_upload,
    });
    song.online_identity = online_content::song_identity{
        .server_url = "https://server.example/",
        .remote_song_id = "remote-song",
        .content_source = online_content::source::community,
        .can_edit = false,
    };

    const title_create_tools_model::bindings denied_bindings =
        title_create_tools_model::resolve_bindings(index, &song, &chart, "https://server.example");
    title_create_tools_model::view_model denied_model =
        title_create_tools_model::build({
            .song = &song,
            .chart = &chart,
            .server_url = "https://server.example",
            .online_status_checking = false,
            .upload_bindings = denied_bindings,
        });
    const title_create_tools_model::entry* denied_song_upload =
        find_entry(denied_model, title_create_tools_model::action::upload_song);
    expect(denied_bindings.song.has_value() && denied_bindings.song->remote_song_id == "remote-song",
           "Expected live song identity to preserve the remote song mapping.", ok);
    expect(denied_song_upload != nullptr && denied_song_upload->enabled,
           "Expected cached canEdit=false songs to keep the update modal entry enabled.", ok);
    expect(denied_song_upload != nullptr && denied_song_upload->title == "UPDATE SONG",
           "Expected denied linked songs to still expose the update action.", ok);
    expect(denied_song_upload != nullptr && denied_song_upload->detail == "Verify on submit",
           "Expected cached denial to be deferred until submit confirmation.", ok);

    song.online_identity = std::nullopt;
    title_create_tools_model::view_model cached_denial_model =
        title_create_tools_model::build({
            .song = &song,
            .chart = &chart,
            .server_url = "https://server.example",
            .online_status_checking = false,
            .upload_bindings = denied_bindings,
        });
    const title_create_tools_model::entry* cached_denial_upload =
        find_entry(cached_denial_model, title_create_tools_model::action::upload_song);
    expect(cached_denial_upload != nullptr && cached_denial_upload->enabled,
           "Expected user-scoped cached denial hints to keep song updates confirmable.",
           ok);
    expect(cached_denial_upload != nullptr && cached_denial_upload->detail == "Verify on submit",
           "Expected user-scoped cached denial hints to be deferred until submit confirmation.",
           ok);

    title_create_tools_model::view_model cached_allow_model =
        title_create_tools_model::build({
            .song = &song,
            .chart = &chart,
            .server_url = "https://server.example",
            .online_status_checking = false,
            .upload_bindings = denied_bindings,
        });
    const title_create_tools_model::entry* cached_allow_upload =
        find_entry(cached_allow_model, title_create_tools_model::action::upload_song);
    expect(cached_allow_upload != nullptr && cached_allow_upload->enabled,
           "Expected user-scoped cached allow hints to keep song updates enabled.",
           ok);
    expect(cached_allow_upload != nullptr && cached_allow_upload->detail == "Verify on submit",
           "Expected user-scoped cached allow hints to be deferred until submit confirmation.",
           ok);

    song.source = content_source::community;
    song.source_status = content_status::community;
    song.status = content_status::community;
    song.sync_state = content_sync_state::modified;
    title_create_tools_model::view_model modified_song_model =
        title_create_tools_model::build({
            .song = &song,
            .chart = &chart,
            .server_url = "https://server.example",
            .online_status_checking = false,
            .upload_bindings = denied_bindings,
        });
    const title_create_tools_model::entry* modified_song_update =
        find_entry(modified_song_model, title_create_tools_model::action::upload_song);
    expect(modified_song_update != nullptr && modified_song_update->title == "UPDATE SONG",
           "Expected sync-modified linked songs to show the update action.",
           ok);
    expect(modified_song_update != nullptr && modified_song_update->detail == "Local changes ready",
           "Expected sync-modified linked songs to explain that local changes are ready.",
           ok);
    song.sync_state = content_sync_state::clean;

    chart.remote_links.push_back({
        .server_url = "https://server.example",
        .remote_song_id = "remote-song",
        .remote_chart_id = "remote-chart",
        .content_source = online_content::source::community,
        .remote_chart_version = 3,
        .can_edit = true,
        .lifecycle_status = "pending_review",
    });
    const title_create_tools_model::bindings chart_bindings =
        title_create_tools_model::resolve_bindings({}, &song, &chart, "https://server.example");
    title_create_tools_model::view_model chart_model =
        title_create_tools_model::build({
            .song = &song,
            .chart = &chart,
            .server_url = "https://server.example",
            .online_status_checking = false,
            .upload_bindings = chart_bindings,
        });
    const title_create_tools_model::entry* chart_update =
        find_entry(chart_model, title_create_tools_model::action::upload_chart);
    expect(chart_bindings.chart.has_value() && chart_bindings.chart->remote_chart_id == "remote-chart",
           "Expected chart remote links to resolve without touching the database.", ok);
    expect(chart_update != nullptr && chart_update->enabled,
           "Expected editable linked charts to be enabled.", ok);
    expect(chart_update != nullptr && chart_update->title == "UPDATE CHART",
           "Expected editable linked charts to show the update action.", ok);
    expect(chart_update != nullptr && chart_update->detail == "PENDING REVIEW",
           "Expected chart lifecycle state to be surfaced in the create tools model.", ok);

    chart.source = content_source::community;
    chart.source_status = content_status::community;
    chart.status = content_status::community;
    chart.sync_state = content_sync_state::modified;
    title_create_tools_model::view_model modified_chart_model =
        title_create_tools_model::build({
            .song = &song,
            .chart = &chart,
            .server_url = "https://server.example",
            .online_status_checking = false,
            .upload_bindings = chart_bindings,
        });
    const title_create_tools_model::entry* modified_chart_update =
        find_entry(modified_chart_model, title_create_tools_model::action::upload_chart);
    expect(modified_chart_update != nullptr && modified_chart_update->title == "UPDATE CHART",
           "Expected sync-modified linked charts to show the update action.",
           ok);
    expect(modified_chart_update != nullptr && modified_chart_update->detail == "Local changes ready",
           "Expected sync-modified linked charts to explain that local changes are ready.",
           ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "create_tools_model smoke test passed\n";
    return EXIT_SUCCESS;
}
