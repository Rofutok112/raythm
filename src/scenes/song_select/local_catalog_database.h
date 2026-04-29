#pragma once

#include <string>
#include <vector>

#include "song_select/song_select_state.h"

namespace song_select::local_catalog_database {

catalog_data load_cached_catalog();
void replace_catalog(const std::vector<song_entry>& songs);
void remove_song(const std::string& song_id);
void remove_chart(const std::string& chart_id);

}  // namespace song_select::local_catalog_database
