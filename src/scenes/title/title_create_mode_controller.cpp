#include "title/title_create_mode_controller.h"

#include <optional>

#include "models/data_models.h"
#include "models/online_content_identity.h"
#include "network/server_environment.h"
#include "song_select/song_select_navigation.h"
#include "title/create_upload_permissions.h"
#include "title/local_content_index.h"
#include "title/title_local_song_select_controller.h"
#include "ui_notice.h"

namespace {

bool same_server(const std::string& left, const std::string& right) {
    return !left.empty() &&
           server_environment::normalize_url(left) == server_environment::normalize_url(right);
}

std::optional<local_content_index::online_song_binding> song_upload_binding(
    const song_select::song_entry& song,
    const std::string& server_url) {
    std::optional<local_content_index::online_song_binding> binding =
        local_content_index::find_song_by_local(server_url, song.song.meta.song_id);
    if (binding.has_value() || !song.online_identity.has_value() ||
        !same_server(song.online_identity->server_url, server_url) ||
        song.online_identity->remote_song_id.empty()) {
        return binding;
    }
    return local_content_index::online_song_binding{
        .server_url = server_url,
        .local_song_id = song.song.meta.song_id,
        .remote_song_id = song.online_identity->remote_song_id,
        .origin = local_content_index::online_origin::linked,
        .can_edit = song.online_identity->can_edit,
        .lifecycle_status = song.online_identity->lifecycle_status,
    };
}

std::optional<local_content_index::online_chart_binding> chart_upload_binding(
    const song_select::chart_option& chart,
    const std::string& server_url) {
    std::optional<local_content_index::online_chart_binding> binding =
        local_content_index::find_chart_by_local(server_url, chart.meta.chart_id);
    if (binding.has_value()) {
        return binding;
    }
    const auto make_binding = [&](const online_content::chart_identity& identity) {
        return local_content_index::online_chart_binding{
            .server_url = server_url,
            .local_chart_id = chart.meta.chart_id,
            .remote_chart_id = identity.remote_chart_id,
            .remote_song_id = identity.remote_song_id,
            .remote_chart_version = identity.remote_chart_version,
            .origin = local_content_index::online_origin::linked,
            .can_edit = identity.can_edit,
            .lifecycle_status = identity.lifecycle_status,
        };
    };
    if (chart.online_identity.has_value() &&
        same_server(chart.online_identity->server_url, server_url) &&
        !chart.online_identity->remote_chart_id.empty()) {
        return make_binding(*chart.online_identity);
    }
    for (const online_content::chart_identity& link : chart.remote_links) {
        if (same_server(link.server_url, server_url) && !link.remote_chart_id.empty()) {
            return make_binding(link);
        }
    }
    return std::nullopt;
}

bool can_upload_song(const song_select::song_entry& song, const std::string& session_server_url) {
    const std::string server_url = server_environment::normalize_url(session_server_url);
    const auto binding = song_upload_binding(song, server_url);
    return title_create_upload_permissions::can_start_song_upload(true, false, binding);
}

bool can_upload_chart(const song_select::song_entry& song,
                      const song_select::chart_option& chart,
                      const std::string& session_server_url) {
    const std::string server_url = server_environment::normalize_url(session_server_url);
    const auto song_binding = song_upload_binding(song, server_url);
    const auto chart_binding = chart_upload_binding(chart, server_url);
    return title_create_upload_permissions::can_start_chart_upload(true, false, song_binding, chart_binding);
}

}  // namespace

