#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

#include "app_paths.h"
#include "core/path_utils.h"
#include "data_models.h"
#include "mv/composition/mv_composition_serializer.h"
#include "mv/mv_managed_storage.h"
#include "mv/mv_storage.h"

namespace {
namespace fs = std::filesystem;

#ifdef _WIN32
FILE* open_file_write_binary(const fs::path& path) {
    return _wfopen(path.c_str(), L"wb");
}
#else
FILE* open_file_write_binary(const fs::path& path) {
    return std::fopen(path.string().c_str(), "wb");
}
#endif

bool set_local_app_data(const std::string& value) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", value.c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", value.c_str(), 1) == 0;
#endif
}

class local_app_data_guard {
public:
    explicit local_app_data_guard(const fs::path& path) {
        if (const char* current = std::getenv("LOCALAPPDATA")) {
            previous_value_ = current;
        }
        active_ = set_local_app_data(path.string());
    }

    ~local_app_data_guard() {
        if (active_) {
            set_local_app_data(previous_value_);
        }
    }

    [[nodiscard]] bool active() const {
        return active_;
    }

private:
    std::string previous_value_;
    bool active_ = false;
};

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

bool write_text_file(const fs::path& path, const std::string& content) {
    FILE* file = open_file_write_binary(path);
    if (file == nullptr) {
        return false;
    }

    const size_t written = std::fwrite(content.data(), 1, content.size(), file);
    const bool ok = written == content.size() && std::fflush(file) == 0;
    std::fclose(file);
    return ok;
}

std::optional<std::string> read_text_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

