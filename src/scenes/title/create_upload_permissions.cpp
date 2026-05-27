#include "title/create_upload_permissions.h"

#include "title/local_content_binding.h"

namespace title_create_upload_permissions {

namespace {

bool has_explicit_edit_denial(const std::optional<bool>& can_edit) {
    return can_edit.has_value() && !*can_edit;
}

bool has_remote_song_target(
    const std::optional<local_content_index::online_song_binding>& song_binding,
    const std::optional<local_content_index::online_chart_binding>& chart_binding) {
    return (song_binding.has_value() && !song_binding->remote_song_id.empty()) ||
           (chart_binding.has_value() && !chart_binding->remote_song_id.empty());
}

}  // namespace

bool can_edit_remote(const local_content_index::online_song_binding& binding) {
    return local_content_binding::can_edit_remote(binding.can_edit, binding.origin);
}

bool can_edit_remote(const local_content_index::online_chart_binding& binding) {
    return local_content_binding::can_edit_remote(binding.can_edit, binding.origin);
}

bool can_start_song_upload(bool song_selected,
                           bool online_status_checking,
                           const std::optional<local_content_index::online_song_binding>& song_binding) {
    if (!song_selected || online_status_checking) {
        return false;
    }
    return !song_binding.has_value() || !has_explicit_edit_denial(song_binding->can_edit);
}

bool can_start_chart_upload(bool chart_selected,
                            bool online_status_checking,
                            const std::optional<local_content_index::online_song_binding>& song_binding,
                            const std::optional<local_content_index::online_chart_binding>& chart_binding) {
    if (!chart_selected || online_status_checking ||
        !has_remote_song_target(song_binding, chart_binding)) {
        return false;
    }
    if (chart_binding.has_value()) {
        return !has_explicit_edit_denial(chart_binding->can_edit);
    }
    return song_binding.has_value() && !has_explicit_edit_denial(song_binding->can_edit);
}

}  // namespace title_create_upload_permissions
