#pragma once

#include <future>
#include <functional>
#include <optional>
#include <string>

#include "ranking_service.h"

namespace online_catalog {

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
        std::optional<std::string> chart_id;
        std::optional<ranking_service::listing> listing;
    };

    struct poll_result {
        bool completed = false;
        bool stale = false;
        std::optional<ranking_service::listing> loaded;
    };

    using listing_loader = std::function<ranking_service::listing(std::string chart_id,
                                                                  ranking_service::source source,
                                                                  int limit)>;

    ranking_load_controller();
    explicit ranking_load_controller(listing_loader loader);

    void reset();
    [[nodiscard]] bool loading() const;
    [[nodiscard]] bool request(std::string chart_id);
    [[nodiscard]] poll_result poll(const std::string& current_chart_id);
    [[nodiscard]] snapshot current() const;

private:
    void start_load(std::string chart_id);

    listing_loader loader_;
    std::future<ranking_service::listing> future_;
    std::optional<std::string> active_chart_id_;
    std::optional<std::string> loaded_chart_id_;
    std::optional<ranking_service::listing> loaded_listing_;
    load_status status_ = load_status::idle;
};

}  // namespace online_catalog