int main() {
    const fs::path temp_local_app_data = fs::temp_directory_path() / "raythm_mv_storage_smoke";

    std::error_code ec;
    fs::remove_all(temp_local_app_data, ec);
    ec.clear();
    fs::create_directories(temp_local_app_data, ec);
    if (ec) {
        std::cerr << "Failed to prepare temporary LOCALAPPDATA root\n";
        return EXIT_FAILURE;
    }

    bool ok = true;
    {
        local_app_data_guard guard(temp_local_app_data);
        if (!guard.active()) {
            std::cerr << "Failed to update LOCALAPPDATA for smoke test\n";
            return EXIT_FAILURE;
        }

        app_paths::ensure_directories();

        song_meta song;
        song.song_id = "song-123";
        song.title = "Smoke Song";
        song.duration_seconds = 123.0f;

        mv::mv_package package = mv::make_default_package_for_song(song);
        package.meta.name = "Smoke MV";
        package.meta.author = "Codex";

        expect(mv::ensure_composition_package(package), "Expected MV composition package to be written.", ok);
        const auto default_duration_composition = mv::load_composition(package);
        expect(default_duration_composition.has_value() &&
                   default_duration_composition->duration_ms == 123000.0,
               "Expected new MV composition duration to match song duration.",
               ok);
        expect(default_duration_composition.has_value() &&
                   !default_duration_composition->layers.empty() &&
                   default_duration_composition->layers.front().duration_ms == 123000.0,
               "Expected new MV default layers to cover the song duration.",
               ok);
        mv::mv_package unsafe_composition_file_package = package;
        unsafe_composition_file_package.meta.composition_file = "../outside.rmvcomp";
        std::vector<std::string> unsafe_metadata_errors;
        expect(!mv::write_mv_json(unsafe_composition_file_package.meta,
                                  unsafe_composition_file_package.directory),
               "Expected unsafe MV compositionFile metadata writes to fail.",
               ok);
        expect(!mv::save_metadata(unsafe_composition_file_package, &unsafe_metadata_errors),
               "Expected unsafe MV compositionFile metadata saves to fail.",
               ok);
        expect(!mv::save_composition(unsafe_composition_file_package,
                                     mv::make_default_composition_for_song(package)),
               "Expected unsafe MV compositionFile composition saves to fail.",
               ok);
        unsafe_composition_file_package.meta.composition_file = "C:outside.rmvcomp";
        expect(!mv::write_mv_json(unsafe_composition_file_package.meta,
                                  unsafe_composition_file_package.directory),
               "Expected drive-relative MV compositionFile metadata writes to fail.",
               ok);
        expect(fs::is_directory(path_utils::from_utf8(package.directory) / "assets" / "images"),
               "Expected MV image asset directory to be created.",
               ok);
        expect(fs::is_directory(path_utils::from_utf8(package.directory) / "assets" / "generated"),
               "Expected MV generated asset directory to be created.",
               ok);
        const fs::path image_source = temp_local_app_data / "asset-source.png";
        expect(write_text_file(image_source, "not-a-real-png-but-stable-bytes"),
               "Expected to write image asset source fixture.",
               ok);
        std::vector<std::string> asset_errors;
        const auto imported_asset = mv::import_image_asset(package, path_utils::to_utf8(image_source), &asset_errors);
        expect(imported_asset.has_value(), "Expected image asset import to succeed.", ok);
        if (imported_asset.has_value()) {
            expect(imported_asset->type == "image", "Expected image asset type.", ok);
            expect(imported_asset->path.rfind("assets/images/", 0) == 0,
                   "Expected image asset to use package-relative assets/images path.",
                   ok);
            expect(fs::exists(mv::resolve_asset_path(package, *imported_asset)),
                   "Expected imported image asset file to exist inside the MV package.",
                   ok);
        }
        const auto initial_composition = mv::load_composition(package);
        expect(initial_composition.has_value(), "Expected default composition to load.", ok);
        if (initial_composition.has_value()) {
            expect(initial_composition->composition_id == package.meta.mv_id,
                   "Expected composition id to match package id.",
                   ok);
            expect(!initial_composition->layers.empty(), "Expected default composition to contain layers.", ok);
        }

        const std::string japanese_dir = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
        const fs::path import_dir = temp_local_app_data / "MV Import" / path_utils::from_utf8(japanese_dir);
        fs::create_directories(import_dir, ec);
        expect(!ec, "Expected to create import directory.", ok);
        const fs::path import_path = import_dir / "imported.rmvcomp";
        mv::composition::mv_composition imported = mv::make_default_composition_for_song(package);
        imported.composition_id = "foreign-composition-id";
        imported.canvas_data.background = "#112233";
        imported.layers.front().source_data.fill = "#112233";
        expect(write_text_file(import_path, mv::composition::serialize(imported)),
               "Expected to write import composition.",
               ok);
        std::vector<std::string> import_errors;
        expect(mv::import_composition(package, path_utils::to_utf8(import_path), &import_errors),
               "Expected import_composition to overwrite the package composition.",
               ok);
        const auto loaded_import = mv::load_composition(package);
        expect(loaded_import.has_value() &&
                   loaded_import->canvas_data.background == "#112233",
               "Expected imported composition to replace the previous composition.",
               ok);
        expect(mv::import_composition(package, path_utils::to_utf8(mv::composition_path(package)), &import_errors),
               "Expected import_composition to accept re-importing the current composition file.",
               ok);

        const auto found = mv::find_first_package_for_song(song.song_id);
        expect(found.has_value(), "Expected to find MV package by song ID.", ok);
        if (found.has_value()) {
            expect(found->meta.mv_id == package.meta.mv_id, "Expected mv_id to round-trip.", ok);
            expect(found->meta.name == package.meta.name, "Expected MV name to round-trip.", ok);
            expect(found->meta.author == package.meta.author, "Expected MV author to round-trip.", ok);
            expect(found->meta.song_id == song.song_id, "Expected song_id to round-trip.", ok);
            expect(found->meta.composition_file == "composition.rmvcomp",
                   "Expected composition file to round-trip.",
                   ok);
            const auto found_composition = mv::load_composition(*found);
            expect(found_composition.has_value() &&
                       found_composition->canvas_data.background == "#112233",
                   "Expected composition data to round-trip.",
                   ok);
            expect(found_composition.has_value() &&
                       found_composition->composition_id == package.meta.mv_id,
                   "Expected imported composition id to be normalized to package id.",
                   ok);
        }

        const fs::path export_path = import_dir / "exported.rmvcomp";
        expect(mv::export_composition(package, path_utils::to_utf8(export_path)),
               "Expected composition export to succeed.",
               ok);
        expect(fs::exists(export_path), "Expected exported composition file to exist.", ok);
        if (imported_asset.has_value()) {
            mv::composition::mv_composition package_composition = *loaded_import;
            package_composition.assets.push_back(*imported_asset);
            mv::composition::layer image_layer;
            image_layer.id = "layer-package-image";
            image_layer.name = "Package Image";
            image_layer.source_data.type = "image";
            image_layer.source_data.asset_id = imported_asset->id;
            image_layer.duration_ms = 1000.0;
            package_composition.layers.push_back(image_layer);
            expect(mv::save_composition(package, package_composition),
                   "Expected to save package composition with image asset.",
                   ok);
            const fs::path stray_asset = path_utils::from_utf8(package.directory) /
                                         "assets" / "images" / "unreferenced-stray.png";
            expect(write_text_file(stray_asset, "stray-bytes"),
                   "Expected to write unreferenced package file fixture.",
                   ok);

            const fs::path package_export_path = import_dir / "exported.rmvpack";
            std::vector<std::string> package_export_errors;
            expect(mv::export_package(package, path_utils::to_utf8(package_export_path), &package_export_errors),
                   package_export_errors.empty() ? "Expected MV package export to succeed."
                                                 : package_export_errors.front(),
                   ok);
            expect(fs::exists(package_export_path), "Expected exported MV package archive to exist.", ok);

            std::error_code remove_ec;
            fs::remove_all(path_utils::from_utf8(package.directory), remove_ec);
            std::vector<std::string> package_import_errors;
            expect(mv::import_package(path_utils::to_utf8(package_export_path), "song-123-imported",
                                      &package_import_errors),
                   package_import_errors.empty() ? "Expected MV package import to succeed."
                                                 : package_import_errors.front(),
                   ok);
            const auto imported_package = mv::find_first_package_for_song("song-123-imported");
            expect(imported_package.has_value(),
                   "Expected imported MV package to be found by overridden song ID.",
                   ok);
            if (imported_package.has_value()) {
                const auto imported_package_composition = mv::load_composition(*imported_package);
                expect(imported_package_composition.has_value() &&
                           imported_package_composition->composition_id == imported_package->meta.mv_id,
                       "Expected imported MV package composition id to be normalized.",
                       ok);
                expect(imported_package_composition.has_value() &&
                           !imported_package_composition->assets.empty(),
                       "Expected imported MV package assets to round-trip.",
                       ok);
                if (imported_package_composition.has_value() &&
                    !imported_package_composition->assets.empty()) {
                    const auto package_asset_bytes = mv::read_asset_bytes(
                        *imported_package, imported_package_composition->assets.front());
                    expect(package_asset_bytes.has_value() &&
                               std::string(package_asset_bytes->begin(), package_asset_bytes->end()) ==
                                   "not-a-real-png-but-stable-bytes",
                           "Expected imported MV package asset bytes to round-trip.",
                           ok);
                    expect(!fs::exists(path_utils::from_utf8(imported_package->directory) /
                                           "assets" / "images" / "unreferenced-stray.png"),
                           "Expected unreferenced MV package files not to be imported.",
                           ok);
                }
            }

            mv::mv_package unsafe_export_package = package;
            unsafe_export_package.meta.composition_file = "../outside.rmvcomp";
            std::vector<std::string> unsafe_export_errors;
            expect(!mv::export_package(unsafe_export_package,
                                       path_utils::to_utf8(import_dir / "unsafe.rmvpack"),
                                       &unsafe_export_errors),
                   "Expected unsafe MV package compositionFile export to fail.",
                   ok);
        }

        mv::managed_storage::mv_identity managed_identity{
            .source = online_content::source::community,
            .server_url = "https://server.example/api",
            .remote_song_id = "remote-managed-song",
            .remote_mv_id = "remote-managed-mv",
            .song_version = 2,
            .mv_version = 4,
            .revision_id = "managed-mv-rev-4",
            .package_id = "package-managed-mv",
            .remote_mv_hash = "remote-managed-hash",
            .remote_mv_fingerprint = "remote-managed-fingerprint",
        };
        mv::managed_storage::package_manifest managed_manifest;
        managed_manifest.mv = managed_identity;
        managed_manifest.local_song_id = "managed-local-song";
        std::string managed_error;
        const std::string managed_mv_json =
            "{\"mvId\":\"remote-managed-mv\",\"songId\":\"remote-managed-song\","
            "\"name\":\"Managed MV\",\"author\":\"Codex\","
            "\"compositionFile\":\"composition.rmvcomp\",\"formatVersion\":1}";
        expect(mv::managed_storage::write_mv_json_asset(managed_manifest, managed_mv_json, managed_error),
               "Expected managed MV json asset to be written.",
               ok);
        mv::composition::mv_composition managed_composition =
            mv::composition::make_default_for_song(mv::managed_storage::local_mv_id(managed_identity));
        managed_composition.canvas_data.background = "#223344";
        expect(mv::managed_storage::write_composition(managed_manifest, managed_composition, managed_error),
               "Expected managed MV composition asset to be written.",
               ok);
        expect(!fs::exists(mv::managed_storage::package_directory(managed_identity) / "composition.rmvcomp"),
               "Expected managed MV composition not to be stored as plain text.",
               ok);

        const auto found_managed = mv::find_first_package_for_song("managed-local-song");
        expect(found_managed.has_value(), "Expected managed MV package to be found by local song ID.", ok);
            if (found_managed.has_value()) {
            expect(found_managed->storage == storage_policy::managed_package,
                   "Expected managed MV package storage policy.",
                   ok);
            expect(found_managed->meta.mv_id == mv::managed_storage::local_mv_id(managed_identity),
                   "Expected managed MV id to use the local cache id.",
                   ok);
            expect(found_managed->meta.song_id == "managed-local-song",
                   "Expected managed MV to attach to the local song id.",
                   ok);
            mv::mv_package renamed_managed = *found_managed;
            renamed_managed.meta.name = "Renamed Managed MV";
            std::vector<std::string> metadata_errors;
            expect(mv::save_metadata(renamed_managed, &metadata_errors),
                   metadata_errors.empty() ? "Expected managed MV metadata save to encrypt."
                                           : metadata_errors.front(),
                   ok);
            expect(!fs::exists(mv::managed_storage::package_directory(managed_identity) / "mv.json"),
                   "Expected managed MV metadata save not to create a plain mv.json file.",
                   ok);
            const auto metadata_manifest =
                mv::managed_storage::read_manifest(mv::managed_storage::package_directory(managed_identity));
            expect(metadata_manifest.has_value(), "Expected managed MV metadata manifest to reload.", ok);
            if (metadata_manifest.has_value()) {
                const auto metadata_read = mv::managed_storage::read_mv_json_asset(
                    *metadata_manifest, mv::managed_storage::package_directory(managed_identity));
                expect(metadata_read.success &&
                           std::string(metadata_read.bytes.begin(), metadata_read.bytes.end())
                               .find("Renamed Managed MV") != std::string::npos,
                       "Expected managed MV metadata to round-trip through encrypted asset.",
                       ok);
            }
            const auto loaded_managed = mv::load_composition(*found_managed);
            expect(loaded_managed.has_value() &&
                       loaded_managed->canvas_data.background == "#223344",
                   "Expected managed MV composition to decrypt through mv_storage.",
                   ok);
            const fs::path managed_export_path = import_dir / "managed-exported.rmvcomp";
            expect(mv::export_composition(*found_managed, path_utils::to_utf8(managed_export_path)),
                   "Expected managed MV composition export to decrypt and write a plain composition file.",
                   ok);
            const std::optional<std::string> managed_export_text = read_text_file(managed_export_path);
            expect(managed_export_text.has_value(), "Expected managed MV export text to be readable.", ok);
            if (managed_export_text.has_value()) {
                const auto parsed_export = mv::composition::parse(*managed_export_text);
                expect(parsed_export.success &&
                           parsed_export.composition.composition_id == found_managed->meta.mv_id,
                       "Expected managed MV export to contain normalized composition id.",
                       ok);
            }
            std::vector<std::string> managed_asset_errors;
            const auto managed_asset = mv::import_image_asset(
                *found_managed, path_utils::to_utf8(image_source), &managed_asset_errors);
            expect(managed_asset.has_value(),
                   "Expected managed MV image asset import to encrypt through mv_storage.",
                   ok);
            if (managed_asset.has_value()) {
                expect(mv::resolve_asset_path(*found_managed, *managed_asset).empty(),
                       "Expected managed MV image assets not to expose a plain package path.",
                       ok);
                const auto managed_asset_bytes =
                    mv::read_asset_bytes(*found_managed, *managed_asset, &managed_asset_errors);
                expect(managed_asset_bytes.has_value() &&
                           std::string(managed_asset_bytes->begin(), managed_asset_bytes->end()) ==
                               "not-a-real-png-but-stable-bytes",
                       "Expected managed MV image asset bytes to decrypt through mv_storage.",
                       ok);
                const auto updated_asset_manifest =
                    mv::managed_storage::read_manifest(mv::managed_storage::package_directory(managed_identity));
                expect(updated_asset_manifest.has_value() &&
                           updated_asset_manifest->asset_files.size() == 1,
                       "Expected managed MV image asset metadata in manifest.",
                       ok);
                if (loaded_managed.has_value()) {
                    mv::composition::mv_composition managed_package_composition = *loaded_managed;
                    managed_package_composition.assets.push_back(*managed_asset);
                    mv::composition::layer managed_image_layer;
                    managed_image_layer.id = "layer-managed-image";
                    managed_image_layer.name = "Managed Image";
                    managed_image_layer.source_data.type = "image";
                    managed_image_layer.source_data.asset_id = managed_asset->id;
                    managed_image_layer.duration_ms = 1000.0;
                    managed_package_composition.layers.push_back(managed_image_layer);
                    expect(mv::save_composition(*found_managed, managed_package_composition),
                           "Expected managed MV composition with image asset to save.",
                           ok);
                    const fs::path managed_package_export_path = import_dir / "managed-exported.rmvpack";
                    std::vector<std::string> managed_package_export_errors;
                    expect(mv::export_package(*found_managed,
                                              path_utils::to_utf8(managed_package_export_path),
                                              &managed_package_export_errors),
                           managed_package_export_errors.empty()
                               ? "Expected managed MV package export to decrypt assets."
                               : managed_package_export_errors.front(),
                           ok);
                    expect(fs::exists(managed_package_export_path),
                           "Expected managed MV package archive export to exist.",
                           ok);
                }
            }
            if (loaded_managed.has_value()) {
                mv::composition::mv_composition edited_managed = *loaded_managed;
                edited_managed.canvas_data.background = "#445566";
                const std::string previous_remote = managed_manifest.mv.remote_mv_fingerprint;
                expect(mv::save_composition(*found_managed, edited_managed),
                       "Expected mv_storage managed save to re-encrypt composition.",
                       ok);
                const auto updated_manifest =
                    mv::managed_storage::read_manifest(mv::managed_storage::package_directory(managed_identity));
                expect(updated_manifest.has_value() &&
                           updated_manifest->mv.remote_mv_fingerprint == previous_remote,
                       "Expected managed MV save to preserve remote fingerprint.",
                       ok);
                expect(updated_manifest.has_value() &&
                           updated_manifest->mv.mv_hash != managed_manifest.mv.mv_hash,
                       "Expected managed MV save to update local hash.",
                       ok);
                const auto reloaded_managed = mv::load_composition(*found_managed);
                expect(reloaded_managed.has_value() &&
                           reloaded_managed->canvas_data.background == "#445566",
                       "Expected edited managed MV composition to reload through mv_storage.",
                       ok);
            }
        }
    }

    fs::remove_all(temp_local_app_data, ec);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "mv_storage smoke test passed\n";
    return EXIT_SUCCESS;
}
