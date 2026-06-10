#pragma once

#include <string>

#include "title/catalog_reload_policy.h"

class title_play_create_feature;

class title_catalog_reload_coordinator {
public:
    void reset();
    void mark_level_refresh_covered();

    void request_reload(title_play_create_feature& feature,
                        std::string preferred_song_id,
                        std::string preferred_chart_id,
                        title_catalog::reload_policy policy);

    void request_background_rebuild_if_due(title_play_create_feature& feature,
                                           bool startup_loading,
                                           bool content_mode_is_play_or_create);

private:
    bool background_level_refresh_requested_ = false;
};
