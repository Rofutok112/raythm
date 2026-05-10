#include "title/local_content_index.h"

#include "title/local_content_database.h"

namespace local_content_index {
namespace {

online_song_binding to_index_binding(const local_content_binding::song_binding& entry) {
    return {
        .server_url = entry.server_url,
        .local_song_id = entry.local_song_id,
        .remote_song_id = entry.remote_song_id,
        .origin = entry.origin,
    };
}

online_chart_binding to_index_binding(const local_content_binding::chart_binding& entry) {
    return {
        .server_url = entry.server_url,
        .local_chart_id = entry.local_chart_id,
        .remote_chart_id = entry.remote_chart_id,
        .remote_song_id = entry.remote_song_id,
        .remote_chart_version = entry.remote_chart_version,
        .origin = entry.origin,
    };
}

}  // namespace

snapshot load_snapshot() {
    const local_content_binding::store mappings = local_content_database::load_mappings();
    snapshot index;
    index.songs.reserve(mappings.songs.size());
    for (const local_content_binding::song_binding& entry : mappings.songs) {
        index.songs.push_back(to_index_binding(entry));
    }
    index.charts.reserve(mappings.charts.size());
    for (const local_content_binding::chart_binding& entry : mappings.charts) {
        index.charts.push_back(to_index_binding(entry));
    }
    return index;
}

std::optional<online_song_binding> find_song_by_local(const snapshot& index,
                                                      const std::string& server_url,
                                                      const std::string& local_song_id) {
    for (const online_song_binding& binding : index.songs) {
        if (binding.server_url == server_url && binding.local_song_id == local_song_id) {
            return binding;
        }
    }
    return std::nullopt;
}

std::optional<online_song_binding> find_song_by_remote(const snapshot& index,
                                                       const std::string& server_url,
                                                       const std::string& remote_song_id) {
    for (const online_song_binding& binding : index.songs) {
        if (binding.server_url == server_url && binding.remote_song_id == remote_song_id) {
            return binding;
        }
    }
    return std::nullopt;
}

std::optional<online_chart_binding> find_chart_by_local(const snapshot& index,
                                                        const std::string& server_url,
                                                        const std::string& local_chart_id) {
    for (const online_chart_binding& binding : index.charts) {
        if (binding.server_url == server_url && binding.local_chart_id == local_chart_id) {
            return binding;
        }
    }
    return std::nullopt;
}

std::optional<online_chart_binding> find_chart_by_remote(const snapshot& index,
                                                         const std::string& server_url,
                                                         const std::string& remote_chart_id) {
    for (const online_chart_binding& binding : index.charts) {
        if (binding.server_url == server_url && binding.remote_chart_id == remote_chart_id) {
            return binding;
        }
    }
    return std::nullopt;
}

std::optional<online_song_binding> find_song_by_local(const std::string& server_url,
                                                      const std::string& local_song_id) {
    const std::optional<local_content_binding::song_binding> binding =
        local_content_database::find_song_by_local(server_url, local_song_id);
    return binding.has_value() ? std::optional<online_song_binding>(to_index_binding(*binding)) : std::nullopt;
}

std::optional<online_song_binding> find_song_by_remote(const std::string& server_url,
                                                       const std::string& remote_song_id) {
    const std::optional<local_content_binding::song_binding> binding =
        local_content_database::find_song_by_remote(server_url, remote_song_id);
    return binding.has_value() ? std::optional<online_song_binding>(to_index_binding(*binding)) : std::nullopt;
}

std::optional<online_chart_binding> find_chart_by_local(const std::string& server_url,
                                                        const std::string& local_chart_id) {
    const std::optional<local_content_binding::chart_binding> binding =
        local_content_database::find_chart_by_local(server_url, local_chart_id);
    return binding.has_value() ? std::optional<online_chart_binding>(to_index_binding(*binding)) : std::nullopt;
}

std::optional<online_chart_binding> find_chart_by_remote(const std::string& server_url,
                                                         const std::string& remote_chart_id) {
    const std::optional<local_content_binding::chart_binding> binding =
        local_content_database::find_chart_by_remote(server_url, remote_chart_id);
    return binding.has_value() ? std::optional<online_chart_binding>(to_index_binding(*binding)) : std::nullopt;
}

void put_song_binding(const online_song_binding& binding) {
    local_content_database::put_song({
        .server_url = binding.server_url,
        .local_song_id = binding.local_song_id,
        .remote_song_id = binding.remote_song_id,
        .origin = binding.origin,
    });
}

void put_chart_binding(const online_chart_binding& binding) {
    local_content_database::put_chart({
        .server_url = binding.server_url,
        .local_chart_id = binding.local_chart_id,
        .remote_chart_id = binding.remote_chart_id,
        .remote_song_id = binding.remote_song_id,
        .remote_chart_version = binding.remote_chart_version,
        .origin = binding.origin,
    });
}

void remove_song_binding(const std::string& server_url, const std::string& local_song_id) {
    local_content_database::remove_song(server_url, local_song_id);
}

void remove_chart_binding(const std::string& server_url, const std::string& local_chart_id) {
    local_content_database::remove_chart(server_url, local_chart_id);
}

void remove_song_bindings(const std::string& local_song_id) {
    local_content_database::remove_song_bindings(local_song_id);
}

void remove_chart_bindings(const std::string& local_chart_id) {
    local_content_database::remove_chart_bindings(local_chart_id);
}

}  // namespace local_content_index
