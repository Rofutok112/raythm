#include "network/http_client.h"

#include <optional>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace network::http {
namespace {

#ifdef _WIN32
struct url_parts {
    std::wstring host;
    std::wstring path_and_query;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
    bool secure = false;
};

constexpr int kResolveTimeoutMs = 5000;
constexpr int kConnectTimeoutMs = 5000;
constexpr int kSendTimeoutMs = 5000;
constexpr int kReceiveTimeoutMs = 5000;

std::wstring to_wstring(std::string_view value) {
    return std::wstring(value.begin(), value.end());
}

std::string describe_winhttp_error(DWORD error_code) {
    switch (error_code) {
        case ERROR_WINHTTP_TIMEOUT:
            return "The connection to raythm-Server timed out.";
        case ERROR_WINHTTP_CANNOT_CONNECT:
            return "Could not connect to raythm-Server.";
        case ERROR_WINHTTP_CONNECTION_ERROR:
            return "The connection to raythm-Server was interrupted.";
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
            return "The server name could not be resolved.";
        case ERROR_WINHTTP_INVALID_URL:
            return "The server URL is invalid.";
        case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:
            return "The server URL scheme is not supported.";
        case ERROR_WINHTTP_SECURE_FAILURE:
            return "A secure connection to raythm-Server could not be established.";
        default:
            return "Failed to communicate with raythm-Server.";
    }
}

std::optional<url_parts> parse_url_parts(const std::string& url) {
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

    url_parts parts;
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

#ifdef _WIN32
response send_request(const std::string& method,
                      const std::string& url,
                      const std::vector<std::pair<std::string, std::string>>& headers,
                      const std::string& body) {
    response result;

    const std::optional<url_parts> parts = parse_url_parts(url);
    if (!parts.has_value()) {
        result.error_message = "Invalid server URL.";
        return result;
    }

    HINTERNET session = WinHttpOpen(L"raythm/0.1",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (session == nullptr) {
        result.error_message = describe_winhttp_error(GetLastError());
        return result;
    }

    WinHttpSetTimeouts(session, kResolveTimeoutMs, kConnectTimeoutMs, kSendTimeoutMs, kReceiveTimeoutMs);

    HINTERNET connection = WinHttpConnect(session, parts->host.c_str(), parts->port, 0);
    if (connection == nullptr) {
        result.error_message = describe_winhttp_error(GetLastError());
        WinHttpCloseHandle(session);
        return result;
    }

    const DWORD request_flags = parts->secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection,
                                           to_wstring(method).c_str(),
                                           parts->path_and_query.c_str(),
                                           nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           request_flags);
    if (request == nullptr) {
        result.error_message = describe_winhttp_error(GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    std::wstring header_block;
    for (const auto& [name, value] : headers) {
        header_block += to_wstring(name);
        header_block += L": ";
        header_block += to_wstring(value);
        header_block += L"\r\n";
    }

    LPVOID request_body = body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data());
    const DWORD request_body_size = static_cast<DWORD>(body.size());
    const BOOL sent = WinHttpSendRequest(request,
                                         header_block.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : header_block.c_str(),
                                         header_block.empty() ? 0 : static_cast<DWORD>(-1L),
                                         request_body,
                                         request_body_size,
                                         request_body_size,
                                         0);
    if (sent == FALSE || WinHttpReceiveResponse(request, nullptr) == FALSE) {
        result.error_message = describe_winhttp_error(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    DWORD status_code = 0;
    DWORD status_code_size = sizeof(status_code);
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status_code,
                            &status_code_size,
                            WINHTTP_NO_HEADER_INDEX) == FALSE) {
        result.error_message = describe_winhttp_error(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    result.status_code = static_cast<int>(status_code);

    DWORD available_size = 0;
    while (WinHttpQueryDataAvailable(request, &available_size) == TRUE && available_size > 0) {
        std::string chunk(available_size, '\0');
        DWORD bytes_read = 0;
        if (WinHttpReadData(request, chunk.data(), available_size, &bytes_read) == FALSE) {
            result.error_message = describe_winhttp_error(GetLastError());
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return result;
        }

        chunk.resize(bytes_read);
        result.body += chunk;
        available_size = 0;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return result;
}
#else
response send_request(const std::string&,
                      const std::string&,
                      const std::vector<std::pair<std::string, std::string>>&,
                      const std::string&) {
    return {
        .status_code = 0,
        .body = {},
        .error_message = "Networking is only supported on Windows in the current build.",
    };
}
#endif

}  // namespace network::http
