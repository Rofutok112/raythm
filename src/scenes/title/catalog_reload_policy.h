#pragma once

namespace title_catalog {

enum class reload_mode {
    quiet_refresh,
    fast_startup,
    selection_sync,
    background_rebuild,
    scoring_ruleset_warmed,
    user_refresh,
    content_changed,
    import_completed,
    transfer_completed,
};

struct reload_policy {
    reload_mode mode = reload_mode::quiet_refresh;
    bool sync_media_on_apply = false;
    bool calculate_missing_levels = false;
};

inline reload_policy policy_for(reload_mode mode, bool sync_media_on_apply = false) {
    switch (mode) {
        case reload_mode::quiet_refresh:
            return {mode, false, false};
        case reload_mode::fast_startup:
            return {mode, sync_media_on_apply, false};
        case reload_mode::selection_sync:
            return {mode, true, false};
        case reload_mode::background_rebuild:
            return {mode, false, true};
        case reload_mode::scoring_ruleset_warmed:
            return {mode, false, false};
        case reload_mode::user_refresh:
        case reload_mode::content_changed:
        case reload_mode::import_completed:
            return {mode, sync_media_on_apply, true};
        case reload_mode::transfer_completed:
            return {mode, sync_media_on_apply, false};
    }
    return {mode, sync_media_on_apply, false};
}

}  // namespace title_catalog
