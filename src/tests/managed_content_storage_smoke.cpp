#include "managed_content_storage.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app_paths.h"
#include "chart_serializer.h"
#include "editor/service/editor_chart_load_service.h"
#include "local_catalog_signature.h"
#include "mv/mv_storage.h"
#include "path_utils.h"
#include "player_note_offsets.h"
#include "song_select/local_catalog_database.h"
#include "ranking_service.h"
#include "song_select/song_catalog_service.h"
#include "song_writer.h"
#include "title/local_content_database.h"
#include "title/local_content_index.h"

namespace fs = std::filesystem;

namespace {

bool set_local_app_data(const fs::path& path) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", path.string().c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", path.string().c_str(), 1) == 0;
#endif
}

void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ":" << line << ": check failed: " << expression << "\n";
        std::exit(EXIT_FAILURE);
    }
}

#undef assert
#define assert(expr) check((expr), #expr, __FILE__, __LINE__)

song_meta make_song_meta(std::string song_id, std::string title) {
    song_meta meta;
    meta.song_id = std::move(song_id);
    meta.title = std::move(title);
    meta.artist = "Codex";
    meta.base_bpm = 128.0f;
    meta.audio_file = "audio.ogg";
    meta.jacket_file = "jacket.png";
    meta.preview_start_ms = 0;
    meta.song_version = 1;
    return meta;
}

chart_data make_chart(std::string chart_id, std::string song_id) {
    chart_data chart;
    chart.meta.chart_id = std::move(chart_id);
    chart.meta.song_id = std::move(song_id);
    chart.meta.key_count = 4;
    chart.meta.difficulty = "Managed";
    chart.meta.chart_author = "Codex";
    chart.meta.format_version = 1;
    chart.meta.resolution = 480;
    chart.timing_events = {
        {.type = timing_event_type::bpm, .tick = 0, .bpm = 128.0f, .numerator = 4, .denominator = 4},
        {.type = timing_event_type::meter, .tick = 0, .bpm = 0.0f, .numerator = 4, .denominator = 4},
    };
    chart.notes = {
        {.type = note_type::tap, .tick = 0, .lane = 0, .end_tick = 0},
    };
    return chart;
}

void touch_assets(const fs::path& song_dir) {
    std::ofstream(song_dir / "audio.ogg", std::ios::binary) << "audio";
    std::ofstream(song_dir / "jacket.png", std::ios::binary) << "jacket";
}

}  // namespace

namespace mv {

std::vector<mv_package> load_all_packages() {
    return {};
}

}  // namespace mv

namespace ranking_service {

listing load_chart_ranking(const std::string&, source ranking_source, int) {
    listing result;
    result.ranking_source = ranking_source;
    return result;
}

}  // namespace ranking_service

player_chart_offset_map load_player_chart_offsets() {
    return {};
}

