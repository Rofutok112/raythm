#pragma once

#include <future>
#include <optional>
#include <string>

#include "ranking_service.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_state.h"
#include "title/create_upload_client.h"

class title_play_data_controller {
public:
    struct catalog_poll_result {
        bool sync_play_media = false;
        bool sync_create_preview = false;
    };

    struct upload_poll_result {
        bool refresh_catalog = false;
    };

    void reset(song_select::state& state);

    [[nodiscard]] bool upload_in_progress() const;

    void request_catalog_reload(song_select::state& state,
                                std::string preferred_song_id = "",
                                std::string preferred_chart_id = "",
                                bool sync_media_on_apply = false,
                                bool calculate_missing_levels = false);
    catalog_poll_result poll_catalog_reload(song_select::state& state,
                                            bool play_mode_active,
                                            bool create_mode_active);

    void request_ranking_reload(song_select::state& state);
    void poll_ranking_reload(song_select::state& state);

    void request_scoring_ruleset_warm(bool force_refresh = false);
    void poll_scoring_ruleset_warm();

    void start_song_upload(const song_select::song_entry& song);
    void start_chart_upload(const song_select::song_entry& song,
                            const song_select::chart_option& chart);
    upload_poll_result poll_create_upload(song_select::state& state);

private:
    void start_catalog_load(song_select::state& state,
                            std::string preferred_song_id,
                            std::string preferred_chart_id,
                            bool sync_media_on_apply,
                            bool calculate_missing_levels);
    void start_ranking_load(song_select::state& state,
                            std::string chart_id,
                            ranking_service::source source);

    std::future<song_select::catalog_data> catalog_future_;
    bool catalog_loading_ = false;
    bool catalog_reload_pending_ = false;
    bool catalog_sync_media_on_apply_ = false;
    bool queued_catalog_sync_media_on_apply_ = false;
    bool catalog_calculate_missing_levels_ = false;
    bool queued_catalog_calculate_missing_levels_ = false;
    std::string catalog_song_id_;
    std::string catalog_chart_id_;
    std::string queued_catalog_song_id_;
    std::string queued_catalog_chart_id_;

    std::future<ranking_service::listing> ranking_future_;
    bool ranking_loading_ = false;
    bool ranking_reload_pending_ = false;
    int ranking_generation_ = 0;
    int ranking_pending_generation_ = 0;

    std::future<bool> scoring_ruleset_future_;
    bool scoring_ruleset_loading_ = false;

    std::future<title_create_upload::upload_result> upload_future_;
    bool upload_in_progress_ = false;
};
