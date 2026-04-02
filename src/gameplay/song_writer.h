#pragma once

#include <string>

#include "data_models.h"

class song_writer {
public:
    // Write song.json to the given directory. Creates the directory if needed.
    static bool write_song_json(const song_meta& meta, const std::string& directory);
};
