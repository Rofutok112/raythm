#include <cassert>

#include "title/ranking_panel_view.h"

int main() {
    title_ranking_view::draw_config config;

    config.online_sources_available = true;
    assert(title_ranking_view::source_available(config, ranking_service::source::local));
    assert(title_ranking_view::source_available(config, ranking_service::source::online));
    assert(title_ranking_view::source_available(config, ranking_service::source::friends));

    config.online_sources_available = false;
    assert(title_ranking_view::source_available(config, ranking_service::source::local));
    assert(!title_ranking_view::source_available(config, ranking_service::source::online));
    assert(!title_ranking_view::source_available(config, ranking_service::source::friends));

    return 0;
}
