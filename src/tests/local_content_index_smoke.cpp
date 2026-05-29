#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#include "app_paths.h"
#include "local_catalog_signature.h"
#include "local_sqlite.h"
#include "title/local_content_index.h"

#include "sqlite3.h"

namespace {
bool set_local_app_data(const std::filesystem::path& path) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", path.string().c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", path.string().c_str(), 1) == 0;
#endif
}

bool seed_catalog_cache(const std::string& signature) {
    local_sqlite::database database = local_sqlite::open_local_catalog_cache_database();
    if (!database.valid()) {
        return false;
    }
    const bool ready =
        local_sqlite::exec(database.get(), "DROP TABLE IF EXISTS local_songs;") &&
        local_sqlite::exec(database.get(), "DROP TABLE IF EXISTS local_charts;") &&
        local_sqlite::exec(database.get(),
                           "CREATE TABLE local_songs ("
                           "song_id TEXT PRIMARY KEY,"
                           "storage_policy TEXT NOT NULL"
                           ");") &&
        local_sqlite::exec(database.get(),
                           "CREATE TABLE local_charts ("
                           "chart_id TEXT PRIMARY KEY,"
                           "storage_policy TEXT NOT NULL"
                           ");") &&
        local_sqlite::exec(database.get(), "INSERT INTO local_songs(song_id, storage_policy) VALUES('local-song', 'managed_package');") &&
        local_sqlite::exec(database.get(), "INSERT INTO local_songs(song_id, storage_policy) VALUES('plain-local-song', 'plain_workspace');") &&
        local_sqlite::exec(database.get(), "INSERT INTO local_charts(chart_id, storage_policy) VALUES('local-chart', 'managed_package');") &&
        local_sqlite::exec(database.get(), "INSERT INTO local_charts(chart_id, storage_policy) VALUES('chart-without-song', 'managed_package');") &&
        local_sqlite::exec(database.get(), "INSERT INTO local_charts(chart_id, storage_policy) VALUES('plain-local-chart', 'plain_workspace');");
    if (!ready) {
        return false;
    }
    local_sqlite::put_metadata(database.get(), "local_catalog.signature", signature);
    local_sqlite::put_metadata(database.get(), "local_catalog.status_schema", local_catalog_signature::kStatusSchema);
    return true;
}

}

