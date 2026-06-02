#pragma once

#include <string>

#include "chart_parser.h"

namespace editor_chart_load_service {

chart_parse_result load_chart(const std::string& chart_path);

}  // namespace editor_chart_load_service
