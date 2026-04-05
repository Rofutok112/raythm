#pragma once

#include <string>

#include "song_loader.h"

class editor_chart_identity_service final {
public:
    static std::string generated_chart_id(const song_data& song, const std::string& difficulty);
};
