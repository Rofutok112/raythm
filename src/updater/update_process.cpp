#include "updater/update_process.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace {

std::string to_lower_ascii(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

#ifdef _WIN32
std::string narrow_process_name(const wchar_t* value) {
    std::string result;
    while (*value != L'\0') {
        result.push_back(static_cast<char>(*value & 0xff));
        ++value;
    }
    return result;
}

std::vector<DWORD> find_process_ids_by_name(std::string_view process_name) {
    std::vector<DWORD> process_ids;
    const std::string expected_name = to_lower_ascii(process_name);
    const DWORD current_pid = GetCurrentProcessId();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return process_ids;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry) != FALSE) {
        do {
            if (entry.th32ProcessID == current_pid) {
                continue;
            }
            if (to_lower_ascii(narrow_process_name(entry.szExeFile)) == expected_name) {
                process_ids.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snapshot, &entry) != FALSE);
    }

    CloseHandle(snapshot);
    return process_ids;
}
#endif

}  // namespace

namespace updater {

bool process_name_matches(std::string_view process_name, std::string_view expected_name) {
    return to_lower_ascii(process_name) == to_lower_ascii(expected_name);
}

bool ensure_process_stopped(std::string_view process_name, std::chrono::milliseconds wait_timeout) {
#ifdef _WIN32
    const auto deadline = std::chrono::steady_clock::now() + wait_timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (find_process_ids_by_name(process_name).empty()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (const DWORD process_id : find_process_ids_by_name(process_name)) {
        HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, process_id);
        if (process == nullptr) {
            return false;
        }

        const BOOL terminated = TerminateProcess(process, 0);
        if (terminated == FALSE) {
            CloseHandle(process);
            return false;
        }

        WaitForSingleObject(process, 5000);
        CloseHandle(process);
    }

    return find_process_ids_by_name(process_name).empty();
#else
    (void)process_name;
    (void)wait_timeout;
    return false;
#endif
}

}  // namespace updater
