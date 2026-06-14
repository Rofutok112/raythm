#pragma once

#include "ranking_service.h"
#include "song_select/song_select_state.h"

namespace song_select::ranking_source_policy {

struct availability {
    bool online_sources_available = false;
};

[[nodiscard]] ranking_service::source personal_best_source_for_chart(const chart_option* chart);

inline availability availability_for_chart(const chart_option* chart) {
    return {
        .online_sources_available = chart != nullptr && can_use_online_chart_routes(*chart),
    };
}

inline bool source_available(const availability& availability, ranking_service::source source) {
    return source == ranking_service::source::local || availability.online_sources_available;
}

inline ranking_service::source effective_source(const availability& availability,
                                                ranking_service::source requested) {
    return source_available(availability, requested) ? requested : ranking_service::source::local;
}

}  // namespace song_select::ranking_source_policy
