#pragma once

#include <string>

#include "data_models.h"

namespace song_select {

std::string regenerated_export_id(const std::string& current_id);
chart_data make_export_chart_copy(const chart_data& chart);
song_meta make_export_song_meta_copy(const song_meta& meta);

}  // namespace song_select
