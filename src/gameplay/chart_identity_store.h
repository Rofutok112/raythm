#pragma once

#include <optional>
#include <string>

namespace chart_identity {

std::optional<std::string> find_song_id(const std::string& chart_id);
void put(const std::string& chart_id, const std::string& song_id);
void remove(const std::string& chart_id);
void remove_for_song(const std::string& song_id);

}  // namespace chart_identity