int main() {
    const fs::path temp_root = fs::temp_directory_path() / "raythm-managed-content-storage-smoke";
    std::error_code ec;
    fs::remove_all(temp_root, ec);
    assert(set_local_app_data(temp_root));

    app_paths::ensure_directories();

    const fs::path legacy_song_dir = app_paths::song_dir("remote-song");
    assert(song_writer::write_song_json(make_song_meta("remote-song", "Legacy Local"), legacy_song_dir.string()));
    touch_assets(legacy_song_dir);

    const managed_content_storage::song_identity song_identity{
        .source = online_content::source::community,
        .server_url = "https://server.example/api",
        .remote_song_id = "remote-song",
        .song_version = 2,
        .revision_id = "song-rev-2",
        .package_id = "package-remote-song",
    };
    const managed_content_storage::chart_identity chart_identity{
        .source = online_content::source::community,
        .server_url = song_identity.server_url,
        .remote_song_id = song_identity.remote_song_id,
        .remote_chart_id = "remote-chart",
        .song_version = song_identity.song_version,
        .chart_version = 5,
        .revision_id = "chart-rev-5",
        .chart_hash = "local-chart-sha",
        .chart_fingerprint = "local-chart-fingerprint",
        .remote_chart_hash = "remote-chart-sha",
        .remote_chart_fingerprint = "remote-chart-fingerprint",
    };

    const std::string managed_song_id = managed_content_storage::local_song_id(song_identity);
    const std::string managed_chart_id = managed_content_storage::local_chart_id(chart_identity);
    assert(managed_song_id != "remote-song");
    assert(managed_chart_id != "remote-chart");

    const fs::path managed_song_dir = managed_content_storage::song_directory(song_identity);
    const song_meta managed_meta = make_song_meta(managed_song_id, "Managed Remote");
    const chart_data managed_chart = make_chart(managed_chart_id, managed_song_id);
    const std::string managed_song_json = song_writer::serialize_song_json(managed_meta);
    const std::string managed_chart_text = chart_serializer::serialize_to_string(managed_chart);

    managed_content_storage::package_manifest manifest{
        .song = song_identity,
        .local_song_id = managed_song_id,
        .song_json_hash = "local-song-json-sha",
        .song_json_fingerprint = "local-song-json-fingerprint",
        .audio_hash = "local-audio-sha",
        .jacket_hash = "local-jacket-sha",
        .remote_song_json_hash = "remote-song-json-sha",
        .remote_song_json_fingerprint = "remote-song-json-fingerprint",
        .remote_audio_hash = "remote-audio-sha",
        .remote_jacket_hash = "remote-jacket-sha",
    };
    managed_content_storage::upsert_chart(manifest, chart_identity);
    std::string error_message;
    assert(managed_content_storage::write_encrypted_asset(
        manifest, managed_song_dir, "song.json", managed_song_json, manifest.song_json_asset, error_message));
    assert(managed_content_storage::write_encrypted_asset(
        manifest, managed_song_dir, managed_meta.audio_file, std::string_view("audio", 5), manifest.audio_asset,
        error_message));
    assert(managed_content_storage::write_encrypted_asset(
        manifest, managed_song_dir, managed_meta.jacket_file, std::string_view("jacket", 6), manifest.jacket_asset,
        error_message));
    managed_content_storage::chart_manifest_entry* encrypted_chart = nullptr;
    for (managed_content_storage::chart_manifest_entry& chart : manifest.charts) {
        if (chart.local_chart_id == managed_chart_id) {
            encrypted_chart = &chart;
            break;
        }
    }
    assert(encrypted_chart != nullptr);
    assert(managed_content_storage::write_encrypted_asset(
        manifest,
        managed_song_dir,
        path_utils::to_utf8(fs::path("charts") / (managed_chart_id + ".rchart")),
        managed_chart_text,
        encrypted_chart->encrypted_chart,
        error_message));
    assert(managed_content_storage::write_manifest(manifest, error_message));
    assert(!fs::exists(managed_song_dir / "song.json"));
    assert(!fs::exists(managed_song_dir / "audio.ogg"));
    assert(!fs::exists(managed_song_dir / "jacket.png"));
    assert(!fs::exists(managed_song_dir / "charts" / (managed_chart_id + ".rchart")));
    const std::optional<managed_content_storage::package_manifest> stored_manifest =
        managed_content_storage::read_manifest(managed_song_dir);
    assert(stored_manifest.has_value());
    assert(stored_manifest->song.source == online_content::source::community);
    assert(stored_manifest->song.package_id == "package-remote-song");
    assert(stored_manifest->song_json_hash == "local-song-json-sha");
    assert(stored_manifest->song_json_fingerprint == "local-song-json-fingerprint");
    assert(stored_manifest->audio_hash == "local-audio-sha");
    assert(stored_manifest->jacket_hash == "local-jacket-sha");
    assert(stored_manifest->remote_song_json_hash == "remote-song-json-sha");
    assert(stored_manifest->remote_song_json_fingerprint == "remote-song-json-fingerprint");
    assert(stored_manifest->remote_audio_hash == "remote-audio-sha");
    assert(stored_manifest->remote_jacket_hash == "remote-jacket-sha");
    assert(!stored_manifest->key_id.empty());
    assert(stored_manifest->content_key_version == 1);
    assert(std::string(stored_manifest->encryption_scheme) == managed_content_storage::default_encryption_scheme());
    assert(!stored_manifest->song_json_asset.ciphertext_hash.empty());
    assert(!stored_manifest->audio_asset.ciphertext_hash.empty());
    assert(!stored_manifest->jacket_asset.ciphertext_hash.empty());
    assert(!stored_manifest->song_json_asset.content_hash.empty());
    assert(!stored_manifest->created_at.empty());
    assert(!stored_manifest->updated_at.empty());
    assert(stored_manifest->charts.size() == 1);
    assert(stored_manifest->charts.front().chart_hash == "local-chart-sha");
    assert(stored_manifest->charts.front().chart_fingerprint == "local-chart-fingerprint");
    assert(stored_manifest->charts.front().remote_chart_hash == "remote-chart-sha");
    assert(stored_manifest->charts.front().remote_chart_fingerprint == "remote-chart-fingerprint");
    assert(!stored_manifest->charts.front().encrypted_chart.ciphertext_hash.empty());

    const chart_parse_result editor_loaded_chart = editor_chart_load_service::load_chart(
        path_utils::to_utf8(managed_content_storage::chart_file_path(managed_song_dir, managed_chart_id)));
    assert(editor_loaded_chart.success);
    assert(editor_loaded_chart.data.has_value());
    assert(editor_loaded_chart.data->meta.chart_id == managed_chart_id);

    const managed_content_storage::managed_file_read_result decrypted_song_json =
        managed_content_storage::read_managed_file(managed_song_dir / "song.json");
    assert(decrypted_song_json.managed);
    assert(decrypted_song_json.success);
    assert(std::string(decrypted_song_json.bytes.begin(), decrypted_song_json.bytes.end()).find("Managed Remote") !=
           std::string::npos);

    managed_content_storage::package_manifest revoked_manifest = *stored_manifest;
    revoked_manifest.license_revoked = true;
    assert(managed_content_storage::write_manifest(revoked_manifest, error_message));
    const managed_content_storage::managed_file_read_result revoked_read =
        managed_content_storage::read_managed_file(managed_song_dir / "song.json");
    assert(revoked_read.managed);
    assert(!revoked_read.success);
    revoked_manifest.license_revoked = false;
    assert(managed_content_storage::write_manifest(revoked_manifest, error_message));

    local_content_index::put_song_binding({
        .server_url = song_identity.server_url,
        .local_song_id = song_identity.remote_song_id,
        .remote_song_id = song_identity.remote_song_id,
        .origin = local_content_index::online_origin::downloaded,
    });
    local_content_index::put_chart_binding({
        .server_url = song_identity.server_url,
        .local_chart_id = chart_identity.remote_chart_id,
        .remote_chart_id = chart_identity.remote_chart_id,
        .remote_song_id = chart_identity.remote_song_id,
        .remote_chart_version = chart_identity.chart_version,
        .origin = local_content_index::online_origin::downloaded,
    });

    const song_select::catalog_data catalog_rebuilt_from_manifest = song_select::load_catalog(true);
    const song_select::song_entry* manifest_managed = nullptr;
    for (const song_select::song_entry& song : catalog_rebuilt_from_manifest.songs) {
        if (song.song.meta.song_id == managed_song_id) {
            manifest_managed = &song;
            break;
        }
    }
    assert(manifest_managed != nullptr);
    assert(manifest_managed->online_identity.has_value());
    assert(!manifest_managed->charts.empty());
    assert(manifest_managed->charts.front().online_identity.has_value());
    assert(manifest_managed->charts.front().online_identity->remote_chart_id == "remote-chart");

    const std::optional<local_content_index::online_song_binding> manifest_song_binding =
        local_content_index::find_song_by_local(song_identity.server_url, managed_song_id);
    assert(manifest_song_binding.has_value());
    assert(manifest_song_binding->remote_song_id == song_identity.remote_song_id);
    const std::optional<local_content_index::online_chart_binding> manifest_chart_binding =
        local_content_index::find_chart_by_local(song_identity.server_url, managed_chart_id);
    assert(manifest_chart_binding.has_value());
    assert(manifest_chart_binding->remote_chart_id == chart_identity.remote_chart_id);
    assert(manifest_chart_binding->remote_song_id == chart_identity.remote_song_id);
    assert(manifest_chart_binding->remote_chart_version == chart_identity.chart_version);

    const std::string extra_chart_id = "local-extra-chart";
    const chart_data extra_chart = make_chart(extra_chart_id, managed_song_id);
    std::error_code extra_chart_ec;
    fs::create_directories(managed_song_dir / "charts", extra_chart_ec);
    assert(!extra_chart_ec);
    assert(chart_serializer::serialize(
        extra_chart, path_utils::to_utf8(managed_content_storage::chart_file_path(managed_song_dir, extra_chart_id))));

    const song_select::catalog_data catalog = song_select::load_catalog(true);
    assert(catalog.songs.size() == 2);

    const song_select::song_entry* legacy = nullptr;
    const song_select::song_entry* managed = nullptr;
    for (const song_select::song_entry& song : catalog.songs) {
        if (song.song.meta.song_id == "remote-song") {
            legacy = &song;
        }
        if (song.song.meta.song_id == managed_song_id) {
            managed = &song;
        }
    }

    assert(legacy != nullptr);
    assert(legacy->storage == storage_policy::plain_workspace);
    assert(legacy->song.directory.find("content-cache") == std::string::npos);

    assert(managed != nullptr);
    assert(managed->storage == storage_policy::managed_package);
    assert(managed->status == content_status::modified);
    assert(managed->source_status == content_status::community);
    assert(managed->sync_state == content_sync_state::modified);
    assert(managed->online_identity.has_value());
    assert(managed->online_identity->remote_song_id == "remote-song");
    assert(managed->managed_manifest.has_value());
    assert(managed->managed_manifest->package_id == "package-remote-song");
    assert(managed->managed_manifest->song_json_hash == "local-song-json-sha");
    assert(managed->managed_manifest->audio_hash == "local-audio-sha");
    assert(managed->song.directory.find("content-cache") != std::string::npos);
    assert(managed->song.directory.find("community") != std::string::npos);
    assert(managed->charts.size() == 2);
    const song_select::chart_option* managed_remote_chart = nullptr;
    const song_select::chart_option* local_extra_chart = nullptr;
    for (const song_select::chart_option& chart : managed->charts) {
        if (chart.meta.chart_id == managed_chart_id) {
            managed_remote_chart = &chart;
        }
        if (chart.meta.chart_id == extra_chart_id) {
            local_extra_chart = &chart;
        }
    }
    assert(managed_remote_chart != nullptr);
    assert(managed_remote_chart->storage == storage_policy::managed_package);
    assert(managed_remote_chart->status == content_status::modified);
    assert(managed_remote_chart->online_identity.has_value());
    assert(managed_remote_chart->online_identity->remote_chart_id == "remote-chart");
    assert(managed_remote_chart->managed_manifest.has_value());
    assert(managed_remote_chart->managed_manifest->chart_hash == "local-chart-sha");
    assert(managed_remote_chart->managed_manifest->remote_chart_fingerprint == "remote-chart-fingerprint");
    assert(managed_remote_chart->path.find("content-cache") != std::string::npos);
    assert(managed_remote_chart->meta.level > 0.0f);
    assert(local_extra_chart != nullptr);
    assert(local_extra_chart->storage == storage_policy::plain_workspace);
    assert(local_extra_chart->kind == content_kind::local);
    assert(local_extra_chart->status == content_status::local);
    assert(local_extra_chart->source_status == content_status::local);
    assert(local_extra_chart->source == content_source::local);
    assert(local_extra_chart->sync_state == content_sync_state::clean);
    assert(!local_extra_chart->online_identity.has_value());
    assert(!local_extra_chart->managed_manifest.has_value());
    assert(local_extra_chart->remote_links.empty());
    assert(local_extra_chart->path.find("content-cache") != std::string::npos);

    song_select::catalog_data stale_cached_catalog = catalog;
    for (song_select::song_entry& song : stale_cached_catalog.songs) {
        if (song.song.meta.song_id == managed_song_id) {
            for (song_select::chart_option& chart : song.charts) {
                if (chart.meta.chart_id == managed_chart_id) {
                    chart.meta.level = 0.0f;
                }
            }
        }
    }
    song_select::local_catalog_database::replace_catalog(stale_cached_catalog.songs);

    const song_select::catalog_data fast_cached_catalog = song_select::load_catalog(false);
    const song_select::song_entry* fast_cached_managed = nullptr;
    for (const song_select::song_entry& song : fast_cached_catalog.songs) {
        if (song.song.meta.song_id == managed_song_id) {
            fast_cached_managed = &song;
            break;
        }
    }
    assert(fast_cached_managed != nullptr);
    assert(!fast_cached_managed->charts.empty());
    const song_select::chart_option* fast_cached_remote = nullptr;
    const song_select::chart_option* fast_cached_extra = nullptr;
    for (const song_select::chart_option& chart : fast_cached_managed->charts) {
        if (chart.meta.chart_id == managed_chart_id) {
            fast_cached_remote = &chart;
        }
        if (chart.meta.chart_id == extra_chart_id) {
            fast_cached_extra = &chart;
        }
    }
    assert(fast_cached_remote != nullptr);
    assert(fast_cached_remote->meta.level == 0.0f);
    assert(fast_cached_extra != nullptr);
    assert(fast_cached_extra->storage == storage_policy::plain_workspace);

    const song_select::catalog_data repaired_cached_catalog = song_select::load_catalog(true);
    const song_select::song_entry* repaired_managed = nullptr;
    for (const song_select::song_entry& song : repaired_cached_catalog.songs) {
        if (song.song.meta.song_id == managed_song_id) {
            repaired_managed = &song;
            break;
        }
    }
    assert(repaired_managed != nullptr);
    assert(!repaired_managed->charts.empty());
    const song_select::chart_option* repaired_remote = nullptr;
    const song_select::chart_option* repaired_extra = nullptr;
    for (const song_select::chart_option& chart : repaired_managed->charts) {
        if (chart.meta.chart_id == managed_chart_id) {
            repaired_remote = &chart;
        }
        if (chart.meta.chart_id == extra_chart_id) {
            repaired_extra = &chart;
        }
    }
    assert(repaired_remote != nullptr);
    assert(repaired_remote->meta.level > 0.0f);
    assert(repaired_extra != nullptr);
    assert(repaired_extra->storage == storage_policy::plain_workspace);

    const song_select::catalog_data persisted_cached_catalog =
        song_select::local_catalog_database::load_cached_catalog();
    const song_select::song_entry* persisted_managed = nullptr;
    for (const song_select::song_entry& song : persisted_cached_catalog.songs) {
        if (song.song.meta.song_id == managed_song_id) {
            persisted_managed = &song;
            break;
        }
    }
    assert(persisted_managed != nullptr);
    assert(!persisted_managed->charts.empty());
    const song_select::chart_option* persisted_remote = nullptr;
    const song_select::chart_option* persisted_extra = nullptr;
    for (const song_select::chart_option& chart : persisted_managed->charts) {
        if (chart.meta.chart_id == managed_chart_id) {
            persisted_remote = &chart;
        }
        if (chart.meta.chart_id == extra_chart_id) {
            persisted_extra = &chart;
        }
    }
    assert(persisted_remote != nullptr);
    assert(persisted_remote->meta.level > 0.0f);
    assert(persisted_extra != nullptr);
    assert(persisted_extra->storage == storage_policy::plain_workspace);

    const managed_content_storage::chart_identity uploaded_extra_identity{
        .source = online_content::source::community,
        .server_url = song_identity.server_url,
        .remote_song_id = song_identity.remote_song_id,
        .remote_chart_id = "uploaded-extra-chart",
        .song_version = song_identity.song_version,
        .chart_version = 7,
        .revision_id = "uploaded-extra-rev-7",
        .chart_hash = "uploaded-extra-local-sha",
        .chart_fingerprint = "uploaded-extra-local-fingerprint",
        .remote_chart_hash = "uploaded-extra-remote-sha",
        .remote_chart_fingerprint = "uploaded-extra-local-fingerprint",
    };
    const fs::path local_extra_path =
        managed_content_storage::chart_file_path(managed_song_dir, extra_chart_id);
    const managed_content_storage::chart_promotion_result promoted_extra =
        managed_content_storage::promote_plain_chart_to_managed(
            managed_song_dir, uploaded_extra_identity, local_extra_path, true, error_message);
    assert(promoted_extra.success);
    assert(!promoted_extra.local_chart_id.empty());
    assert(promoted_extra.local_chart_id != extra_chart_id);
    assert(!fs::exists(local_extra_path));
    assert(fs::exists(managed_content_storage::encrypted_asset_path(
        promoted_extra.song_directory, "charts/" + promoted_extra.local_chart_id + ".rchart")));

    local_content_index::put_chart_binding({
        .server_url = song_identity.server_url,
        .local_chart_id = promoted_extra.local_chart_id,
        .remote_chart_id = uploaded_extra_identity.remote_chart_id,
        .remote_song_id = uploaded_extra_identity.remote_song_id,
        .remote_chart_version = uploaded_extra_identity.chart_version,
        .origin = local_content_index::online_origin::owned_upload,
    });

    const song_select::catalog_data promoted_catalog = song_select::load_catalog(true);
    const song_select::song_entry* promoted_song = nullptr;
    for (const song_select::song_entry& song : promoted_catalog.songs) {
        if (song.song.meta.song_id == managed_song_id) {
            promoted_song = &song;
            break;
        }
    }
    assert(promoted_song != nullptr);
    const song_select::chart_option* promoted_chart = nullptr;
    for (const song_select::chart_option& chart : promoted_song->charts) {
        if (chart.meta.chart_id == promoted_extra.local_chart_id) {
            promoted_chart = &chart;
            break;
        }
    }
    assert(promoted_chart != nullptr);
    assert(promoted_chart->storage == storage_policy::managed_package);
    assert(promoted_chart->source_status == content_status::community);
    assert(promoted_chart->source == content_source::community);
    assert(promoted_chart->sync_state == content_sync_state::clean);
    assert(promoted_chart->online_identity.has_value());
    assert(promoted_chart->online_identity->remote_chart_id == uploaded_extra_identity.remote_chart_id);
    assert(promoted_chart->managed_manifest.has_value());

    local_content_database::put_remote_metadata({
        .server_url = song_identity.server_url,
        .type = local_content_database::remote_content_type::song,
        .remote_id = song_identity.remote_song_id,
        .content_source = "official",
        .lifecycle_status = "published",
        .review_status = "approved",
        .remote_version = song_identity.song_version,
        .revision_id = song_identity.revision_id,
        .content_hash = "official-song-fingerprint",
    });
    const managed_content_storage::song_identity official_song_identity{
        .source = online_content::source::official,
        .server_url = song_identity.server_url,
        .remote_song_id = song_identity.remote_song_id,
        .song_version = song_identity.song_version,
        .revision_id = song_identity.revision_id,
        .package_id = song_identity.package_id,
    };
    const fs::path official_song_dir = managed_content_storage::song_directory(official_song_identity);
    const song_select::catalog_data official_catalog = song_select::load_catalog(true);
    assert(!fs::exists(managed_content_storage::manifest_path(managed_song_dir)));
    assert(fs::exists(managed_content_storage::manifest_path(official_song_dir)));
    const song_select::song_entry* official_song = nullptr;
    for (const song_select::song_entry& song : official_catalog.songs) {
        if (song.song.meta.song_id == managed_song_id) {
            official_song = &song;
            break;
        }
    }
    assert(official_song != nullptr);
    assert(official_song->storage == storage_policy::managed_package);
    assert(official_song->source_status == content_status::official);
    assert(official_song->source == content_source::official);
    assert(official_song->song.directory.find("official") != std::string::npos);
    const song_select::chart_option* official_promoted_chart = nullptr;
    for (const song_select::chart_option& chart : official_song->charts) {
        if (chart.meta.chart_id == promoted_extra.local_chart_id) {
            official_promoted_chart = &chart;
            break;
        }
    }
    assert(official_promoted_chart != nullptr);
    assert(official_promoted_chart->storage == storage_policy::managed_package);
    assert(official_promoted_chart->source_status == content_status::official);
    assert(official_promoted_chart->source == content_source::official);

    const fs::path encrypted_chart_path =
        managed_content_storage::encrypted_asset_path(official_song_dir, stored_manifest->charts.front().encrypted_chart);
    const fs::path encrypted_chart_backup = temp_root / "managed-chart.renc.backup";
    const std::string complete_signature = local_catalog_signature::current();
    fs::rename(encrypted_chart_path, encrypted_chart_backup, ec);
    assert(!ec);
    assert(local_catalog_signature::current() != complete_signature);
    assert(song_select::local_catalog_database::load_cached_catalog().songs.empty());
    fs::rename(encrypted_chart_backup, encrypted_chart_path, ec);
    assert(!ec);
    (void)song_select::load_catalog(true);
    assert(!song_select::local_catalog_database::load_cached_catalog().songs.empty());

    fs::remove_all(temp_root, ec);
    std::cout << "managed_content_storage smoke test passed\n";
    return EXIT_SUCCESS;
}
