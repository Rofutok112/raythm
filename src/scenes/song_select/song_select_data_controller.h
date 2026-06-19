#pragma once

#include <future>
#include <functional>
#include <memory>
#include <string>

#include "load_progress.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_state.h"

namespace song_select {

struct catalog_reload_request {
    std::string preferred_song_id;
    std::string preferred_chart_id;
    bool calculate_missing_levels = false;
    bool preserve_current_selection = false;
};

struct catalog_reload_result {
    bool completed = false;
    bool queued_reload_started = false;
    bool stale = false;
    bool chart_levels_updated = false;
    bool selection_changed = false;
    bool failed = false;
    std::string message;
};

class data_controller {
public:
    using catalog_loader = std::function<catalog_data(bool calculate_missing_levels,
                                                      catalog_progress_callback progress)>;

    data_controller();
    explicit data_controller(catalog_loader catalog_loader_fn);
    static catalog_data load_catalog_from_service(bool calculate_missing_levels,
                                                  catalog_progress_callback progress);

    void reset(state& state);

    [[nodiscard]] bool catalog_loading() const;
    [[nodiscard]] load_progress catalog_progress() const;

    void request_catalog_reload(state& state, catalog_reload_request request = {});
    catalog_reload_result poll_catalog_reload(state& state);

private:
    void start_catalog_load(state& state, catalog_reload_request request);

    catalog_loader catalog_loader_;

    std::shared_ptr<shared_load_progress> catalog_progress_ = std::make_shared<shared_load_progress>();
    std::future<catalog_data> catalog_future_;
    bool catalog_loading_ = false;
    bool catalog_reload_pending_ = false;
    catalog_reload_request active_catalog_request_;
    catalog_reload_request queued_catalog_request_;
};

}  // namespace song_select
