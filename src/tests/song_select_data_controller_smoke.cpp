#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "network/auth_client.h"
#include "song_select/song_select_data_controller.h"
#include "song_select/song_select_navigation.h"
#include "song_select/song_select_ranking_loader.h"
#include "title/online_catalog_ranking_loader.h"

namespace {

void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ":" << line << ": check failed: " << expression << "\n";
        std::exit(EXIT_FAILURE);
    }
}

#undef assert
#define assert(expr) check((expr), #expr, __FILE__, __LINE__)

bool logged_in = true;
ranking_service::source last_loaded_source = ranking_service::source::local;

song_select::chart_option make_chart(const char* chart_id) {
    song_select::chart_option chart;
    chart.meta.chart_id = chart_id;
    chart.meta.difficulty = "Normal";
    chart.meta.level = 5;
    chart.meta.key_count = 4;
    if (std::string(chart_id) == "chart-level") {
        chart.storage = storage_policy::managed_package;
        chart.remote_links.push_back(online_content::chart_identity{
            .server_url = "https://api.example",
            .remote_song_id = "remote-song",
            .remote_chart_id = "remote-chart",
            .content_source = online_content::source::community,
            .remote_chart_version = 1,
        });
    }
    return chart;
}

song_select::song_entry make_song(const char* song_id, const char* chart_id) {
    song_select::song_entry song;
    song.song.meta.song_id = song_id;
    song.song.meta.title = song_id;
    song.charts.push_back(make_chart(chart_id));
    return song;
}

bool has_queueable_link_for_server(const song_select::chart_option& chart, const std::string& server_url) {
    if (!song_select::can_use_online_chart_routes(chart)) {
        return false;
    }
    std::string normalized = auth::normalize_server_url(server_url);
    if (normalized.empty()) {
        return false;
    }
    if (online_content::is_queueable(chart.online_identity) &&
        auth::normalize_server_url(chart.online_identity->server_url) == normalized) {
        return true;
    }
    for (const online_content::chart_identity& link : chart.remote_links) {
        if (online_content::is_queueable(link) &&
            auth::normalize_server_url(link.server_url) == normalized) {
            return true;
        }
    }
    return false;
}

bool uses_submitted_ranking_best(const song_select::chart_option* chart) {
    if (chart == nullptr || !song_select::can_use_online_chart_routes(*chart)) {
        return false;
    }
    if (chart->source == content_source::official ||
            chart->source == content_source::community) {
        return true;
    }

    const auth::session_summary summary = auth::load_session_summary();
    return summary.logged_in && has_queueable_link_for_server(*chart, summary.server_url);
}

song_select::ranking_load_request make_ranking_request(const song_select::state& state) {
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);

    song_select::ranking_load_request request;
    request.key = song_select::selection_key_for_state(state);
    request.best_source = uses_submitted_ranking_best(chart)
        ? ranking_service::source::online
        : ranking_service::source::local;
    return request;
}

void apply_ranking_request_started(song_select::state& state,
                                   const song_select::ranking_load_request& request) {
    if (request.refresh_best) {
        state.ranking_panel.best_source = request.best_source;
        state.ranking_panel.best_chart_id = request.key.chart_id;
        state.ranking_panel.best_loaded = false;
        state.ranking_panel.best_entry.reset();
    }
    if (request.key.source == ranking_service::source::online) {
        state.ranking_panel.listing = {};
        state.ranking_panel.listing.ranking_source = request.key.source;
        state.ranking_panel.listing.available = false;
        state.ranking_panel.listing.message = "ランキング読み込み中...";
    }
}

void apply_ranking_loaded(song_select::state& state,
                          song_select::ranking_load_data loaded) {
    state.ranking_panel.listing = std::move(loaded.listing);
    if (loaded.best_refreshed) {
        state.ranking_panel.best_source = loaded.best_source;
        state.ranking_panel.best_chart_id = loaded.best_chart_id;
        state.ranking_panel.best_entry = std::move(loaded.best_entry);
        state.ranking_panel.best_loaded = true;
    }
    state.ranking_panel.reveal_anim = 0.0f;
}

song_select::ranking_request_result request_ranking(song_select::ranking_load_controller& controller,
                                                    song_select::state& state) {
    const song_select::ranking_load_request request = make_ranking_request(state);
    const song_select::ranking_request_result result = controller.request_reload(request);
    if (result.accepted_request.has_value()) {
        apply_ranking_request_started(state, *result.accepted_request);
    }
    return result;
}

