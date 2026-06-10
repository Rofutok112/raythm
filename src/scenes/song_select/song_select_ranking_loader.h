#pragma once

#include <future>
#include <functional>
#include <optional>
#include <string>

#include "ranking_service.h"
#include "song_select/selection_key.h"

namespace song_select {

struct ranking_load_request {
    selection_key key;
    ranking_service::source best_source = ranking_service::source::local;
    bool refresh_best = false;
};

struct ranking_load_data {
    ranking_service::listing listing;
    ranking_service::source best_source = ranking_service::source::local;
    std::string best_chart_id;
    bool best_refreshed = false;
    std::optional<ranking_service::entry> best_entry;
};

struct ranking_request_result {
    enum class action {
        none,
        started,
        queued,
    };

    action reload_action = action::none;
    std::optional<ranking_load_request> accepted_request;
};

struct ranking_reload_result {
    bool completed = false;
    bool stale = false;
    bool queued_reload_started = false;
    std::optional<ranking_load_data> loaded;
    std::optional<ranking_load_request> started_request;
};

class ranking_load_controller {
public:
    enum class load_status {
        idle,
        loading,
        ready,
        failed,
    };

    struct snapshot {
        load_status status = load_status::idle;
        std::optional<selection_key> key;
        std::optional<ranking_load_data> data;
    };

    using listing_loader = std::function<ranking_service::listing(std::string chart_id,
                                                                  ranking_service::source source,
                                                                  int limit)>;

    ranking_load_controller();
    explicit ranking_load_controller(listing_loader loader);

    void reset();
    [[nodiscard]] bool loading() const;
    [[nodiscard]] load_status status() const;
    [[nodiscard]] snapshot current() const;

    ranking_request_result request_reload(ranking_load_request request);
    ranking_reload_result poll(const ranking_load_request& current_request);

private:
    [[nodiscard]] bool active_request_covers(const ranking_load_request& next) const;
    [[nodiscard]] bool loaded_request_covers(const ranking_load_request& next) const;
    [[nodiscard]] bool loaded_best_covers(const ranking_load_request& next) const;
    void start_load(ranking_load_request next);

    listing_loader loader_;
    std::future<ranking_load_data> future_;
    std::optional<ranking_load_request> active_request_;
    std::optional<ranking_load_request> queued_request_;
    std::optional<ranking_load_request> loaded_request_;
    std::optional<selection_key> snapshot_key_;
    std::optional<ranking_load_data> loaded_data_;
    ranking_service::source loaded_best_source_ = ranking_service::source::local;
    std::string loaded_best_chart_id_;
    bool loaded_best_ = false;
    load_status status_ = load_status::idle;
};

[[nodiscard]] ranking_load_controller::snapshot ranking_snapshot_for_key(
    ranking_load_controller::snapshot snapshot,
    const selection_key& key);

}  // namespace song_select
