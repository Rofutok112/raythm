#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "app_paths.h"
#include "updater/update_apply.h"
#include "updater/update_download.h"
#include "updater/update_extract.h"
#include "updater/update_process.h"
#include "updater/update_paths.h"
#include "updater/update_version.h"
#include "updater/update_verify.h"
#include "updater/update_workflow.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
bool launch_game_process() {
    const std::filesystem::path game_path = app_paths::executable_dir() / "raythm.exe";
    if (!std::filesystem::exists(game_path)) {
        return false;
    }

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};
    std::wstring command_line = L"\"" + game_path.wstring() + L"\"";
    const BOOL created = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                                        app_paths::executable_dir().wstring().c_str(), &startup_info, &process_info);
    if (created == FALSE) {
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return true;
}
#else
bool launch_game_process() {
    return false;
}
#endif

}  // namespace

int main(int argc, char* argv[]) {
    updater::ensure_update_directories();

    const auto installed_version = updater::load_installed_version();
    if (installed_version.has_value()) {
        std::cout << "Updater initialized for installed version "
                  << updater::to_string(installed_version->version) << '\n';
    } else {
        std::cout << "Updater initialized without installed version metadata\n";
    }

    const auto request = updater::parse_updater_arguments(argc, argv);
    if (request.has_value()) {
        std::filesystem::path package_path;
        std::filesystem::path checksum_path;

        std::cout << "Requested update from " << updater::to_string(request->current_version)
                  << " to " << request->target_release.tag_name << '\n';
        if (!request->target_release.assets.package_url.empty()) {
            std::cout << "Package URL: " << request->target_release.assets.package_url << '\n';
        }
        if (!request->target_release.assets.checksum_url.empty()) {
            std::cout << "Checksum URL: " << request->target_release.assets.checksum_url << '\n';
        }

        if (!request->target_release.assets.package_url.empty()) {
            package_path =
                updater::downloads_root() /
                updater::file_name_from_url(request->target_release.assets.package_url, "game-win64.zip");
            std::cout << "Downloading package to " << package_path.string() << '\n';
            if (!updater::download_url_to_file(request->target_release.assets.package_url, package_path)) {
                std::cerr << "Failed to download package.\n";
                return EXIT_FAILURE;
            }
        }

        if (!request->target_release.assets.checksum_url.empty()) {
            checksum_path =
                updater::downloads_root() /
                updater::file_name_from_url(request->target_release.assets.checksum_url, "SHA256SUMS.txt");
            std::cout << "Downloading checksum to " << checksum_path.string() << '\n';
            if (!updater::download_url_to_file(request->target_release.assets.checksum_url, checksum_path)) {
                std::cerr << "Failed to download checksum file.\n";
                return EXIT_FAILURE;
            }
        }

        if (!package_path.empty() && !checksum_path.empty()) {
            std::cout << "Verifying SHA-256 checksum\n";
            if (!updater::verify_sha256_checksum(package_path, checksum_path)) {
                std::cerr << "SHA-256 verification failed.\n";
                return EXIT_FAILURE;
            }
        }

        if (!package_path.empty()) {
            std::cout << "Extracting package to " << updater::staging_root().string() << '\n';
            if (!updater::extract_zip_to_directory(package_path, updater::staging_root())) {
                std::cerr << "Failed to extract package into staging.\n";
                return EXIT_FAILURE;
            }

            std::cout << "Waiting for raythm.exe to stop\n";
            if (!updater::ensure_process_stopped("raythm.exe", std::chrono::seconds(2))) {
                std::cerr << "Failed to stop running game process.\n";
                return EXIT_FAILURE;
            }

            std::cout << "Applying staged update to " << app_paths::executable_dir().string() << '\n';
            if (!updater::apply_staged_update(app_paths::executable_dir(),
                                              updater::staging_root(),
                                              updater::backup_root() / "current")) {
                std::cerr << "Failed to apply staged update.\n";
                return EXIT_FAILURE;
            }

            if (!updater::save_installed_version({request->target_release.version})) {
                std::cerr << "Failed to update installed version metadata.\n";
                return EXIT_FAILURE;
            }

            if (!launch_game_process()) {
                std::cerr << "Failed to launch updated game.\n";
                return EXIT_FAILURE;
            }
        }
    } else {
        std::cout << "Updater started without a launch request\n";
    }

    std::cout << "Updater workflow completed.\n";
    return EXIT_SUCCESS;
}
