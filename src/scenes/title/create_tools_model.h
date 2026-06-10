#pragma once

#include <optional>
#include <string>
#include <vector>

#include "song_select/song_select_state.h"
#include "title/local_content_index.h"

namespace title_create_tools_model {

enum class action {
    create_song,
    edit_song,
    import_song,
    export_song,
    upload_song,
    create_chart,
    edit_chart,
    import_chart,
    export_chart,
    upload_chart,
    edit_mv,
};

struct entry {
    std::string title;
    std::string detail;
    action command = action::create_song;
    bool enabled = true;
    bool primary = false;
};

struct section {
    std::string title;
    std::vector<entry> entries;
};

struct bindings {
    std::optional<local_content_index::online_song_binding> song;
    std::optional<local_content_index::online_chart_binding> chart;
};

struct view_model {
    std::vector<section> sections;
    bool song_upload_enabled = false;
    bool chart_upload_enabled = false;
};

struct build_context {
    const song_select::song_entry* song = nullptr;
    const song_select::chart_option* chart = nullptr;
    std::string server_url;
    bool online_status_checking = false;
    bindings upload_bindings;
    std::optional<bool> song_permission_hint;
    std::optional<bool> chart_permission_hint;
};

bindings resolve_bindings(const local_content_index::snapshot& index,
                          const song_select::song_entry* song,
                          const song_select::chart_option* chart,
                          const std::string& server_url);

view_model build(const build_context& context);
bool action_enabled(const view_model& model, action command);

}  // namespace title_create_tools_model
