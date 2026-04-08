#pragma once

#include <string>
#include <vector>

#include "ui/ui_text_editor.h"

namespace mv {

const ui::text_editor_style& mv_script_editor_style();
std::vector<ui::text_editor_span> highlight_mv_script_line(const std::string& line);
ui::text_editor_completion_result complete_mv_script_line(const std::vector<std::string>& lines,
                                                          int cursor_line, int cursor_col);

} // namespace mv
