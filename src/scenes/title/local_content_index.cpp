#include "title/local_content_index.h"

#include "chart_identity_store.h"
#include "title/local_content_database.h"

namespace local_content_index {
namespace {

online_song_binding to_index_binding(const title_upload_mapping::song_mapping_entry& entry) {
    return {
        .server_url = entry.server_url,
        .local_song_id = entry.local_song_id,
        .remote_song_id = entry.remote_song_id,
        .origin = entry.origin,
    };
}

online_chart_binding to_index_binding(const title_upload_mapping::chart_mapping_entry& entry) {
    return {
        .server_url = entry.server_url,
        .local_chart_id = entry.local_chart_id,
        .local_song_id = entry.local_song_id,
        .remote_chart_id = entry.remote_chart_id,
        .remote_song_id = entry.remote_song_id,
        .origin = entry.origin,
    };
}

}  // namespace

std::optional<std::string> linked_song_for_chart(const std::string& local_chart_id) {
    return chart_identity::find_song_id(local_chart_id);
}

void link_chart_to_song(const std::string& local_chart_id, const std::string& local_song_id) {
    chart_identity::put(local_chart_id, local_song_id);
}

void unlink_chart(const std::string& local_chart_id) {
    chart_identity::remove(local_chart_id);
}

void unlink_charts_for_song(const std::string& local_song_id) {
    chart_identity::remove_for_song(local_song_id);
}

snapshot load_snapshot() {
    const title_upload_mapping::store mappings = local_content_database::load_mappings();
    snapshot index;
    index.songs.reserve(mappings.songs.size());
    for (const title_upload_mapping::song_mapping_entry& entry : mappings.songs) {
        index.songs.push_back(to_index_binding(entry));
    }
    index.charts.reserve(mappings.charts.size());
    for (const title_upload_mapping::chart_mapping_entry& entry : mappings.charts) {
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
    const std::optional<title_upload_mapping::song_mapping_entry> binding =
        local_content_database::find_song_by_local(server_url, local_song_id);
    return binding.has_value() ? std::optional<online_song_binding>(to_index_binding(*binding)) : std::nullopt;
}

std::optional<online_song_binding> find_song_by_remote(const std::string& server_url,
                                                       const std::string& remote_song_id) {
    const std::optional<title_upload_mapping::song_mapping_entry> binding =
        local_content_database::find_song_by_remote(server_url, remote_song_id);
    return binding.has_value() ? std::optional<online_song_binding>(to_index_binding(*binding)) : std::nullopt;
}

std::optional<online_chart_binding> find_chart_by_local(const std::string& server_url,
                                                        const std::string& local_chart_id) {
    const std::optional<title_upload_mapping::chart_mapping_entry> binding =
        local_content_database::find_chart_by_local(server_url, local_chart_id);
    return binding.has_value() ? std::optional<online_chart_binding>(to_index_binding(*binding)) : std::nullopt;
}

std::optional<online_chart_binding> find_chart_by_remote(const std::string& server_url,
                                                         const std::string& remote_chart_id) {
    const std::optional<title_upload_mapping::chart_mapping_entry> binding =
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
        .local_song_id = binding.local_song_id,
        .remote_chart_id = binding.remote_chart_id,
        .remote_song_id = binding.remote_song_id,
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
