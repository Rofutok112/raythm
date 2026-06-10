#pragma once

#include <future>
#include <functional>
#include <string>

#include "load_progress.h"
#include "ranking_service.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_ranking_loader.h"
#include "song_select/song_select_state.h"

namespace song_select {

struct catalog_reload_request {
    std::string preferred_song_id;
    std::string preferred_chart_id;
    bool calculate_missing_levels = false;
};

struct catalog_reload_result {
    bool completed = false;
    bool queued_reload_started = false;
    bool selection_changed = false;
    bool failed = false;
    std::string message;
};

class data_controller {
public:
    using catalog_loader = std::function<catalog_data(bool calculate_missing_levels,
                                                      catalog_progress_callback progress)>;
    using ranking_loader = std::function<ranking_service::listing(std::string chart_id,
                                                                  ranking_service::source source,
                                                                  int limit)>;

    data_controller();
    data_controller(catalog_loader catalog_loader_fn, ranking_loader ranking_loader_fn);
    static catalog_data load_catalog_from_service(bool calculate_missing_levels,
                                                  catalog_progress_callback progress);
    static ranking_service::listing load_ranking_from_service(std::string chart_id,
                                                              ranking_service::source source,
                                                              int limit);

    void reset(state& state);

    [[nodiscard]] bool catalog_loading() const;
    [[nodiscard]] bool ranking_loading() const;
    [[nodiscard]] load_progress catalog_progress() const;

    void request_catalog_reload(state& state, catalog_reload_request request = {});
    catalog_reload_result poll_catalog_reload(state& state);

    void request_ranking_reload(state& state);
    ranking_reload_result poll_ranking_reload(state& state);

private:
    void start_catalog_load(state& state, catalog_reload_request request);

    catalog_loader catalog_loader_;
    ranking_load_controller ranking_controller_;

    shared_load_progress catalog_progress_;
    std::future<catalog_data> catalog_future_;
    bool catalog_loading_ = false;
    bool catalog_reload_pending_ = false;
    catalog_reload_request active_catalog_request_;
    catalog_reload_request queued_catalog_request_;
};

}  // namespace song_select
