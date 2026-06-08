#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "song_select/song_select_state.h"

namespace song_select::local_catalog_database {

using progress_callback = std::function<void(std::string message, float progress)>;

catalog_data load_cached_catalog(progress_callback progress = {});
std::optional<float> find_chart_level_by_path(const std::string& chart_path);
void replace_catalog(const std::vector<song_entry>& songs);
void replace_catalog(const std::vector<song_entry>& songs, const std::string& catalog_signature);
void remove_song(const std::string& song_id);
void remove_chart(const std::string& chart_id);

}  // namespace song_select::local_catalog_database
