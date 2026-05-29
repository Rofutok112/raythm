#pragma once

#include <string>

#include "data_models.h"

class chart_serializer {
public:
    static std::string serialize_to_string(const chart_data& data);
    static bool serialize(const chart_data& data, const std::string& file_path);
};
