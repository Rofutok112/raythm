#pragma once

#include <string>

#include "chart_parser.h"
#include "data_models.h"

class song_loader {
public:
    static song_load_result load_all(const std::string& songs_dir,
                                     content_source source = content_source::legacy_assets);
    static chart_parse_result load_chart(const std::string& path);
    static content_source classify_chart_path(const std::string& path);

    // Scan a directory of flat .chart files and attach them to matching songs by song_id.
    static void attach_external_charts(const std::string& charts_dir, std::vector<song_data>& songs);
};
