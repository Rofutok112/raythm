#include "editor/service/editor_chart_load_service.h"

#include "song_loader.h"

namespace editor_chart_load_service {

chart_parse_result load_chart(const std::string& chart_path) {
    return song_loader::load_chart(chart_path);
}

}  // namespace editor_chart_load_service
