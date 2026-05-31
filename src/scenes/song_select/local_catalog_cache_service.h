#pragma once

#include <optional>
#include <string>
#include <vector>

#include "managed_content_storage.h"
#include "song_select/song_select_state.h"

namespace song_select::local_catalog_cache_service {

struct refresh_guard {
    std::string signature;
};

std::optional<catalog_data> load_ready_catalog();
refresh_guard capture_refresh_guard();
void replace_if_unchanged(const refresh_guard& guard, const std::vector<song_entry>& songs);
void sync_managed_manifest_identity(const managed_content_storage::package_manifest& manifest);

}  // namespace song_select::local_catalog_cache_service
