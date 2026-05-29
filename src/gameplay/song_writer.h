#pragma once

#include <string>

#include "data_models.h"

class song_writer {
public:
    static std::string serialize_song_json(const song_meta& meta);
    // Write song.json to the given directory. Creates the directory if needed.
    static bool write_song_json(const song_meta& meta, const std::string& directory);
};
