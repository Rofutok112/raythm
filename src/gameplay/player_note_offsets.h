#pragma once

#include <string>
#include <unordered_map>

using player_song_offset_map = std::unordered_map<std::string, int>;

player_song_offset_map load_player_song_offsets();
int load_player_song_offset(const std::string& song_id);
bool save_player_song_offset(const std::string& song_id, int offset_ms);
