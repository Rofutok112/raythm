#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "app_paths.h"
#include "updater/update_release.h"
#include "updater/update_paths.h"
#include "updater/update_version.h"
#include "updater/update_workflow.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

#ifndef RAYTHM_PACKAGE_VERSION_STRING
#define RAYTHM_PACKAGE_VERSION_STRING "0.1.0"
#endif

constexpr char kCurrentPackageVersion[] = RAYTHM_PACKAGE_VERSION_STRING;

#ifdef _WIN32
std::wstring quote_windows_argument(const std::filesystem::path& value) {
    return L"\"" + value.wstring() + L"\"";
}

std::wstring quote_windows_argument(const std::string& value) {
    std::wstring wide(value.begin(), value.end());
    return L"\"" + wide + L"\"";
}

bool launch_process(const std::filesystem::path& executable_path, const std::vector<std::string>& arguments) {
    if (!std::filesystem::exists(executable_path)) {
        std::cerr << "Executable not found: " << executable_path.string() << '\n';
        return false;
    }

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};
    std::wstring command_line = quote_windows_argument(executable_path);
    for (const std::string& argument : arguments) {
        command_line += L" ";
        command_line += quote_windows_argument(argument);
    }

    const BOOL created = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                                        app_paths::executable_dir().wstring().c_str(), &startup_info, &process_info);
    if (created == FALSE) {
        std::cerr << "Failed to launch game process.\n";
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return true;
}

bool launch_game_process() {
    return launch_process(app_paths::executable_dir() / "raythm.exe", {});
}

bool launch_updater_process(const updater::update_launch_request& request) {
    return launch_process(app_paths::executable_dir() / "Updater.exe", updater::build_updater_arguments(request));
}
#else
bool launch_game_process() {
    return false;
}

bool launch_updater_process(const updater::update_launch_request&) {
    return false;
}
#endif

}  // namespace

int main() {
    const auto package_version = updater::parse_semantic_version(kCurrentPackageVersion);
    if (!package_version.has_value()) {
        std::cerr << "Invalid launcher package version.\n";
        return EXIT_FAILURE;
    }

    updater::ensure_update_directories();
    updater::ensure_installed_version_file(*package_version);

    const auto installed_version = updater::load_installed_version();
    if (!installed_version.has_value()) {
        std::cerr << "Failed to load installed version.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Launcher initialized with installed version "
              << updater::to_string(installed_version->version) << '\n';

    const auto latest_release = updater::fetch_latest_release_info();
    if (latest_release.has_value()) {
        const bool update_available = updater::is_newer_version(latest_release->version, installed_version->version);
        std::cout << "Latest GitHub release: " << latest_release->tag_name
                  << (update_available ? " (update available)" : " (up to date)") << '\n';
        if (!latest_release->assets.package_url.empty()) {
            std::cout << "Package URL: " << latest_release->assets.package_url << '\n';
        }
        if (!latest_release->assets.checksum_url.empty()) {
            std::cout << "Checksum URL: " << latest_release->assets.checksum_url << '\n';
        }

        if (update_available && !latest_release->assets.package_url.empty()) {
            const updater::update_launch_request request{installed_version->version, *latest_release};
            if (!launch_updater_process(request)) {
                std::cerr << "Failed to launch updater process.\n";
                return EXIT_FAILURE;
            }
            std::cout << "Updater launched for " << latest_release->tag_name << '\n';
            return EXIT_SUCCESS;
        }
    } else {
        std::cout << "Latest GitHub release could not be fetched; continuing with installed version.\n";
    }

    if (!launch_game_process()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
