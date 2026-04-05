#include "updater/update_extract.h"

#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
std::wstring quote_powershell_argument(const std::filesystem::path& path) {
    std::wstring value = path.wstring();
    size_t cursor = 0;
    while ((cursor = value.find(L"'", cursor)) != std::wstring::npos) {
        value.replace(cursor, 1, L"''");
        cursor += 2;
    }
    return L"'" + value + L"'";
}

bool run_powershell_command(const std::wstring& script) {
    std::wstring command_line =
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" + script + L"\"";

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};

    const BOOL created = CreateProcessW(nullptr,
                                        command_line.data(),
                                        nullptr,
                                        nullptr,
                                        FALSE,
                                        CREATE_NO_WINDOW,
                                        nullptr,
                                        nullptr,
                                        &startup_info,
                                        &process_info);
    if (created == FALSE) {
        return false;
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);

    DWORD exit_code = 1;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return exit_code == 0;
}
#endif

}  // namespace

namespace updater {

bool reset_directory(const std::filesystem::path& directory_path) {
    std::error_code ec;
    std::filesystem::remove_all(directory_path, ec);
    ec.clear();
    std::filesystem::create_directories(directory_path, ec);
    return !ec;
}

bool extract_zip_to_directory(const std::filesystem::path& zip_path, const std::filesystem::path& destination_directory) {
#ifdef _WIN32
    if (!std::filesystem::exists(zip_path) || !reset_directory(destination_directory)) {
        return false;
    }

    const std::wstring script =
        L"$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath " + quote_powershell_argument(zip_path) +
        L" -DestinationPath " + quote_powershell_argument(destination_directory) + L" -Force";
    return run_powershell_command(script);
#else
    (void)zip_path;
    (void)destination_directory;
    return false;
#endif
}

}  // namespace updater
