#pragma once

#include <string>
#include <vector>

#include "network/unlock_rule_client.h"
#include "song_select/song_select_state.h"

namespace title_create_upload {

struct upload_result {
    bool success = false;
    bool maintenance = false;
    bool should_refresh_online_catalog = false;
    std::string message;
    std::string retry_after;
    std::string remote_song_id;
    std::string remote_chart_id;
    std::string local_chart_id;
};

upload_result upload_song(const song_select::song_entry& song);
upload_result upload_chart(const song_select::song_entry& song,
                           const song_select::chart_option& chart,
                           const std::vector<unlock_rule_client::rule>& unlock_rules = {});
bool refresh_song_edit_permission(const std::string& remote_song_id);
bool refresh_chart_edit_permission(const std::string& remote_chart_id);

}  // namespace title_create_upload