song_select::ranking_reload_result poll_ranking(song_select::ranking_load_controller& controller,
                                                song_select::state& state) {
    const song_select::ranking_load_request request = make_ranking_request(state);
    song_select::ranking_reload_result result = controller.poll(request);
    if (result.loaded.has_value()) {
        apply_ranking_loaded(state, std::move(*result.loaded));
    }
    if (result.started_request.has_value()) {
        apply_ranking_request_started(state, *result.started_request);
    }
    return result;
}

template <typename Fn>
void spin_until(Fn fn) {
    for (int i = 0; i < 200; ++i) {
        if (fn()) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(false);
}

}  // namespace

namespace song_select {

catalog_data load_catalog(bool, catalog_progress_callback) {
    return {};
}

}  // namespace song_select

namespace ranking_service {

source last_personal_best_source = source::local;
int personal_best_load_count = 0;

listing load_chart_ranking(const std::string&, source ranking_source, int) {
    listing result;
    result.ranking_source = ranking_source;
    return result;
}

std::optional<entry> load_chart_personal_best(const std::string&, source best_source) {
    ++personal_best_load_count;
    last_personal_best_source = best_source;
    return std::nullopt;
}

}  // namespace ranking_service

namespace auth {

std::string normalize_server_url(const std::string& server_url) {
    std::string normalized = server_url;
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

session_summary load_session_summary() {
    session_summary summary;
    summary.logged_in = logged_in;
    summary.server_url = "https://api.example";
    return summary;
}

}  // namespace auth

int main() {
    int catalog_load_count = 0;
    int ranking_load_count = 0;
    song_select::data_controller controller(
        [&](bool calculate_missing_levels, song_select::catalog_progress_callback progress) {
            ++catalog_load_count;
            if (progress) {
                progress("Loading test catalog...", 0.5f, true);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            song_select::catalog_data catalog;
            catalog.songs.push_back(make_song(catalog_load_count == 1 ? "song-a" : "song-b",
                                              calculate_missing_levels ? "chart-level" : "chart-normal"));
            return catalog;
        });
    song_select::ranking_load_controller ranking_controller(
        [&](std::string chart_id, ranking_service::source source, int) {
            ++ranking_load_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            last_loaded_source = source;
            ranking_service::listing listing;
            listing.available = true;
            listing.ranking_source = source;
            listing.message = chart_id;
            return listing;
        });

    song_select::state state;
    controller.request_catalog_reload(state, {"song-a", "chart-normal", false});
    assert(state.catalog_loading);
    spin_until([&] {
        return controller.catalog_progress().message == "Loading test catalog...";
    });
    assert(controller.catalog_progress().active);
    controller.request_catalog_reload(state, {"song-b", "chart-level", true});
    controller.request_catalog_reload(state, {"song-b", "chart-level", false});

    spin_until([&] {
        const song_select::catalog_reload_result result = controller.poll_catalog_reload(state);
        if (!result.completed || !result.queued_reload_started) {
            return false;
        }
        assert(result.stale);
        return true;
    });
    assert(!state.catalog_loaded_once);
    assert(song_select::selected_song(state) == nullptr);

    spin_until([&] {
        const song_select::catalog_reload_result result = controller.poll_catalog_reload(state);
        if (!result.completed) {
            return false;
        }
        assert(!result.stale);
        return true;
    });
    assert(catalog_load_count == 2);
    assert(!state.catalog_loading);
    assert(!controller.catalog_progress().active);
    assert(song_select::selected_song(state)->song.meta.song_id == "song-b");
    assert(song_select::selected_chart_for(state, song_select::filtered_charts_for_selected_song(state))
               ->meta.chart_id == "chart-level");
    song_select::selection_key selected_key = song_select::selection_key_for_state(state);
    assert(selected_key.song_id == "song-b");
    assert(selected_key.chart_id == "chart-level");
    assert(selected_key.source == ranking_service::source::local);

    state.ranking_panel.selected_source = ranking_service::source::online;
    selected_key = song_select::selection_key_for_state(state);
    assert(selected_key.song_id == "song-b");
    assert(selected_key.chart_id == "chart-level");
    assert(selected_key.source == ranking_service::source::online);
    const song_select::selection_key selected_media_key = song_select::song_media_key_for(selected_key);
    assert(selected_media_key.song_id == "song-b");
    assert(selected_media_key.chart_id.empty());
    assert(selected_media_key.source == ranking_service::source::local);
    assert(selected_media_key == song_select::song_media_key_for_song_id("song-b"));
    logged_in = false;
    request_ranking(ranking_controller, state);
    spin_until([&] {
        return poll_ranking(ranking_controller, state).completed;
    });
    assert(ranking_service::last_personal_best_source == ranking_service::source::local);

    logged_in = true;
    request_ranking(ranking_controller, state);
    song_select::ranking_load_controller::snapshot ranking_snapshot = ranking_controller.current();
    assert(ranking_snapshot.status == song_select::ranking_load_controller::load_status::loading);
    assert(ranking_snapshot.key.has_value());
    assert(*ranking_snapshot.key == song_select::selection_key_for_state(state));
    assert(!ranking_snapshot.data.has_value());
    assert(state.ranking_panel.listing.message == "ランキング読み込み中...");
    request_ranking(ranking_controller, state);

    spin_until([&] {
        const song_select::ranking_reload_result result = poll_ranking(ranking_controller, state);
        return result.completed && !result.stale && !result.queued_reload_started;
    });
    assert(state.ranking_panel.listing.available);
    assert(state.ranking_panel.listing.message == "chart-level");
    assert(last_loaded_source == ranking_service::source::online);
    assert(ranking_service::last_personal_best_source == ranking_service::source::online);
    ranking_snapshot = ranking_controller.current();
    assert(ranking_snapshot.status == song_select::ranking_load_controller::load_status::ready);
    assert(ranking_snapshot.key.has_value());
    assert(*ranking_snapshot.key == song_select::selection_key_for_state(state));
    assert(ranking_snapshot.data.has_value());
    assert(ranking_snapshot.data->listing.message == "chart-level");
    song_select::selection_key other_key = song_select::selection_key_for_state(state);
    other_key.song_id = "other-song";
    const song_select::ranking_load_controller::snapshot filtered_other =
        song_select::ranking_snapshot_for_key(ranking_snapshot, other_key);
    assert(filtered_other.status == song_select::ranking_load_controller::load_status::idle);
    assert(!filtered_other.key.has_value());
    const song_select::ranking_load_controller::snapshot filtered_current =
        song_select::ranking_snapshot_for_key(ranking_snapshot, song_select::selection_key_for_state(state));
    assert(filtered_current.status == song_select::ranking_load_controller::load_status::ready);
    assert(filtered_current.key.has_value());
    const int loaded_ranking_count = ranking_load_count;
    const int loaded_best_count = ranking_service::personal_best_load_count;
    state.ranking_panel.reveal_anim = 1.0f;
    request_ranking(ranking_controller, state);
    assert(!ranking_controller.loading());
    assert(ranking_load_count == loaded_ranking_count);
    assert(ranking_service::personal_best_load_count == loaded_best_count);
    assert(state.ranking_panel.reveal_anim == 1.0f);

    state.ranking_panel.selected_source = ranking_service::source::local;
    request_ranking(ranking_controller, state);
    assert(ranking_controller.loading());
    state.ranking_panel.selected_source = ranking_service::source::online;
    request_ranking(ranking_controller, state);

    spin_until([&] {
        const song_select::ranking_reload_result result = poll_ranking(ranking_controller, state);
        return result.completed && result.stale && result.queued_reload_started;
    });
    assert(state.ranking_panel.listing.message == "ランキング読み込み中...");

    spin_until([&] {
        const song_select::ranking_reload_result result = poll_ranking(ranking_controller, state);
        return result.completed && !result.stale && !result.queued_reload_started;
    });
    assert(state.ranking_panel.listing.available);
    assert(state.ranking_panel.listing.message == "chart-level");
    assert(last_loaded_source == ranking_service::source::online);

    song_select::catalog_data same_chart_catalog;
    same_chart_catalog.songs.push_back(make_song("same-chart-song-a", "shared-chart"));
    same_chart_catalog.songs.push_back(make_song("same-chart-song-b", "shared-chart"));
    song_select::state same_chart_state;
    song_select::apply_catalog(same_chart_state, std::move(same_chart_catalog), "same-chart-song-a", "shared-chart");
    same_chart_state.ranking_panel.selected_source = ranking_service::source::local;
    request_ranking(ranking_controller, same_chart_state);
    spin_until([&] {
        return poll_ranking(ranking_controller, same_chart_state).completed;
    });
    assert(same_chart_state.ranking_panel.listing.available);
    const int same_chart_first_load_count = ranking_load_count;
    same_chart_state.selected_song_index = 1;
    same_chart_state.difficulty_index = 0;
    request_ranking(ranking_controller, same_chart_state);
    assert(ranking_controller.loading());
    spin_until([&] {
        return poll_ranking(ranking_controller, same_chart_state).completed;
    });
    assert(ranking_load_count == same_chart_first_load_count + 1);
    assert(same_chart_state.ranking_panel.listing.available);
    assert(same_chart_state.ranking_panel.listing.message == "shared-chart");

    state.ranking_panel.reveal_anim = 0.9f;
    state.ranking_panel.selected_source = ranking_service::source::local;
    request_ranking(ranking_controller, state);
    assert(ranking_controller.loading());
    state.ranking_panel.selected_source = ranking_service::source::online;
    spin_until([&] {
        const song_select::ranking_reload_result result = poll_ranking(ranking_controller, state);
        return result.completed && result.stale && !result.queued_reload_started;
    });
    assert(!ranking_controller.loading());
    assert(ranking_controller.status() == song_select::ranking_load_controller::load_status::idle);
    ranking_snapshot = ranking_controller.current();
    assert(ranking_snapshot.status == song_select::ranking_load_controller::load_status::idle);
    assert(!ranking_snapshot.key.has_value());
    assert(state.ranking_panel.listing.available);
    assert(state.ranking_panel.listing.ranking_source == ranking_service::source::online);
    assert(state.ranking_panel.reveal_anim == 0.9f);

    int failing_ranking_load_count = 0;
    song_select::ranking_load_controller failing_ranking_controller(
        [&](std::string, ranking_service::source, int) -> ranking_service::listing {
            ++failing_ranking_load_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            throw std::runtime_error("ranking failed");
        });
    song_select::state failing_ranking_state;
    song_select::catalog_data failing_ranking_catalog;
    failing_ranking_catalog.songs.push_back(make_song("failing-ranking-song", "failing-ranking-chart"));
    song_select::apply_catalog(
        failing_ranking_state,
        std::move(failing_ranking_catalog),
        "failing-ranking-song",
        "failing-ranking-chart");
    request_ranking(failing_ranking_controller, failing_ranking_state);
    spin_until([&] {
        return poll_ranking(failing_ranking_controller, failing_ranking_state).completed;
    });
    assert(failing_ranking_controller.status() == song_select::ranking_load_controller::load_status::failed);
    assert(failing_ranking_load_count == 1);
    song_select::ranking_load_controller::snapshot failing_snapshot = failing_ranking_controller.current();
    assert(failing_snapshot.status == song_select::ranking_load_controller::load_status::failed);
    assert(failing_snapshot.key.has_value());
    assert(*failing_snapshot.key == song_select::selection_key_for_state(failing_ranking_state));
    assert(failing_snapshot.data.has_value());
    assert(failing_snapshot.data->listing.message == "ranking failed");
    request_ranking(failing_ranking_controller, failing_ranking_state);
    assert(!failing_ranking_controller.loading());
    assert(failing_ranking_load_count == 1);

    state.ranking_panel.reveal_anim = 0.75f;
    state.song_change_anim_t = 0.25f;
    state.chart_change_anim_t = 0.5f;
    controller.request_catalog_reload(state, {"song-b", "chart-level", true});
    spin_until([&] {
        const song_select::catalog_reload_result result = controller.poll_catalog_reload(state);
        if (!result.completed) {
            return false;
        }
        assert(!result.selection_changed);
        return true;
    });
    assert(state.ranking_panel.listing.available);
    assert(state.ranking_panel.listing.message == "chart-level");
    assert(state.ranking_panel.reveal_anim == 0.75f);
    assert(state.song_change_anim_t == 0.25f);
    assert(state.chart_change_anim_t == 0.5f);

    song_select::state preserve_state;
    song_select::catalog_data preserve_initial_catalog;
    preserve_initial_catalog.songs.push_back(make_song("preserve-song-a", "preserve-chart-a"));
    preserve_initial_catalog.songs.push_back(make_song("preserve-song-b", "preserve-chart-b"));
    song_select::apply_catalog(
        preserve_state,
        std::move(preserve_initial_catalog),
        "preserve-song-a",
        "preserve-chart-a");
    int preserve_reload_count = 0;
    song_select::data_controller preserve_controller(
        [&](bool, song_select::catalog_progress_callback) {
            ++preserve_reload_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            song_select::catalog_data catalog;
            catalog.songs.push_back(make_song("preserve-song-a", "preserve-chart-a"));
            catalog.songs.push_back(make_song("preserve-song-b", "preserve-chart-b"));
            return catalog;
        });
    preserve_controller.request_catalog_reload(
        preserve_state,
        {"preserve-song-a", "preserve-chart-a", true, true});
    assert(song_select::apply_song_selection(preserve_state, 1, 0));
    spin_until([&] {
        const song_select::catalog_reload_result result =
            preserve_controller.poll_catalog_reload(preserve_state);
        if (!result.completed) {
            return false;
        }
        assert(result.chart_levels_updated);
        assert(!result.selection_changed);
        return true;
    });
    assert(preserve_reload_count == 1);
    assert(song_select::selected_song(preserve_state)->song.meta.song_id == "preserve-song-b");
    assert(song_select::selected_chart_for(preserve_state, song_select::filtered_charts_for_selected_song(preserve_state))
               ->meta.chart_id == "preserve-chart-b");

    song_select::state queued_preserve_state;
    song_select::catalog_data queued_preserve_initial_catalog;
    queued_preserve_initial_catalog.songs.push_back(make_song("queued-preserve-song-a", "queued-preserve-chart-a"));
    queued_preserve_initial_catalog.songs.push_back(make_song("queued-preserve-song-b", "queued-preserve-chart-b"));
    song_select::apply_catalog(
        queued_preserve_state,
        std::move(queued_preserve_initial_catalog),
        "queued-preserve-song-a",
        "queued-preserve-chart-a");
    int queued_preserve_reload_count = 0;
    song_select::data_controller queued_preserve_controller(
        [&](bool, song_select::catalog_progress_callback) {
            ++queued_preserve_reload_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            song_select::catalog_data catalog;
            catalog.songs.push_back(make_song("queued-preserve-song-a", "queued-preserve-chart-a"));
            catalog.songs.push_back(make_song("queued-preserve-song-b", "queued-preserve-chart-b"));
            return catalog;
        });
    queued_preserve_controller.request_catalog_reload(
        queued_preserve_state,
        {"queued-preserve-song-a", "queued-preserve-chart-a", false, false});
    queued_preserve_controller.request_catalog_reload(
        queued_preserve_state,
        {"queued-preserve-song-a", "queued-preserve-chart-a", true, true});
    queued_preserve_controller.request_catalog_reload(
        queued_preserve_state,
        {"queued-preserve-song-a", "queued-preserve-chart-a", false, false});
    spin_until([&] {
        const song_select::catalog_reload_result result =
            queued_preserve_controller.poll_catalog_reload(queued_preserve_state);
        if (!result.completed || !result.queued_reload_started) {
            return false;
        }
        assert(result.stale);
        return true;
    });
    assert(song_select::apply_song_selection(queued_preserve_state, 1, 0));
    spin_until([&] {
        const song_select::catalog_reload_result result =
            queued_preserve_controller.poll_catalog_reload(queued_preserve_state);
        if (!result.completed) {
            return false;
        }
        assert(result.chart_levels_updated);
        assert(!result.selection_changed);
        return true;
    });
    assert(queued_preserve_reload_count == 2);
    assert(song_select::selected_song(queued_preserve_state)->song.meta.song_id == "queued-preserve-song-b");
    assert(song_select::selected_chart_for(
               queued_preserve_state,
               song_select::filtered_charts_for_selected_song(queued_preserve_state))
               ->meta.chart_id == "queued-preserve-chart-b");

    int source_reload_count = 0;
    song_select::data_controller source_controller(
        [&](bool, song_select::catalog_progress_callback) {
            ++source_reload_count;
            song_select::catalog_data catalog;
            catalog.songs.push_back(make_song("source-song", "source-chart"));
            song_select::chart_option& chart = catalog.songs.back().charts.back();
            if (source_reload_count == 1) {
                chart.storage = storage_policy::managed_package;
                chart.remote_links.push_back(online_content::chart_identity{
                    .server_url = "https://api.example",
                    .remote_song_id = "source-remote-song",
                    .remote_chart_id = "source-remote-chart",
                    .content_source = online_content::source::community,
                    .remote_chart_version = 1,
                });
            }
            return catalog;
        });
    song_select::state source_state;
    source_controller.request_catalog_reload(source_state, {"source-song", "source-chart", false});
    spin_until([&] {
        return source_controller.poll_catalog_reload(source_state).completed;
    });
    source_state.ranking_panel.selected_source = ranking_service::source::online;
    assert(song_select::selection_key_for_state(source_state).source == ranking_service::source::online);
    source_state.ranking_panel.selected_source = ranking_service::source::friends;
    assert(song_select::selection_key_for_state(source_state).source == ranking_service::source::friends);
    source_controller.request_catalog_reload(source_state, {"source-song", "source-chart", false});
    spin_until([&] {
        const song_select::catalog_reload_result result = source_controller.poll_catalog_reload(source_state);
        if (!result.completed) {
            return false;
        }
        assert(result.selection_changed);
        return true;
    });
    assert(song_select::selection_key_for_state(source_state).source == ranking_service::source::local);
    source_state.ranking_panel.selected_source = ranking_service::source::friends;
    assert(song_select::selection_key_for_state(source_state).source == ranking_service::source::local);

    song_select::catalog_data legacy_catalog;
    legacy_catalog.songs.push_back(make_song("legacy-song", "legacy-chart"));
    legacy_catalog.songs.back().charts.back().remote_links.push_back(online_content::chart_identity{
        .server_url = "https://api.example",
        .remote_song_id = "legacy-remote-song",
        .remote_chart_id = "legacy-remote-chart",
        .content_source = online_content::source::community,
        .remote_chart_version = 1,
    });
    song_select::state legacy_state;
    song_select::apply_catalog(legacy_state, std::move(legacy_catalog), "legacy-song", "legacy-chart");
    legacy_state.ranking_panel.selected_source = ranking_service::source::friends;
    const song_select::selection_key legacy_key = song_select::selection_key_for_state(legacy_state);
    assert(legacy_key.song_id == "legacy-song");
    assert(legacy_key.chart_id == "legacy-chart");
    assert(legacy_key.source == ranking_service::source::local);
    request_ranking(ranking_controller, legacy_state);
    spin_until([&] {
        return poll_ranking(ranking_controller, legacy_state).completed;
    });
    assert(legacy_state.ranking_panel.selected_source == ranking_service::source::friends);
    assert(legacy_state.ranking_panel.listing.ranking_source == ranking_service::source::local);
    assert(last_loaded_source == ranking_service::source::local);
    assert(ranking_service::last_personal_best_source == ranking_service::source::local);

    int online_ranking_load_count = 0;
    online_catalog::ranking_load_controller online_ranking_controller(
        [&](std::string chart_id, ranking_service::source source, int limit) {
            ++online_ranking_load_count;
            assert(source == ranking_service::source::online);
            assert(limit == 10);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ranking_service::listing listing;
            listing.available = true;
            listing.ranking_source = source;
            listing.message = std::move(chart_id);
            return listing;
        });
    assert(online_ranking_controller.request("online-chart-a"));
    assert(online_ranking_controller.loading());
    online_catalog::ranking_load_controller::snapshot online_snapshot = online_ranking_controller.current();
    assert(online_snapshot.status == online_catalog::ranking_load_controller::load_status::loading);
    assert(online_snapshot.chart_id.has_value());
    assert(*online_snapshot.chart_id == "online-chart-a");
    assert(!online_ranking_controller.request("online-chart-a"));
    spin_until([&] {
        const online_catalog::ranking_load_controller::poll_result result =
            online_ranking_controller.poll("online-chart-a");
        return result.completed && !result.stale && result.loaded.has_value();
    });
    assert(online_ranking_load_count == 1);
    online_snapshot = online_ranking_controller.current();
    assert(online_snapshot.status == online_catalog::ranking_load_controller::load_status::ready);
    assert(online_snapshot.chart_id.has_value());
    assert(*online_snapshot.chart_id == "online-chart-a");
    assert(online_snapshot.listing.has_value());
    assert(online_snapshot.listing->message == "online-chart-a");
    assert(!online_ranking_controller.request("online-chart-a"));
    assert(online_ranking_load_count == 1);
    assert(online_ranking_controller.request("online-chart-b"));
    spin_until([&] {
        const online_catalog::ranking_load_controller::poll_result result =
            online_ranking_controller.poll("online-chart-c");
        return result.completed && result.stale && !result.loaded.has_value();
    });
    assert(!online_ranking_controller.loading());
    online_snapshot = online_ranking_controller.current();
    assert(online_snapshot.status == online_catalog::ranking_load_controller::load_status::idle);
    assert(!online_snapshot.chart_id.has_value());

    return 0;
}
