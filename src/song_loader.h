#pragma once

#include <string>

#include "chart_parser.h"
#include "data_models.h"

class song_loader {
public:
    static song_load_result load_all(const std::string& songs_dir);
    static chart_parse_result load_chart(const std::string& path);
};
