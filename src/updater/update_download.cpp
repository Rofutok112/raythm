#include "updater/update_download.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace {

#ifdef _WIN32
std::wstring to_wstring(std::string_view value) {
    return std::wstring(value.begin(), value.end());
}

struct http_url_parts {
    std::wstring host;
    std::wstring path_and_query;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool secure = true;
};

std::optional<http_url_parts> parse_url_parts(const std::string& url) {
    std::wstring wide_url = to_wstring(url);

    URL_COMPONENTSW components{};
    components.dwStructSize = sizeof(components);

    wchar_t host_buffer[256];
    wchar_t path_buffer[2048];
    wchar_t extra_buffer[2048];
    components.lpszHostName = host_buffer;
    components.dwHostNameLength = sizeof(host_buffer) / sizeof(wchar_t);
    components.lpszUrlPath = path_buffer;
    components.dwUrlPathLength = sizeof(path_buffer) / sizeof(wchar_t);
    components.lpszExtraInfo = extra_buffer;
    components.dwExtraInfoLength = sizeof(extra_buffer) / sizeof(wchar_t);

    if (WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &components) == FALSE) {
        return std::nullopt;
    }

    http_url_parts parts;
    parts.host.assign(components.lpszHostName, components.dwHostNameLength);
    parts.path_and_query.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0) {
        parts.path_and_query.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    parts.port = components.nPort;
    parts.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    return parts;
}
#endif

}  // namespace

namespace updater {

std::string file_name_from_url(const std::string& url, const std::string& fallback_name) {
    const size_t query_pos = url.find_first_of("?#");
    const std::string without_query = url.substr(0, query_pos);
    const size_t slash_pos = without_query.find_last_of('/');
    if (slash_pos == std::string::npos || slash_pos + 1 >= without_query.size()) {
        return fallback_name;
    }

    const std::string file_name = without_query.substr(slash_pos + 1);
    if (file_name.empty()) {
        return fallback_name;
    }
    return file_name;
}

bool download_url_to_file(const std::string& url, const std::filesystem::path& destination_path) {
#ifdef _WIN32
    const std::optional<http_url_parts> parts = parse_url_parts(url);
    if (!parts.has_value()) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(destination_path.parent_path(), ec);

    const std::filesystem::path temp_path = destination_path.string() + ".part";
    std::filesystem::remove(temp_path, ec);

    HINTERNET session = WinHttpOpen(L"raythm-updater/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (session == nullptr) {
        return false;
    }

    HINTERNET connection = WinHttpConnect(session, parts->host.c_str(), parts->port, 0);
    if (connection == nullptr) {
        WinHttpCloseHandle(session);
        return false;
    }

    const DWORD request_flags = parts->secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection,
                                           L"GET",
                                           parts->path_and_query.c_str(),
                                           nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           request_flags);
    if (request == nullptr) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    constexpr wchar_t kHeaders[] = L"User-Agent: raythm-updater/1.0\r\n";
    const BOOL sent = WinHttpSendRequest(request,
                                         kHeaders,
                                         static_cast<DWORD>(-1L),
                                         WINHTTP_NO_REQUEST_DATA,
                                         0,
                                         0,
                                         0);
    if (sent == FALSE || WinHttpReceiveResponse(request, nullptr) == FALSE) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status_code = 0;
    DWORD status_code_size = sizeof(status_code);
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status_code,
                            &status_code_size,
                            WINHTTP_NO_HEADER_INDEX) == FALSE ||
        status_code != 200) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    std::ofstream output(temp_path, std::ios::binary);
    if (!output.is_open()) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD available_size = 0;
    while (WinHttpQueryDataAvailable(request, &available_size) == TRUE && available_size > 0) {
        std::string chunk(available_size, '\0');
        DWORD bytes_read = 0;
        if (WinHttpReadData(request, chunk.data(), available_size, &bytes_read) == FALSE) {
            output.close();
            std::filesystem::remove(temp_path, ec);
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return false;
        }

        output.write(chunk.data(), static_cast<std::streamsize>(bytes_read));
        if (!output) {
            output.close();
            std::filesystem::remove(temp_path, ec);
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return false;
        }
        available_size = 0;
    }

    output.close();
    std::filesystem::remove(destination_path, ec);
    std::filesystem::rename(temp_path, destination_path, ec);
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return true;
#else
    (void)url;
    (void)destination_path;
    return false;
#endif
}

}  // namespace updater