void title_create_mode_controller::update(scene_manager& manager,
                                          song_select::state& state,
                                          title_play_transfer_controller& transfer_controller,
                                          float play_view_anim,
                                          Rectangle play_entry_origin_rect,
                                          float dt,
                                          const callbacks& callbacks) {
    const title_play_view::update_result result =
        title_local_song_select_controller::update(
            state, title_play_view::mode::create, play_view_anim, play_entry_origin_rect, dt);
    const bool create_action_requested =
        result.create_song_requested ||
        result.edit_song_requested ||
        result.upload_song_requested ||
        result.import_song_requested ||
        result.export_song_requested ||
        result.create_chart_requested ||
        result.edit_chart_requested ||
        result.upload_chart_requested ||
        result.import_chart_requested ||
        result.export_chart_requested ||
        result.edit_mv_requested ||
        result.manage_library_requested;

    if (result.back_requested) {
        callbacks.enter_home();
        return;
    }
    if (result.song_selection_changed) {
        callbacks.sync_preview();
        return;
    }
    if (result.chart_selection_changed) {
        return;
    }
    if (callbacks.upload_in_progress() && create_action_requested) {
        ui::notify("Wait for the current upload to finish.", ui::notice_tone::info, 1.8f);
        return;
    }
    if (transfer_controller.busy() && create_action_requested) {
        ui::notify("Wait for the current transfer to finish.", ui::notice_tone::info, 1.8f);
        return;
    }

    const song_select::song_entry* song = song_select::selected_song(state);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);

    if (result.create_song_requested) {
        manager.change_scene(song_select::make_song_create_scene(manager));
        return;
    }
    if (result.edit_song_requested && song != nullptr) {
        manager.change_scene(song_select::make_edit_song_scene(manager, *song));
        return;
    }
    if (result.import_song_requested) {
        transfer_controller.start_song_import(state);
        return;
    }
    if (result.export_song_requested) {
        if (song == nullptr) {
            song_select::queue_status_message(state, "Select a song to export.", true);
        } else {
            transfer_controller.start_song_export(state);
        }
        return;
    }
    if (result.upload_song_requested) {
        if (song == nullptr) {
            song_select::queue_status_message(state, "Select a song to upload.", true);
        } else if (!can_upload_song(*song, state.auth.server_url)) {
            ui::notify("This song cannot be edited by the current account.", ui::notice_tone::error, 2.8f);
        } else {
            callbacks.start_song_upload(*song);
        }
        return;
    }
    if (result.create_chart_requested && song != nullptr) {
        manager.change_scene(song_select::make_new_chart_scene(manager, *song, state.difficulty_index));
        return;
    }
    if (result.edit_chart_requested && song != nullptr && chart != nullptr) {
        manager.change_scene(song_select::make_edit_chart_scene(manager, *song, *chart));
        return;
    }
    if (result.import_chart_requested) {
        if (song == nullptr) {
            song_select::queue_status_message(state, "Select a song before importing a chart.", true);
        } else {
            transfer_controller.start_chart_import(state, callbacks.transfer_callbacks(),
                                                   callbacks.sync_media_on_transfer());
        }
        return;
    }
    if (result.export_chart_requested) {
        if (song == nullptr || chart == nullptr) {
            song_select::queue_status_message(state, "Select a chart to export.", true);
        } else {
            transfer_controller.start_chart_export(state, callbacks.transfer_callbacks(),
                                                   callbacks.sync_media_on_transfer());
        }
        return;
    }
    if (result.upload_chart_requested) {
        if (song == nullptr || chart == nullptr) {
            song_select::queue_status_message(state, "Select a chart to upload.", true);
        } else if (!can_upload_chart(*song, *chart, state.auth.server_url)) {
            ui::notify("This chart cannot be edited by the current account.", ui::notice_tone::error, 2.8f);
        } else {
            callbacks.start_chart_upload(*song, *chart);
        }
        return;
    }
    if (result.edit_mv_requested && song != nullptr) {
        manager.change_scene(song_select::make_mv_editor_scene(manager, *song));
        return;
    }
    if (result.manage_library_requested) {
        manager.change_scene(song_select::make_legacy_song_select_scene(
            manager,
            song != nullptr ? song->song.meta.song_id : "",
            chart != nullptr ? chart->meta.chart_id : "",
            std::nullopt,
            false));
        return;
    }
}
