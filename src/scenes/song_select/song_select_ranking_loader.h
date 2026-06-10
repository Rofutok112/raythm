#pragma once

#include <future>
#include <functional>
#include <optional>
#include <string>

#include "ranking_service.h"
#include "song_select/song_select_state.h"

namespace song_select {

struct ranking_reload_result {
    bool completed = false;
    bool stale = false;
    bool queued_reload_started = false;
};

class ranking_load_controller {
public:
    enum class load_status {
        idle,
        loading,
        ready,
        failed,
    };

    using listing_loader = std::function<ranking_service::listing(std::string chart_id,
                                                                  ranking_service::source source,
                                                                  int limit)>;

    ranking_load_controller();
    explicit ranking_load_controller(listing_loader loader);

    void reset(state& state);
    [[nodiscard]] bool loading() const;
    [[nodiscard]] load_status status() const;

    void request_reload(state& state);
    ranking_reload_result poll(state& state);

private:
    struct request {
        std::string chart_id;
        ranking_service::source source = ranking_service::source::local;
        ranking_service::source best_source = ranking_service::source::local;
        bool refresh_best = false;
    };

    struct load_data {
        ranking_service::listing listing;
        ranking_service::source best_source = ranking_service::source::local;
        std::string best_chart_id;
        bool best_refreshed = false;
        std::optional<ranking_service::entry> best_entry;
    };

    request build_request(const state& state) const;
    [[nodiscard]] bool active_request_covers(const request& next) const;
    [[nodiscard]] bool loaded_request_covers(const request& next) const;
    void start_load(request next);
    void apply_loaded(state& state, load_data loaded);
    void mark_online_loading(state& state, ranking_service::source source) const;
    void reset_panel_scroll(state& state) const;

    listing_loader loader_;
    std::future<load_data> future_;
    std::optional<request> active_request_;
    std::optional<request> queued_request_;
    std::optional<request> loaded_request_;
    load_status status_ = load_status::idle;
};

}  // namespace song_select
