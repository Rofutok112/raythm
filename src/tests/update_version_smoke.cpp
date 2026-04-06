#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>

#include "app_paths.h"
#include "updater/update_apply.h"
#include "updater/update_download.h"
#include "updater/update_extract.h"
#include "updater/update_process.h"
#include "updater/update_release.h"
#include "updater/update_paths.h"
#include "updater/update_version.h"
#include "updater/update_verify.h"
#include "updater/update_workflow.h"

namespace {
namespace fs = std::filesystem;

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
        if (!active_) {
            return;
        }
        set_local_app_data(previous_value_);
    }

    bool active() const {
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

}  // namespace

int main() {
    const fs::path temp_local_app_data = fs::temp_directory_path() / "raythm_update_version_smoke";

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

        const auto version = updater::parse_semantic_version("v1.2.3");
        expect(version.has_value(), "Expected v-prefixed semantic version to parse.", ok);
        expect(version.has_value() && version->major == 1 && version->minor == 2 && version->patch == 3,
               "Expected parsed semantic version fields to match.",
               ok);
        expect(!updater::parse_semantic_version("1.2").has_value(),
               "Expected incomplete semantic version to be rejected.",
               ok);
        expect(!updater::parse_semantic_version("1.2.3.4").has_value(),
               "Expected four-part semantic version to be rejected.",
               ok);
        expect(updater::is_newer_version({1, 2, 4}, {1, 2, 3}),
               "Expected semantic version comparison to treat patch increments as newer.",
               ok);
        expect(!updater::is_newer_version({1, 2, 3}, {1, 2, 3}),
               "Expected identical semantic versions not to be newer.",
               ok);

        const std::string release_response = R"({
  "tag_name": "v1.4.2",
  "assets": [
    {
      "name": "game-win64.zip",
      "browser_download_url": "https://example.invalid/game-win64.zip"
    },
    {
      "name": "SHA256SUMS.txt",
      "browser_download_url": "https://example.invalid/SHA256SUMS.txt"
    }
  ]
})";
        const std::optional<updater::latest_release_info> latest_release =
            updater::parse_latest_release_response(release_response);
        expect(latest_release.has_value(), "Expected latest release response to parse.", ok);
        expect(latest_release.has_value() && latest_release->version == updater::semantic_version{1, 4, 2},
               "Expected latest release tag to parse as semantic version.",
               ok);
        expect(latest_release.has_value() &&
                   latest_release->assets.package_url == "https://example.invalid/game-win64.zip",
               "Expected package asset URL to be extracted.",
               ok);
        expect(latest_release.has_value() &&
                   latest_release->assets.checksum_url == "https://example.invalid/SHA256SUMS.txt",
               "Expected checksum asset URL to be extracted.",
               ok);

        if (latest_release.has_value()) {
            const updater::update_launch_request request{{1, 2, 3}, *latest_release};
            const std::vector<std::string> arguments = updater::build_updater_arguments(request);
            expect(arguments.size() == 5,
                   "Expected updater launch arguments to include current version, target version, tag, and asset URLs.",
                   ok);

            std::vector<char*> argv;
            argv.reserve(arguments.size() + 1);
            argv.push_back(const_cast<char*>("Updater.exe"));
            for (const std::string& argument : arguments) {
                argv.push_back(const_cast<char*>(argument.c_str()));
            }

            const std::optional<updater::update_launch_request> parsed_request =
                updater::parse_updater_arguments(static_cast<int>(argv.size()), argv.data());
            expect(parsed_request.has_value(), "Expected updater launch arguments to round-trip.", ok);
            expect(parsed_request.has_value() && parsed_request->current_version == updater::semantic_version{1, 2, 3},
                   "Expected updater launch arguments to preserve the current version.",
                   ok);
            expect(parsed_request.has_value() &&
                       parsed_request->target_release.assets.package_url ==
                           "https://example.invalid/game-win64.zip",
                   "Expected updater launch arguments to preserve the package URL.",
                   ok);
        }

        expect(updater::file_name_from_url("https://example.invalid/downloads/game-win64.zip", "fallback.zip") ==
                   "game-win64.zip",
               "Expected file name extraction from package URL to preserve the asset name.",
               ok);
        expect(updater::file_name_from_url("https://example.invalid/releases/latest?foo=bar", "fallback.zip") ==
                   "latest",
               "Expected file name extraction to ignore query parameters.",
               ok);
        expect(updater::file_name_from_url("https://example.invalid/", "fallback.zip") == "fallback.zip",
               "Expected file name extraction to fall back when URL has no trailing asset name.",
               ok);
        expect(updater::process_name_matches("raythm.exe", "RAYTHM.EXE"),
               "Expected process name matching to be case-insensitive.",
               ok);
        expect(!updater::process_name_matches("Updater.exe", "raythm.exe"),
               "Expected process name matching to reject different executable names.",
               ok);

        const fs::path checksum_file = temp_local_app_data / "sample.zip";
        {
            std::ofstream output(checksum_file, std::ios::binary);
            output << "abc";
        }
        const std::optional<std::string> file_hash = updater::compute_sha256_hex(checksum_file);
        expect(file_hash.has_value(), "Expected SHA-256 hash computation to succeed for a local file.", ok);
        expect(file_hash.has_value() &&
                   *file_hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
               "Expected SHA-256 hash for 'abc' to match the known digest.",
               ok);

        const std::string checksums_content =
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad *sample.zip\n"
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef *other.zip\n";
        const std::optional<std::string> parsed_hash =
            updater::parse_sha256sums_for_file(checksums_content, "sample.zip");
        expect(parsed_hash.has_value(), "Expected checksum parser to find the matching file entry.", ok);
        expect(parsed_hash.has_value() &&
                   *parsed_hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
               "Expected checksum parser to return the matching hash text.",
               ok);

        const fs::path checksum_list_path = temp_local_app_data / "SHA256SUMS.txt";
        {
            std::ofstream output(checksum_list_path);
            output << checksums_content;
        }
        expect(updater::verify_sha256_checksum(checksum_file, checksum_list_path),
               "Expected checksum verification to succeed for a matching digest.",
               ok);

        updater::ensure_update_directories();
        expect(fs::is_directory(updater::update_root()), "Expected updater root to be created.", ok);
        expect(fs::is_directory(updater::downloads_root()), "Expected downloads directory to be created.", ok);
        expect(fs::is_directory(updater::staging_root()), "Expected staging directory to be created.", ok);
        expect(fs::is_directory(updater::backup_root()), "Expected backup directory to be created.", ok);
        expect(updater::reset_directory(updater::staging_root()),
               "Expected staging directory reset helper to succeed.",
               ok);
        expect(fs::is_directory(updater::staging_root()),
               "Expected staging directory to still exist after reset.",
               ok);

        const fs::path source_dir = temp_local_app_data / "copy-source";
        const fs::path destination_dir = temp_local_app_data / "copy-destination";
        fs::create_directories(source_dir / "nested", ec);
        {
            std::ofstream output(source_dir / "nested" / "hello.txt");
            output << "world";
        }
        expect(updater::copy_directory_contents(source_dir, destination_dir),
               "Expected directory copy helper to copy staged files.",
               ok);
        expect(fs::is_regular_file(destination_dir / "nested" / "hello.txt"),
               "Expected directory copy helper to create copied files.",
               ok);

        const fs::path install_dir = temp_local_app_data / "install";
        const fs::path staged_dir = temp_local_app_data / "staged";
        const fs::path backup_dir = temp_local_app_data / "backup";
        fs::create_directories(install_dir / "assets" / "charts", ec);
        fs::create_directories(staged_dir / "assets" / "charts", ec);
        {
            std::ofstream output(install_dir / "raythm.exe");
            output << "old";
        }
        {
            std::ofstream output(install_dir / "assets" / "charts" / "legacy-only.rchart");
            output << "legacy";
        }
        {
            std::ofstream output(staged_dir / "raythm.exe");
            output << "new";
        }
        {
            std::ofstream output(staged_dir / "Updater.exe");
            output << "skip";
        }
        {
            std::ofstream output(staged_dir / "assets" / "charts" / "new-only.rchart");
            output << "new-chart";
        }
        expect(updater::apply_staged_update(install_dir, staged_dir, backup_dir),
               "Expected staged update helper to copy staged files into the install root.",
               ok);
        expect(fs::is_regular_file(backup_dir / "raythm.exe"),
               "Expected staged update helper to keep a backup copy of the install root.",
               ok);
        expect(fs::file_size(install_dir / "raythm.exe") == 3,
               "Expected staged update helper to replace install files with staged files.",
               ok);
        expect(!fs::exists(install_dir / "Updater.exe"),
               "Expected staged update helper to avoid overwriting the running updater executable.",
               ok);
        expect(!fs::exists(install_dir / "assets" / "charts" / "legacy-only.rchart"),
               "Expected staged update helper to fully replace bundled assets contents.",
               ok);
        expect(fs::is_regular_file(install_dir / "assets" / "charts" / "new-only.rchart"),
               "Expected staged update helper to copy new bundled assets files.",
               ok);
        expect(updater::ensure_process_stopped("definitely-not-running.exe", std::chrono::milliseconds(1)),
               "Expected process stop helper to succeed immediately when the target process is absent.",
               ok);

        const updater::installed_version_info saved{{1, 4, 2}};
        expect(updater::save_installed_version(saved), "Expected installed version file to be saved.", ok);
        expect(fs::is_regular_file(updater::version_file_path()), "Expected version.json to be created.", ok);

        const std::optional<updater::installed_version_info> loaded = updater::load_installed_version();
        expect(loaded.has_value(), "Expected installed version file to load.", ok);
        expect(loaded.has_value() && loaded->version == saved.version,
               "Expected loaded version to match saved version.",
               ok);

        updater::ensure_installed_version_file({9, 9, 9});
        const std::optional<updater::installed_version_info> loaded_again = updater::load_installed_version();
        expect(loaded_again.has_value() && loaded_again->version == saved.version,
               "Expected ensure_installed_version_file not to overwrite existing version.",
               ok);
        expect(app_paths::app_data_root().filename() == "raythm",
               "Expected updater paths to be rooted under the raythm app data directory.",
               ok);
    }

    fs::remove_all(temp_local_app_data, ec);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "update_version smoke test passed\n";
    return EXIT_SUCCESS;
}
