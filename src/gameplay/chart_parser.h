#pragma once

#include <string>
#include <utility>
#include <vector>

#include "data_models.h"

class chart_parser {
public:
    static chart_parse_result parse(const std::string& file_path);

private:
    static chart_meta parse_metadata(const std::vector<std::pair<int, std::string>>& lines, std::vector<std::string>& errors);
    static std::vector<timing_event> parse_timing(const std::vector<std::pair<int, std::string>>& lines,
                                                  std::vector<std::string>& errors);
    static std::vector<scroll_automation_point> parse_scroll_automation(
        const std::vector<std::pair<int, std::string>>& lines,
        std::vector<std::string>& errors);
    static scroll_automation_guides parse_scroll_automation_guides(
        const std::vector<std::pair<int, std::string>>& lines,
        std::vector<std::string>& errors);
    static std::vector<note_data> parse_notes(const std::vector<std::pair<int, std::string>>& lines,
                                              std::vector<std::string>& errors);
    static std::vector<std::string> validate(const chart_data& data);
};
