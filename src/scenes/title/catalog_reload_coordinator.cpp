#include "title/catalog_reload_coordinator.h"

#include <utility>

#include "title/title_play_create_feature.h"

void title_catalog_reload_coordinator::reset() {
    background_level_refresh_requested_ = false;
}

void title_catalog_reload_coordinator::mark_level_refresh_covered() {
    background_level_refresh_requested_ = true;
}

void title_catalog_reload_coordinator::request_reload(title_play_create_feature& feature,
                                                      std::string preferred_song_id,
                                                      std::string preferred_chart_id,
                                                      title_catalog::reload_policy policy) {
    if (policy.calculate_missing_levels) {
        mark_level_refresh_covered();
    }
    feature.request_catalog_reload(std::move(preferred_song_id), std::move(preferred_chart_id), policy);
}

void title_catalog_reload_coordinator::request_background_rebuild_if_due(
    title_play_create_feature& feature,
    bool startup_loading,
    bool content_mode_is_play_or_create) {
    if (startup_loading ||
        !feature.state().catalog_loaded_once ||
        feature.catalog_loading() ||
        background_level_refresh_requested_ ||
        content_mode_is_play_or_create) {
        return;
    }

    mark_level_refresh_covered();
    feature.capture_current_selection();
    feature.request_catalog_reload(
        feature.preferred_song_id(),
        feature.preferred_chart_id(),
        title_catalog::policy_for(title_catalog::reload_mode::background_rebuild));
}
