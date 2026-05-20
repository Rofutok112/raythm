#pragma once

#include <future>
#include <string>

#include "ranking_service.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_state.h"

namespace song_select {

class data_controller {
public:
    struct catalog_reload_request {
        std::string preferred_song_id;
        std::string preferred_chart_id;
        bool sync_media_on_apply = false;
        bool calculate_missing_levels = false;
    };

    struct catalog_poll_result {
        bool applied = false;
        bool sync_media = false;
    };

    struct ranking_poll_result {
        bool applied = false;
        bool stale = false;
    };

    void reset(state& state);

    [[nodiscard]] bool catalog_loading() const;
    [[nodiscard]] bool ranking_loading() const;

    void request_catalog_reload(state& state);
    void request_catalog_reload(state& state, catalog_reload_request request);
    catalog_poll_result poll_catalog_reload(state& state);

    void request_ranking_reload(state& state);
    ranking_poll_result poll_ranking_reload(state& state);

private:
    void start_catalog_load(state& state, catalog_reload_request request);
    void start_ranking_load(std::string chart_id, ranking_service::source source);

    std::future<catalog_data> catalog_future_;
    bool catalog_loading_ = false;
    bool catalog_reload_pending_ = false;
    catalog_reload_request active_catalog_request_;
    catalog_reload_request queued_catalog_request_;

    std::future<ranking_service::listing> ranking_future_;
    bool ranking_loading_ = false;
    bool ranking_reload_pending_ = false;
    int ranking_generation_ = 0;
    int ranking_pending_generation_ = 0;
};

}  // namespace song_select
