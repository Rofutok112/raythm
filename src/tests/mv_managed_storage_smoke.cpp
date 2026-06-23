#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#include "app_paths.h"
#include "mv/composition/mv_composition_evaluator.h"
#include "mv/mv_managed_storage.h"
#include "path_utils.h"

namespace {
namespace fs = std::filesystem;

bool set_local_app_data(const std::string& value) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", value.c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", value.c_str(), 1) == 0;
#endif
}

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

}  // namespace

int main() {
    const fs::path temp_local_app_data = fs::temp_directory_path() / "raythm_mv_managed_storage_smoke";
    std::error_code ec;
    fs::remove_all(temp_local_app_data, ec);
    ec.clear();
    fs::create_directories(temp_local_app_data, ec);
    if (ec) {
        std::cerr << "Failed to prepare temporary LOCALAPPDATA root\n";
        return EXIT_FAILURE;
    }
    if (!set_local_app_data(temp_local_app_data.string())) {
        std::cerr << "Failed to update LOCALAPPDATA for smoke test\n";
        return EXIT_FAILURE;
    }
    app_paths::ensure_directories();

    bool ok = true;
    mv::managed_storage::mv_identity identity{
        .source = online_content::source::community,
        .server_url = "https://server.example/api",
        .remote_song_id = "remote-song",
        .remote_mv_id = "remote-mv",
        .song_version = 2,
        .mv_version = 3,
        .revision_id = "mv-rev-3",
        .package_id = "package-remote-mv",
        .mv_hash = "local-composition-hash-before-edit",
        .mv_fingerprint = "local-composition-fingerprint-before-edit",
        .remote_mv_hash = "remote-composition-hash",
        .remote_mv_fingerprint = "remote-composition-fingerprint",
        .unlock = {
            .unlock_state = "unlocked",
            .locked = false,
            .can_download = true,
            .can_play = true,
            .lock_reason = "",
            .unlock_rule_count = 0,
        },
    };

    const std::string local_id = mv::managed_storage::local_mv_id(identity);
    const fs::path package_dir = mv::managed_storage::package_directory(identity);
    expect(local_id.rfind("mv_", 0) == 0, "Expected managed MV ids to use mv_ prefix.", ok);
    expect(package_dir.string().find("mvs") != std::string::npos,
           "Expected managed MV package to live under content-cache/{source}/mvs.",
           ok);
    expect(package_dir.string().find("charts") == std::string::npos,
           "Expected managed MV package path not to mention charts.",
           ok);

    mv::managed_storage::package_manifest manifest;
    manifest.mv = identity;
    manifest.local_song_id = "local-song-shadow";
    manifest.unlock = identity.unlock;

    mv::composition::mv_composition composition = mv::composition::make_default_for_song(local_id, 90000.0);
    composition.objects.front().name = "Managed Background";
    mv::composition::keyframe_track& opacity =
        mv::composition::ensure_keyframe_track(composition.objects.front(), "transform.opacity");
    mv::composition::upsert_keyframe(opacity, {.time_ms = 0.0, .value = 0.25f, .easing = "linear"});
    mv::composition::upsert_keyframe(opacity, {.time_ms = 1000.0, .value = 1.0f, .easing = "linear"});

    std::string error_message;
    expect(mv::managed_storage::write_mv_json_asset(
               manifest,
               "{\"mvId\":\"" + local_id + "\",\"songId\":\"remote-song\",\"compositionFile\":\"composition.rmvcomp\"}",
               error_message),
           "Expected managed MV json asset write to succeed: " + error_message,
           ok);
    expect(mv::managed_storage::write_composition(manifest, composition, error_message),
           "Expected managed MV composition write to succeed: " + error_message,
           ok);
    expect(fs::exists(mv::managed_storage::manifest_path(identity)),
           "Expected managed MV manifest to be written.",
           ok);
    expect(!fs::exists(package_dir / "composition.rmvcomp"),
           "Expected managed composition not to be stored as a plain logical file.",
           ok);
    expect(!manifest.composition_asset.encrypted_path.empty(),
           "Expected encrypted composition asset metadata.",
           ok);
    expect(manifest.composition_asset.encrypted_path.find(".encrypted/assets/") == 0,
           "Expected encrypted composition asset to use hashed encrypted asset storage.",
           ok);
    expect(manifest.composition_asset.encrypted_path.find("composition.rmvcomp") == std::string::npos,
           "Expected encrypted asset path not to leak logical composition filename.",
           ok);
    expect(manifest.local_mv_id == local_id,
           "Expected managed MV save to normalize local MV id.",
           ok);
    expect(manifest.mv.mv_hash == manifest.composition_asset.content_hash,
           "Expected local MV hash to follow plaintext composition content hash.",
           ok);
    expect(!manifest.mv.mv_fingerprint.empty(), "Expected local MV fingerprint to be stored.", ok);
    expect(manifest.mv.remote_mv_hash == "remote-composition-hash",
           "Expected remote MV hash to remain unchanged after local managed save.",
           ok);
    expect(manifest.mv.remote_mv_fingerprint == "remote-composition-fingerprint",
           "Expected remote MV fingerprint to remain unchanged after local managed save.",
           ok);
    expect(mv::managed_storage::write_asset_file(manifest,
                                                 "asset-image-test",
                                                 "assets/images/asset-image-test.png",
                                                 "stable-image-bytes",
                                                 error_message),
           "Expected managed MV package image asset write to succeed: " + error_message,
           ok);
    expect(!mv::managed_storage::write_asset_file(manifest,
                                                  "asset-image-escape",
                                                  "../asset-image-escape.png",
                                                  "escape",
                                                  error_message),
           "Expected managed MV asset logical paths outside the package to be rejected.",
           ok);
    expect(!mv::managed_storage::write_asset_file(manifest,
                                                  "asset-image-drive-relative",
                                                  "C:asset-image-drive-relative.png",
                                                  "escape",
                                                  error_message),
           "Expected managed MV drive-relative asset logical paths to be rejected.",
           ok);
    expect(manifest.asset_files.size() == 1 &&
               manifest.asset_files.front().id == "asset-image-test",
           "Expected managed MV asset manifest entry.",
           ok);
    expect(manifest.asset_files.front().asset.encrypted_path.find(".encrypted/assets/") == 0,
           "Expected managed MV asset to use encrypted asset storage.",
           ok);
    expect(!fs::exists(package_dir / "assets" / "images" / "asset-image-test.png"),
           "Expected managed MV image asset not to be stored as a plain logical file.",
           ok);
    const auto read_asset = mv::managed_storage::read_asset_file(
        manifest, package_dir, "asset-image-test");
    expect(read_asset.success &&
               std::string(read_asset.bytes.begin(), read_asset.bytes.end()) == "stable-image-bytes",
           "Expected managed MV image asset to decrypt and round-trip.",
           ok);

    const std::optional<mv::managed_storage::package_manifest> stored =
        mv::managed_storage::read_manifest(package_dir);
    expect(stored.has_value(), "Expected managed MV manifest to parse.", ok);
    if (stored.has_value()) {
        expect(stored->local_mv_id == local_id, "Expected local MV id to round-trip.", ok);
        expect(stored->mv.remote_mv_id == identity.remote_mv_id, "Expected remote MV id to round-trip.", ok);
        expect(stored->mv.remote_song_id == identity.remote_song_id, "Expected remote song id to round-trip.", ok);
        expect(stored->mv.remote_mv_fingerprint == "remote-composition-fingerprint",
               "Expected stored manifest to preserve remote MV fingerprint.",
               ok);
        expect(!stored->composition_asset.ciphertext_hash.empty(),
               "Expected stored manifest to include encrypted composition hash.",
               ok);
        expect(stored->asset_files.size() == 1 &&
                   stored->asset_files.front().id == "asset-image-test",
               "Expected stored manifest to preserve MV asset file entries.",
               ok);

        const mv::managed_storage::composition_read_result read =
            mv::managed_storage::read_composition(*stored, package_dir);
        expect(read.success, "Expected managed MV composition to decrypt and parse.", ok);
        if (read.success) {
            expect(read.composition.composition_id == local_id,
                   "Expected managed composition id to use local MV id.",
                   ok);
            expect(!read.composition.objects.empty(), "Expected managed composition layers to round-trip.", ok);
            const mv::composition::transform evaluated =
                mv::composition::evaluate_transform(read.composition.objects.front(), 500.0);
            expect(evaluated.opacity > 0.62f && evaluated.opacity < 0.63f,
                   "Expected keyframes to survive managed composition round-trip.",
                   ok);
        }
    }

    mv::composition::mv_composition edited = composition;
    if (mv::composition::component* transform = mv::composition::transform_component(edited.objects.front())) {
        transform->position_x = 321.0f;
    }
    const std::string previous_remote_hash = manifest.mv.remote_mv_hash;
    const std::string previous_remote_fingerprint = manifest.mv.remote_mv_fingerprint;
    const std::string previous_local_hash = manifest.mv.mv_hash;
    expect(mv::managed_storage::write_composition(manifest, edited, error_message),
           "Expected edited managed MV composition write to succeed: " + error_message,
           ok);
    expect(manifest.mv.mv_hash != previous_local_hash,
           "Expected editing managed MV to update local composition hash.",
           ok);
    expect(manifest.mv.remote_mv_hash == previous_remote_hash,
           "Expected editing managed MV to keep remote composition hash.",
           ok);
    expect(manifest.mv.remote_mv_fingerprint == previous_remote_fingerprint,
           "Expected editing managed MV to keep remote composition fingerprint.",
           ok);

    mv::managed_storage::package_manifest revoked_manifest = manifest;
    revoked_manifest.license_revoked = true;
    expect(!mv::managed_storage::can_edit(revoked_manifest).editable,
           "Expected revoked managed MV licenses to block editing.",
           ok);
    expect(!mv::managed_storage::write_composition(revoked_manifest, edited, error_message),
           "Expected revoked managed MV licenses to block composition writes.",
           ok);

    mv::managed_storage::package_manifest expired_manifest = manifest;
    expired_manifest.license_expires_at = "2000-01-01T00:00:00Z";
    expired_manifest.offline_license_expires_at.clear();
    expect(!mv::managed_storage::can_edit(expired_manifest).editable,
           "Expected expired managed MV licenses to block editing.",
           ok);

    mv::managed_storage::package_manifest locked_manifest = manifest;
    locked_manifest.unlock.locked = true;
    locked_manifest.unlock.can_play = false;
    locked_manifest.unlock.lock_reason = "Subscription required.";
    const auto locked_access = mv::managed_storage::can_edit(locked_manifest);
    expect(!locked_access.editable && locked_access.reason == "Subscription required.",
           "Expected locked managed MV unlock state to block editing with reason.",
           ok);
    expect(!mv::managed_storage::write_asset_file(locked_manifest,
                                                 "blocked-asset",
                                                 "assets/images/blocked.png",
                                                 "blocked",
                                                 error_message),
           "Expected locked managed MV unlock state to block asset writes.",
           ok);

    fs::remove_all(temp_local_app_data, ec);
    if (!ok) {
        return EXIT_FAILURE;
    }
    std::cout << "mv_managed_storage smoke test passed\n";
    return EXIT_SUCCESS;
}
