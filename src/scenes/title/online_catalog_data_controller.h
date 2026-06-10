#pragma once

#include <future>
#include <string>
#include <vector>

#include "title/online_download_view.h"

namespace online_catalog {

class data_controller {
public:
    data_controller();

    [[nodiscard]] std::future<title_online_view::catalog_load_result>& catalog_future();
    [[nodiscard]] std::future<title_online_view::remote_song_page_fetch_result>& song_page_future();
    [[nodiscard]] std::future<title_online_view::remote_chart_page_fetch_result>& chart_page_future();
    [[nodiscard]] std::future<std::vector<title_online_view::song_entry_state>>& owned_future();
    [[nodiscard]] std::future<title_online_view::download_song_result>& download_future();

private:
    std::future<title_online_view::catalog_load_result> catalog_future_;
    std::future<title_online_view::remote_song_page_fetch_result> song_page_future_;
    std::future<title_online_view::remote_chart_page_fetch_result> chart_page_future_;
    std::future<std::vector<title_online_view::song_entry_state>> owned_future_;
    std::future<title_online_view::download_song_result> download_future_;
};

}  // namespace online_catalog
