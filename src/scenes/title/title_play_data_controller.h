#pragma once

#include <future>
#include <optional>
#include <string>
#include <vector>

#include "ranking_service.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_data_controller.h"
#include "song_select/song_select_state.h"
#include "title/create_upload_client.h"
#include "network/unlock_rule_client.h"

class title_play_data_controller {
public:
    struct catalog_poll_result {
        bool completed = false;
        bool queued_reload_started = false;
        bool stale = false;
        bool chart_levels_updated = false;
        bool selection_changed = false;
    };

    struct upload_poll_result {
        bool completed = false;
        bool success = false;
        bool refresh_catalog = false;
        bool chart_uploaded = false;
        std::string local_chart_id;
        std::string remote_chart_id;
    };

    title_play_data_controller();

    void reset(song_select::state& state);

    [[nodiscard]] bool catalog_loading() const;
    [[nodiscard]] load_progress catalog_progress() const;
    [[nodiscard]] bool scoring_ruleset_loading() const;
    [[nodiscard]] bool upload_in_progress() const;

    void request_catalog_reload(song_select::state& state,
                                std::string preferred_song_id = "",
                                std::string preferred_chart_id = "",
                                bool calculate_missing_levels = false,
                                bool preserve_current_selection = false);
    catalog_poll_result poll_catalog_reload(song_select::state& state);

    void request_scoring_ruleset_warm(bool force_refresh = false);
    bool poll_scoring_ruleset_warm();

    void start_song_upload(const song_select::song_entry& song);
    void start_chart_upload(const song_select::song_entry& song,
                            const song_select::chart_option& chart,
                            std::vector<unlock_rule_client::rule> unlock_rules = {});
    upload_poll_result poll_create_upload(song_select::state& state);

private:
    song_select::data_controller data_controller_;

    std::future<bool> scoring_ruleset_future_;
    bool scoring_ruleset_loading_ = false;

    std::future<title_create_upload::upload_result> upload_future_;
    bool upload_in_progress_ = false;
};
