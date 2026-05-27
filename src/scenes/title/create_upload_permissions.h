#pragma once

#include <optional>

#include "title/local_content_index.h"

namespace title_create_upload_permissions {

bool can_edit_remote(const local_content_index::online_song_binding& binding);
bool can_edit_remote(const local_content_index::online_chart_binding& binding);

bool can_start_song_upload(bool song_selected,
                           bool online_status_checking,
                           const std::optional<local_content_index::online_song_binding>& song_binding);

bool can_start_chart_upload(bool chart_selected,
                            bool online_status_checking,
                            const std::optional<local_content_index::online_song_binding>& song_binding,
                            const std::optional<local_content_index::online_chart_binding>& chart_binding);

}  // namespace title_create_upload_permissions

