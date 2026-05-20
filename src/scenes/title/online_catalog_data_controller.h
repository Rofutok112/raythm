#pragma once

#include <future>
#include <functional>
#include <string>
#include <vector>

#include "ranking_service.h"
#include "title/online_download_view.h"

namespace online_catalog {

class data_controller {
public:
    using ranking_loader = std::function<ranking_service::listing(std::string chart_id,
                                                                  ranking_service::source source,
                                                                  int limit)>;

    data_controller();
    explicit data_controller(ranking_loader ranking_loader_fn);

    void reset();
    [[nodiscard]] std::future<title_online_view::catalog_load_result>& catalog_future();
    [[nodiscard]] std::future<title_online_view::remote_song_page_fetch_result>& song_page_future();
    [[nodiscard]] std::future<title_online_view::remote_chart_page_fetch_result>& chart_page_future();
    [[nodiscard]] std::future<std::vector<title_online_view::song_entry_state>>& owned_future();
    [[nodiscard]] std::future<title_online_view::download_song_result>& download_future();

    void request_selected_chart_ranking(title_online_view::state& state);
    void poll_selected_chart_ranking(title_online_view::state& state);

private:
    [[nodiscard]] std::string selected_ranking_chart_id(const title_online_view::state& state) const;
    void set_select_chart_message(title_online_view::state& state);
    void start_ranking_load(title_online_view::state& state, std::string chart_id);

    ranking_loader ranking_loader_;
    std::future<title_online_view::catalog_load_result> catalog_future_;
    std::future<title_online_view::remote_song_page_fetch_result> song_page_future_;
    std::future<title_online_view::remote_chart_page_fetch_result> chart_page_future_;
    std::future<std::vector<title_online_view::song_entry_state>> owned_future_;
    std::future<title_online_view::download_song_result> download_future_;
    std::future<ranking_service::listing> ranking_future_;
    bool ranking_loading_ = false;
    std::string ranking_requested_chart_id_;
    std::string ranking_loaded_chart_id_;
};

}  // namespace online_catalog
