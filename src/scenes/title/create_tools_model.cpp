#include "title/create_tools_model.h"

#include <optional>
#include <utility>

#include "content_lifecycle.h"
#include "network/server_environment.h"
#include "title/create_upload_permissions.h"

namespace title_create_tools_model {
namespace {

bool same_server(const std::string& left, const std::string& normalized_right) {
    return !left.empty() &&
           server_environment::normalize_url(left) == normalized_right;
}

std::optional<online_content::song_identity> matching_song_identity(
    const song_select::song_entry* song,
    const std::string& server_url) {
    if (song == nullptr || !song->online_identity.has_value() ||
        !same_server(song->online_identity->server_url, server_url) ||
        song->online_identity->remote_song_id.empty()) {
        return std::nullopt;
    }
    return song->online_identity;
}

std::optional<online_content::chart_identity> matching_chart_identity(
    const song_select::chart_option* chart,
    const std::string& server_url) {
    if (chart == nullptr) {
        return std::nullopt;
    }
    if (chart->online_identity.has_value() &&
        same_server(chart->online_identity->server_url, server_url) &&
        !chart->online_identity->remote_chart_id.empty()) {
        return chart->online_identity;
    }
    for (const online_content::chart_identity& link : chart->remote_links) {
        if (same_server(link.server_url, server_url) && !link.remote_chart_id.empty()) {
            return link;
        }
    }
    return std::nullopt;
}

void overlay_song_identity(local_content_index::online_song_binding& binding,
                           const online_content::song_identity& identity) {
    if (!identity.remote_song_id.empty()) {
        binding.remote_song_id = identity.remote_song_id;
    }
}

void overlay_chart_identity(local_content_index::online_chart_binding& binding,
                            const online_content::chart_identity& identity) {
    if (!identity.remote_chart_id.empty()) {
        binding.remote_chart_id = identity.remote_chart_id;
    }
    if (!identity.remote_song_id.empty()) {
        binding.remote_song_id = identity.remote_song_id;
    }
    if (identity.remote_chart_version > 0) {
        binding.remote_chart_version = identity.remote_chart_version;
    }
}

std::string lifecycle_detail(const std::string& review_status,
                             const std::string& lifecycle_status,
                             const std::string& fallback) {
    const std::string label = content_lifecycle::display_label(review_status, lifecycle_status);
    return label.empty() ? fallback : label;
}

bool has_remote_song_link(const std::optional<local_content_index::online_song_binding>& binding) {
    return binding.has_value() && !binding->remote_song_id.empty();
}

bool has_remote_chart_link(const std::optional<local_content_index::online_chart_binding>& binding) {
    return binding.has_value() && !binding->remote_chart_id.empty();
}

bool has_local_song_changes(const song_select::song_entry* song) {
    return song != nullptr && song->sync_state == content_sync_state::modified;
}

bool has_local_chart_changes(const song_select::chart_option* chart) {
    return chart != nullptr && chart->sync_state == content_sync_state::modified;
}

std::optional<local_content_index::online_song_binding> find_cached_song_by_local(
    const local_content_index::snapshot& index,
    const std::string& server_url,
    const std::string& local_song_id) {
    for (const local_content_index::online_song_binding& binding : index.songs) {
        if (binding.server_url == server_url && binding.local_song_id == local_song_id) {
            return binding;
        }
    }
    return std::nullopt;
}

std::optional<local_content_index::online_chart_binding> find_cached_chart_by_local(
    const local_content_index::snapshot& index,
    const std::string& server_url,
    const std::string& local_chart_id) {
    for (const local_content_index::online_chart_binding& binding : index.charts) {
        if (binding.server_url == server_url && binding.local_chart_id == local_chart_id) {
            return binding;
        }
    }
    return std::nullopt;
}

}  // namespace

bindings resolve_bindings(const local_content_index::snapshot& index,
                          const song_select::song_entry* song,
                          const song_select::chart_option* chart,
                          const std::string& server_url) {
    const std::string normalized_server_url = server_environment::normalize_url(server_url);
    bindings result;
    if (normalized_server_url.empty()) {
        return result;
    }

    if (song != nullptr && !song->song.meta.song_id.empty()) {
        result.song = find_cached_song_by_local(index, normalized_server_url, song->song.meta.song_id);
        if (const auto identity = matching_song_identity(song, normalized_server_url)) {
            if (result.song.has_value()) {
                overlay_song_identity(*result.song, *identity);
            } else {
                result.song = local_content_index::online_song_binding{
                    .server_url = normalized_server_url,
                    .local_song_id = song->song.meta.song_id,
                    .remote_song_id = identity->remote_song_id,
                    .origin = local_content_index::online_origin::linked,
                };
            }
        }
    }

    if (chart != nullptr && !chart->meta.chart_id.empty()) {
        result.chart = find_cached_chart_by_local(index, normalized_server_url, chart->meta.chart_id);
        if (const auto identity = matching_chart_identity(chart, normalized_server_url)) {
            if (result.chart.has_value()) {
                overlay_chart_identity(*result.chart, *identity);
            } else {
                result.chart = local_content_index::online_chart_binding{
                    .server_url = normalized_server_url,
                    .local_chart_id = chart->meta.chart_id,
                    .remote_chart_id = identity->remote_chart_id,
                    .remote_song_id = identity->remote_song_id,
                    .remote_chart_version = identity->remote_chart_version,
                    .origin = local_content_index::online_origin::linked,
                };
            }
        }
    }

    return result;
}

view_model build(const build_context& context) {
    const song_select::song_entry* song = context.song;
    const song_select::chart_option* chart = context.chart;
    const auto& song_binding = context.upload_bindings.song;
    const auto& chart_binding = context.upload_bindings.chart;
    const bool song_selected = song != nullptr;
    const bool chart_selected = chart != nullptr;
    const bool linked_remote_song = has_remote_song_link(song_binding);
    const bool linked_remote_chart = has_remote_chart_link(chart_binding);
    const bool chart_has_remote_target = linked_remote_song || linked_remote_chart;
    const bool song_modified_for_update = has_local_song_changes(song) && linked_remote_song;
    const bool chart_modified_for_update = has_local_chart_changes(chart) && linked_remote_chart;

    const std::optional<online_content::song_identity> song_identity =
        matching_song_identity(song, context.server_url);
    const std::optional<online_content::chart_identity> chart_identity =
        matching_chart_identity(chart, context.server_url);
    const std::string song_state_detail =
        song_identity.has_value()
            ? lifecycle_detail(song_identity->review_status, song_identity->lifecycle_status, "")
            : "";
    const std::string chart_state_detail =
        chart_identity.has_value()
            ? lifecycle_detail(chart_identity->review_status, chart_identity->lifecycle_status, "")
            : "";

    const bool song_can_upload = title_create_upload_permissions::can_start_song_upload(
        song_selected, context.online_status_checking, song_binding);
    const bool chart_can_upload = title_create_upload_permissions::can_start_chart_upload(
        chart_selected, context.online_status_checking, song_binding, chart_binding);
    std::string song_publish_title = "UPLOAD SONG";
    std::string song_publish_detail = "Confirm to publish";
    if (!song_selected) {
        song_publish_title = "SELECT SONG";
        song_publish_detail = "Song publish unavailable";
    } else if (context.online_status_checking) {
        song_publish_title = "CHECKING SONG";
        song_publish_detail = "Verifying online status";
    } else if (linked_remote_song) {
        song_publish_title = "UPDATE SONG";
        song_publish_detail = song_modified_for_update
            ? "Local changes ready"
            : !song_state_detail.empty()
            ? song_state_detail
            : "Verify on submit";
    } else if (song->source == content_source::official) {
        song_publish_title = "OFFICIAL SONG";
        song_publish_detail = "Verify on submit";
    } else if (song->source == content_source::community) {
        song_publish_title = "COMMUNITY SONG";
        song_publish_detail = "Verify on submit";
    }

    std::string chart_publish_title = "UPLOAD CHART";
    std::string chart_publish_detail = chart_has_remote_target ? "Confirm to publish" : "Upload song first";
    if (!chart_selected) {
        chart_publish_title = "SELECT CHART";
        chart_publish_detail = "Chart publish unavailable";
    } else if (context.online_status_checking) {
        chart_publish_title = "CHECKING CHART";
        chart_publish_detail = "Verifying online status";
    } else if (linked_remote_chart) {
        chart_publish_title = "UPDATE CHART";
        chart_publish_detail = chart_modified_for_update
            ? "Local changes ready"
            : !chart_state_detail.empty()
            ? chart_state_detail
            : "Verify on submit";
    } else if (chart->source == content_source::official) {
        chart_publish_title = "OFFICIAL CHART";
        chart_publish_detail = "Verify on submit";
    } else if (chart->source == content_source::community) {
        chart_publish_title = "COMMUNITY CHART";
        chart_publish_detail = "Verify on submit";
    }

    view_model model;
    model.song_upload_enabled = song_can_upload;
    model.chart_upload_enabled = chart_can_upload;
    model.sections = {
        {
            "Song",
            {
                {song_publish_title, song_publish_detail, action::upload_song, song_can_upload, true},
                {"NEW SONG", "Create package", action::create_song, true, false},
                {"EDIT SONG", "Metadata", action::edit_song, song_selected, false},
                {"IMPORT SONG", ".rpack", action::import_song, true, false},
                {"EXPORT SONG", ".rpack", action::export_song, song_selected, false},
            },
        },
        {
            "Chart",
            {
                {chart_publish_title, chart_publish_detail, action::upload_chart, chart_can_upload, true},
                {"NEW CHART", "Add to song", action::create_chart, song_selected, false},
                {"EDIT CHART", "Open editor", action::edit_chart, chart_selected, false},
                {"IMPORT CHART", ".rchart", action::import_chart, song_selected, false},
                {"EXPORT CHART", ".rchart", action::export_chart, chart_selected, false},
            },
        },
        {
            "More",
            {
                {"MV EDITOR", "Visuals", action::edit_mv, song_selected, false},
            },
        },
    };
    return model;
}

bool action_enabled(const view_model& model, action command) {
    for (const section& current_section : model.sections) {
        for (const entry& current_entry : current_section.entries) {
            if (current_entry.command == command) {
                return current_entry.enabled;
            }
        }
    }
    return false;
}

}  // namespace title_create_tools_model