int main() {
    const std::filesystem::path appdata_root =
        std::filesystem::temp_directory_path() / "raythm-local-content-index-smoke";
    std::error_code ec;
    std::filesystem::remove_all(appdata_root, ec);
    if (!set_local_app_data(appdata_root)) {
        std::cerr << "failed to set LOCALAPPDATA\n";
        return EXIT_FAILURE;
    }

    {
        local_sqlite::database legacy_content_database(app_paths::local_content_db_path());
        if (!legacy_content_database.valid() ||
            !local_sqlite::ensure_common_schema(legacy_content_database.get()) ||
            !local_sqlite::exec(legacy_content_database.get(), "CREATE TABLE local_songs(song_id TEXT);") ||
            !local_sqlite::exec(legacy_content_database.get(), "CREATE TABLE local_charts(chart_id TEXT);")) {
            std::cerr << "failed to prepare legacy local content database\n";
            return EXIT_FAILURE;
        }
        local_sqlite::put_metadata(legacy_content_database.get(), "local_catalog.signature", "legacy-cache");
    }

    local_content_index::put_song_binding({
        .server_url = "https://server.example",
        .local_song_id = "local-song",
        .remote_song_id = "remote-song",
        .origin = local_content_index::online_origin::owned_upload,
        .can_edit = true,
        .lifecycle_status = "active",
        .review_status = "pending_review",
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "local-chart",
        .remote_chart_id = "remote-chart",
        .remote_song_id = "remote-song",
        .remote_chart_version = 7,
        .origin = local_content_index::online_origin::downloaded,
        .can_edit = false,
        .lifecycle_status = "archived",
        .review_status = "rejected",
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "other-local-chart",
        .remote_chart_id = "other-remote-chart",
        .remote_song_id = "other-remote-song",
        .remote_chart_version = 2,
        .origin = local_content_index::online_origin::downloaded,
    });

    const auto song_by_local =
        local_content_index::find_song_by_local("https://server.example", "local-song");
    const auto song_by_remote =
        local_content_index::find_song_by_remote("https://server.example", "remote-song");
    const auto chart_by_local =
        local_content_index::find_chart_by_local("https://server.example", "local-chart");
    const auto chart_by_remote =
        local_content_index::find_chart_by_remote("https://server.example", "remote-chart");

    bool ok = true;
    ok = song_by_local.has_value() && song_by_local->remote_song_id == "remote-song" && ok;
    ok = song_by_local.has_value() && song_by_local->can_edit == std::optional<bool>(true) && ok;
    ok = song_by_local.has_value() && song_by_local->lifecycle_status == "active" && ok;
    ok = song_by_local.has_value() && song_by_local->review_status == "pending_review" && ok;
    ok = song_by_remote.has_value() && song_by_remote->local_song_id == "local-song" && ok;
    ok = chart_by_local.has_value() && chart_by_local->remote_chart_id == "remote-chart" && ok;
    ok = chart_by_remote.has_value() && chart_by_remote->local_chart_id == "local-chart" && ok;
    ok = chart_by_local.has_value() &&
         chart_by_local->remote_chart_version == 7 &&
         chart_by_local->origin == local_content_index::online_origin::downloaded &&
         chart_by_local->can_edit == std::optional<bool>(false) &&
         chart_by_local->lifecycle_status == "archived" &&
         chart_by_local->review_status == "rejected" && ok;

    local_content_index::put_song_binding({
        .server_url = "https://server.example",
        .local_song_id = "managed-local-song",
        .remote_song_id = "remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "managed-local-chart",
        .remote_chart_id = "remote-chart",
        .remote_song_id = "remote-song",
        .remote_chart_version = 7,
        .origin = local_content_index::online_origin::downloaded,
    });
    const auto duplicate_song_by_local =
        local_content_index::find_song_by_local("https://server.example", "managed-local-song");
    const auto duplicate_chart_by_local =
        local_content_index::find_chart_by_local("https://server.example", "managed-local-chart");
    ok = duplicate_song_by_local.has_value() && duplicate_song_by_local->remote_song_id == "remote-song" && ok;
    ok = duplicate_chart_by_local.has_value() &&
         duplicate_chart_by_local->remote_chart_id == "remote-chart" && ok;

    {
        local_sqlite::database content_database = local_sqlite::open_local_content_database();
        local_sqlite::statement legacy_song_cache(
            content_database.get(),
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'local_songs';");
        local_sqlite::statement legacy_chart_cache(
            content_database.get(),
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'local_charts';");
        ok = legacy_song_cache.valid() && sqlite3_step(legacy_song_cache.get()) != SQLITE_ROW && ok;
        ok = legacy_chart_cache.valid() && sqlite3_step(legacy_chart_cache.get()) != SQLITE_ROW && ok;
        ok = std::filesystem::exists(app_paths::local_content_db_path()) && ok;
        ok = !std::filesystem::exists(app_paths::local_catalog_cache_db_path()) && ok;
    }

    local_content_index::remove_chart_bindings_for_remote_song("https://server.example", "remote-song");
    ok = !local_content_index::find_chart_by_local("https://server.example", "local-chart").has_value() && ok;
    ok = !local_content_index::find_chart_by_local("https://server.example", "managed-local-chart").has_value() && ok;
    ok = local_content_index::find_chart_by_local("https://server.example", "other-local-chart").has_value() && ok;

    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "local-chart",
        .remote_chart_id = "remote-chart",
        .remote_song_id = "remote-song",
        .remote_chart_version = 8,
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_song_binding({
        .server_url = "https://server.example",
        .local_song_id = "orphan-song",
        .remote_song_id = "orphan-remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "orphan-chart",
        .remote_chart_id = "orphan-remote-chart",
        .remote_song_id = "remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "chart-without-song",
        .remote_chart_id = "chart-without-song-remote",
        .remote_song_id = "missing-remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_song_binding({
        .server_url = "https://server.example",
        .local_song_id = "new-local-song",
        .remote_song_id = "new-remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "new-local-chart",
        .remote_chart_id = "new-remote-chart",
        .remote_song_id = "new-remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_song_binding({
        .server_url = "https://server.example",
        .local_song_id = "plain-local-song",
        .remote_song_id = "plain-remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = "https://server.example",
        .local_chart_id = "plain-local-chart",
        .remote_chart_id = "plain-remote-chart",
        .remote_song_id = "plain-remote-song",
        .origin = local_content_index::online_origin::downloaded,
    });

    ok = seed_catalog_cache("stale-test-cache") && ok;
    const local_content_index::snapshot stale_cache = local_content_index::load_snapshot();
    ok = stale_cache.songs.size() == 5 && stale_cache.charts.size() == 6 && ok;
    ok = local_content_index::find_song_by_local("https://server.example", "new-local-song").has_value() && ok;
    ok = local_content_index::find_chart_by_local("https://server.example", "new-local-chart").has_value() && ok;

    ok = seed_catalog_cache(local_catalog_signature::current()) && ok;
    const local_content_index::snapshot pruned = local_content_index::load_snapshot();
    ok = pruned.songs.size() == 1 && pruned.charts.size() == 1 && ok;
    ok = local_content_index::find_song_by_local("https://server.example", "local-song").has_value() && ok;
    ok = local_content_index::find_chart_by_local("https://server.example", "local-chart").has_value() && ok;
    ok = !local_content_index::find_song_by_local("https://server.example", "orphan-song").has_value() && ok;
    ok = !local_content_index::find_chart_by_local("https://server.example", "orphan-chart").has_value() && ok;
    ok = !local_content_index::find_chart_by_local("https://server.example", "chart-without-song").has_value() && ok;
    ok = !local_content_index::find_chart_by_local("https://server.example", "other-local-chart").has_value() && ok;
    ok = !local_content_index::find_song_by_local("https://server.example", "new-local-song").has_value() && ok;
    ok = !local_content_index::find_chart_by_local("https://server.example", "new-local-chart").has_value() && ok;
    ok = !local_content_index::find_song_by_local("https://server.example", "plain-local-song").has_value() && ok;
    ok = !local_content_index::find_chart_by_local("https://server.example", "plain-local-chart").has_value() && ok;

    std::filesystem::remove_all(appdata_root, ec);
    if (!ok) {
        std::cerr << "local_content_index smoke test failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "local_content_index smoke test passed\n";
    return EXIT_SUCCESS;
}
