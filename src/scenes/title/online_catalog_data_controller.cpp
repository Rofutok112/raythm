#include "title/online_catalog_data_controller.h"

namespace online_catalog {

data_controller::data_controller() = default;

std::future<title_online_view::catalog_load_result>& data_controller::catalog_future() {
    return catalog_future_;
}

std::future<title_online_view::remote_song_page_fetch_result>& data_controller::song_page_future() {
    return song_page_future_;
}

std::future<title_online_view::remote_chart_page_fetch_result>& data_controller::chart_page_future() {
    return chart_page_future_;
}

std::future<std::vector<title_online_view::song_entry_state>>& data_controller::owned_future() {
    return owned_future_;
}

std::future<title_online_view::download_song_result>& data_controller::download_future() {
    return download_future_;
}

}  // namespace online_catalog
