#include "services/online_content_availability.h"

#include <algorithm>

namespace online_content_availability {
namespace {

bool remote_song_matches(const online_content::song_identity& identity,
                         const song_ref& remote) {
    return identity.server_url == remote.server_url &&
           identity.remote_song_id == remote.remote_song_id;
}

bool remote_chart_matches(const online_content::chart_identity& identity,
                          const chart_ref& remote) {
    return identity.server_url == remote.server_url &&
           identity.remote_song_id == remote.remote_song_id &&
           identity.remote_chart_id == remote.remote_chart_id;
}

content_status display_status_for_local(content_status local_status,
                                        content_status remote_source_status) {
    if (local_status == content_status::modified || local_status == content_status::update) {
        return local_status;
    }
    return remote_source_status;
}

int installed_remote_chart_version(const song_select::chart_option& chart,
                                   const std::optional<local_content_index::online_chart_binding>& binding) {
    int version = chart.meta.chart_version;
    if (chart.online_identity.has_value()) {
        version = std::max(version, chart.online_identity->remote_chart_version);
    }
    if (binding.has_value()) {
        version = std::max(version, binding->remote_chart_version);
    }
    return version;
}

const song_select::song_entry* find_local_song_by_id(const std::vector<song_select::song_entry>& local_songs,
                                                     const std::string& local_song_id) {
    for (const song_select::song_entry& song : local_songs) {
        if (song.song.meta.song_id == local_song_id) {
            return &song;
        }
    }
    return nullptr;
}

std::optional<local_content_index::online_song_binding> find_song_binding_by_local(
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

std::optional<local_content_index::online_song_binding> find_song_binding_by_remote(
    const local_content_index::snapshot& index,
    const std::string& server_url,
    const std::string& remote_song_id) {
    for (const local_content_index::online_song_binding& binding : index.songs) {
        if (binding.server_url == server_url && binding.remote_song_id == remote_song_id) {
            return binding;
        }
    }
    return std::nullopt;
}

std::optional<local_content_index::online_chart_binding> find_chart_binding_by_local(
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

std::optional<local_content_index::online_chart_binding> find_chart_binding_by_remote(
    const local_content_index::snapshot& index,
    const std::string& server_url,
    const std::string& remote_chart_id) {
    for (const local_content_index::online_chart_binding& binding : index.charts) {
        if (binding.server_url == server_url && binding.remote_chart_id == remote_chart_id) {
            return binding;
        }
    }
    return std::nullopt;
}

}  // namespace

resolved_song resolve_song(const std::vector<song_select::song_entry>& local_songs,
                           const local_content_index::snapshot& index,
                           const song_ref& remote,
                           content_status remote_source_status) {
    resolved_song result;
    if (remote.server_url.empty() || remote.remote_song_id.empty()) {
        return result;
    }

    const std::optional<local_content_index::online_song_binding> remote_binding =
        find_song_binding_by_remote(index, remote.server_url, remote.remote_song_id);

    for (const song_select::song_entry& local_song : local_songs) {
        const bool identity_match = local_song.online_identity.has_value() &&
                                    remote_song_matches(*local_song.online_identity, remote);
        const std::optional<local_content_index::online_song_binding> local_binding =
            find_song_binding_by_local(index, remote.server_url, local_song.song.meta.song_id);
        const bool local_binding_match = local_binding.has_value() &&
                                         local_binding->remote_song_id == remote.remote_song_id;
        const bool remote_binding_match = remote_binding.has_value() &&
                                          remote_binding->local_song_id == local_song.song.meta.song_id;
        if (!identity_match && !local_binding_match && !remote_binding_match) {
            continue;
        }

        result.local_song = &local_song;
        result.local_song_id = local_song.song.meta.song_id;
        result.installed = true;
        result.identity_matched = identity_match;
        result.binding_matched = local_binding_match || remote_binding_match;
        result.update_available =
            remote.remote_song_version > 0 &&
            local_song.song.meta.song_version < remote.remote_song_version;
        result.display_status = display_status_for_local(local_song.status, remote_source_status);
        return result;
    }

    return result;
}

resolved_chart resolve_chart(const std::vector<song_select::song_entry>& local_songs,
                             const local_content_index::snapshot& index,
                             const resolved_song& song,
                             const chart_ref& remote,
                             content_status remote_source_status) {
    resolved_chart result;
    if (!song.installed || song.local_song_id.empty() ||
        remote.server_url.empty() || remote.remote_song_id.empty() || remote.remote_chart_id.empty()) {
        return result;
    }

    const song_select::song_entry* local_song =
        song.local_song != nullptr ? song.local_song : find_local_song_by_id(local_songs, song.local_song_id);
    if (local_song == nullptr) {
        return result;
    }

    const std::optional<local_content_index::online_chart_binding> remote_binding =
        find_chart_binding_by_remote(index, remote.server_url, remote.remote_chart_id);

    for (const song_select::chart_option& local_chart : local_song->charts) {
        const bool identity_match = local_chart.online_identity.has_value() &&
                                    remote_chart_matches(*local_chart.online_identity, remote);
        const std::optional<local_content_index::online_chart_binding> local_binding =
            find_chart_binding_by_local(index, remote.server_url, local_chart.meta.chart_id);
        const bool local_binding_match = local_binding.has_value() &&
                                         local_binding->remote_song_id == remote.remote_song_id &&
                                         local_binding->remote_chart_id == remote.remote_chart_id;
        const bool remote_binding_match = remote_binding.has_value() &&
                                          remote_binding->remote_song_id == remote.remote_song_id &&
                                          remote_binding->local_chart_id == local_chart.meta.chart_id;
        if (!identity_match && !local_binding_match && !remote_binding_match) {
            continue;
        }

        result.local_chart = &local_chart;
        result.local_chart_id = local_chart.meta.chart_id;
        result.installed = true;
        result.identity_matched = identity_match;
        result.binding_matched = local_binding_match || remote_binding_match;
        const int installed_version = installed_remote_chart_version(
            local_chart,
            remote_binding_match ? remote_binding : local_binding);
        result.update_available =
            remote.remote_chart_version > 0 &&
            installed_version > 0 &&
            remote.remote_chart_version > installed_version;
        result.display_status = display_status_for_local(local_chart.status, remote_source_status);
        return result;
    }

    return result;
}

}  // namespace online_content_availability
