#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "app_paths.h"
#include "updater/update_apply.h"
#include "updater/update_download.h"
#include "updater/update_extract.h"
#include "updater/update_log.h"
#include "updater/update_process.h"
#include "updater/update_paths.h"
#include "updater/update_version.h"
#include "updater/update_verify.h"
#include "updater/update_workflow.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

#ifdef _WIN32
std::wstring widen(std::string_view value) {
    return std::wstring(value.begin(), value.end());
}

std::wstring quote_windows_argument(std::wstring value) {
    std::wstring quoted = L"\"";
    for (const wchar_t ch : value) {
        if (ch == L'"') {
            quoted += L'\\';
        }
        quoted += ch;
    }
    quoted += L"\"";
    return quoted;
}

bool current_process_is_elevated() {
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) == FALSE) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok != FALSE && elevation.TokenIsElevated != 0;
}

bool can_write_to_directory(const std::filesystem::path& directory_path) {
    const std::filesystem::path probe_path = directory_path / ".raythm_write_probe.tmp";
    std::ofstream output(probe_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << "probe";
    output.close();
    std::error_code ec;
    std::filesystem::remove(probe_path, ec);
    return true;
}

std::wstring build_argument_string(int argc, char* argv[]) {
    std::wstring arguments;
    for (int index = 1; index < argc; ++index) {
        if (index > 1) {
            arguments += L' ';
        }
        arguments += quote_windows_argument(widen(argv[index]));
    }
    return arguments;
}

int relaunch_self_elevated(int argc, char* argv[]) {
    wchar_t executable_buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, executable_buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return EXIT_FAILURE;
    }

    SHELLEXECUTEINFOW execute_info{};
    execute_info.cbSize = sizeof(execute_info);
    execute_info.fMask = SEE_MASK_NOCLOSEPROCESS;
    execute_info.lpVerb = L"runas";
    execute_info.lpFile = executable_buffer;
    const std::wstring parameters = build_argument_string(argc, argv);
    execute_info.lpParameters = parameters.empty() ? nullptr : parameters.c_str();
    execute_info.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&execute_info) == FALSE) {
        return EXIT_FAILURE;
    }

    WaitForSingleObject(execute_info.hProcess, INFINITE);
    DWORD exit_code = EXIT_FAILURE;
    GetExitCodeProcess(execute_info.hProcess, &exit_code);
    CloseHandle(execute_info.hProcess);
    return static_cast<int>(exit_code);
}

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
    updater::append_update_log("updater", "updater started");

    const auto installed_version = updater::load_installed_version();
    if (installed_version.has_value()) {
        updater::append_update_log("updater", "installed version is " + updater::to_string(installed_version->version));
        std::cout << "Updater initialized for installed version "
                  << updater::to_string(installed_version->version) << '\n';
    } else {
        updater::append_update_log("updater", "installed version metadata missing");
        std::cout << "Updater initialized without installed version metadata\n";
    }

    const auto request = updater::parse_updater_arguments(argc, argv);
    if (request.has_value()) {
        std::filesystem::path package_path;
        std::filesystem::path checksum_path;

        std::cout << "Requested update from " << updater::to_string(request->current_version)
                  << " to " << request->target_release.tag_name << '\n';
        updater::append_update_log("updater",
                                   "requested update from " + updater::to_string(request->current_version) +
                                       " to " + request->target_release.tag_name);
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
            updater::append_update_log("updater", "downloading package to " + package_path.string());
            if (!updater::download_url_to_file(request->target_release.assets.package_url, package_path)) {
                updater::append_update_log("updater", "package download failed");
                std::cerr << "Failed to download package.\n";
                return EXIT_FAILURE;
            }
            updater::append_update_log("updater", "package download completed");
        }

        if (!request->target_release.assets.checksum_url.empty()) {
            checksum_path =
                updater::downloads_root() /
                updater::file_name_from_url(request->target_release.assets.checksum_url, "SHA256SUMS.txt");
            std::cout << "Downloading checksum to " << checksum_path.string() << '\n';
            updater::append_update_log("updater", "downloading checksum to " + checksum_path.string());
            if (!updater::download_url_to_file(request->target_release.assets.checksum_url, checksum_path)) {
                updater::append_update_log("updater", "checksum download failed");
                std::cerr << "Failed to download checksum file.\n";
                return EXIT_FAILURE;
            }
            updater::append_update_log("updater", "checksum download completed");
        }

        if (!package_path.empty() && !checksum_path.empty()) {
            std::cout << "Verifying SHA-256 checksum\n";
            updater::append_update_log("updater", "verifying SHA-256 checksum");
            if (!updater::verify_sha256_checksum(package_path, checksum_path)) {
                updater::append_update_log("updater", "SHA-256 verification failed");
                std::cerr << "SHA-256 verification failed.\n";
                return EXIT_FAILURE;
            }
            updater::append_update_log("updater", "SHA-256 verification succeeded");
        }

        if (!package_path.empty()) {
            std::cout << "Extracting package to " << updater::staging_root().string() << '\n';
            updater::append_update_log("updater", "extracting package to staging");
            if (!updater::extract_zip_to_directory(package_path, updater::staging_root())) {
                updater::append_update_log("updater", "package extraction failed");
                std::cerr << "Failed to extract package into staging.\n";
                return EXIT_FAILURE;
            }
            updater::append_update_log("updater", "package extraction succeeded");

            if (!current_process_is_elevated() && !can_write_to_directory(app_paths::executable_dir())) {
                updater::append_update_log("updater", "install directory requires elevation; relaunching updater");
                return relaunch_self_elevated(argc, argv);
            }

            std::cout << "Waiting for raythm.exe to stop\n";
            updater::append_update_log("updater", "waiting for raythm.exe to stop");
            if (!updater::ensure_process_stopped("raythm.exe", std::chrono::seconds(2))) {
                updater::append_update_log("updater", "failed to stop raythm.exe");
                std::cerr << "Failed to stop running game process.\n";
                return EXIT_FAILURE;
            }
            updater::append_update_log("updater", "raythm.exe is stopped");

            std::cout << "Applying staged update to " << app_paths::executable_dir().string() << '\n';
            updater::append_update_log("updater", "applying staged update");
            if (!updater::apply_staged_update(app_paths::executable_dir(),
                                              updater::staging_root(),
                                              updater::backup_root() / "current")) {
                updater::append_update_log("updater", "failed to apply staged update");
                std::cerr << "Failed to apply staged update.\n";
                return EXIT_FAILURE;
            }
            updater::append_update_log("updater", "staged update applied");

            if (!updater::save_installed_version({request->target_release.version})) {
                updater::append_update_log("updater", "failed to save installed version");
                std::cerr << "Failed to update installed version metadata.\n";
                return EXIT_FAILURE;
            }
            updater::append_update_log("updater", "installed version metadata updated");

            if (!launch_game_process()) {
                updater::append_update_log("updater", "failed to launch updated game");
                std::cerr << "Failed to launch updated game.\n";
                return EXIT_FAILURE;
            }
            updater::append_update_log("updater", "updated game launched");
        }
    } else {
        updater::append_update_log("updater", "updater started without launch request");
        std::cout << "Updater started without a launch request\n";
    }

    updater::append_update_log("updater", "updater workflow completed");
    std::cout << "Updater workflow completed.\n";
    return EXIT_SUCCESS;
}
