#include <cstdlib>
#include <iostream>
#include <string>

#include "title/title_startup_controller.h"

namespace {

void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ":" << line << ": check failed: " << expression << "\n";
        std::exit(EXIT_FAILURE);
    }
}

#undef assert
#define assert(expr) check((expr), #expr, __FILE__, __LINE__)

}  // namespace

int main() {
    title_startup_controller::state startup;
    title_startup_controller::reset(startup);

    song_select::state play_state;
    std::string home_status;
    bool requested = false;
    title_catalog::reload_policy requested_policy;
    std::string requested_song_id;
    std::string requested_chart_id;
    bool online_reloaded = false;
    bool scoring_warm_requested = false;

    title_startup_controller::update(startup, {
        play_state,
        "preferred-song",
        "preferred-chart",
        true,
        home_status,
        [&](std::string song_id, std::string chart_id, title_catalog::reload_policy policy) {
            requested = true;
            requested_song_id = std::move(song_id);
            requested_chart_id = std::move(chart_id);
            requested_policy = policy;
        },
        [] {
            return false;
        },
        [&] {
            online_reloaded = true;
        },
        [] {},
        [&](bool force_refresh) {
            scoring_warm_requested = !force_refresh;
        },
        [] {
            return false;
        },
    });

    assert(requested);
    assert(requested_song_id == "preferred-song");
    assert(requested_chart_id == "preferred-chart");
    assert(requested_policy.mode == title_catalog::reload_mode::fast_startup);
    assert(requested_policy.sync_media_on_apply);
    assert(!requested_policy.calculate_missing_levels);
    assert(startup.catalog_requested);
    assert(startup.loading_message == "Loading local catalog...");

    title_startup_controller::update(startup, {
        play_state,
        "",
        "",
        false,
        home_status,
        [](std::string, std::string, title_catalog::reload_policy) {},
        [] {
            return true;
        },
        [] {},
        [] {},
        [](bool) {},
        [] {
            return false;
        },
        [] {
            return load_progress{
                .message = "Loading local charts 2/8...",
                .progress = 0.35f,
                .active = true,
            };
        },
    });
    assert(startup.loading_message == "Loading local charts 2/8...");
    assert(startup.catalog_progress == 0.35f);

    play_state.catalog_loaded_once = true;
    title_startup_controller::update(startup, {
        play_state,
        "",
        "",
        false,
        home_status,
        [](std::string, std::string, title_catalog::reload_policy) {},
        [] {
            return false;
        },
        [&] {
            online_reloaded = true;
        },
        [] {},
        [&](bool force_refresh) {
            scoring_warm_requested = !force_refresh;
        },
        [] {
            return false;
        },
    });
    assert(startup.fonts_preload_started);

    startup.fonts_preloaded = true;
    title_startup_controller::update(startup, {
        play_state,
        "",
        "",
        false,
        home_status,
        [](std::string, std::string, title_catalog::reload_policy) {},
        [] {
            return false;
        },
        [&] {
            online_reloaded = true;
        },
        [] {},
        [&](bool force_refresh) {
            scoring_warm_requested = !force_refresh;
        },
        [] {
            return false;
        },
    });

    assert(startup.load_complete);
    assert(!startup.load_failed);
    assert(startup.loading_message == "Ready.");
    assert(home_status.empty());
    assert(online_reloaded);
    assert(scoring_warm_requested);

    std::cout << "title_startup_controller smoke test passed\n";
    return EXIT_SUCCESS;
}
