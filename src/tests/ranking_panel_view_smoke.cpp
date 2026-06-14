#include <cassert>

#include "title/ranking_panel_view.h"

int main() {
    title_ranking_view::draw_config config;

    config.source_availability.online_sources_available = true;
    assert(title_ranking_view::source_available(config, ranking_service::source::local));
    assert(title_ranking_view::source_available(config, ranking_service::source::online));
    assert(title_ranking_view::source_available(config, ranking_service::source::friends));
    assert(song_select::ranking_source_policy::effective_source(
               config.source_availability,
               ranking_service::source::friends) == ranking_service::source::friends);

    config.source_availability.online_sources_available = false;
    assert(title_ranking_view::source_available(config, ranking_service::source::local));
    assert(!title_ranking_view::source_available(config, ranking_service::source::online));
    assert(!title_ranking_view::source_available(config, ranking_service::source::friends));
    assert(song_select::ranking_source_policy::effective_source(
               config.source_availability,
               ranking_service::source::friends) == ranking_service::source::local);

    return 0;
}
