#include "updater/update_release.h"

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace {

constexpr std::string_view kLatestReleaseHost = "api.github.com";
constexpr std::string_view kLatestReleasePath = "/repos/Rofutok112/raythm/releases/latest";
constexpr std::string_view kPackageAssetName = "game-win64.zip";
constexpr std::string_view kChecksumAssetName = "SHA256SUMS.txt";

std::optional<std::string> extract_json_string_at(const std::string& content, size_t key_pos) {
    const size_t colon_pos = content.find(':', key_pos);
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    size_t start = colon_pos + 1;
    while (start < content.size() && std::isspace(static_cast<unsigned char>(content[start]))) {
        ++start;
    }

    if (start >= content.size() || content[start] != '"') {
        return std::nullopt;
    }
    ++start;

    std::string result;
    for (size_t i = start; i < content.size(); ++i) {
        if (content[i] == '"') {
            return result;
        }
        if (content[i] == '\\' && i + 1 < content.size()) {
            result += content[++i];
        } else {
            result += content[i];
        }
    }

    return std::nullopt;
}

std::optional<std::string> extract_json_string(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    return extract_json_string_at(content, key_pos + token.size());
}

std::optional<std::string> find_asset_download_url(const std::string& content, std::string_view asset_name) {
    const std::string asset_token = "\"name\"";
    size_t search_pos = 0;
    while (true) {
        const size_t name_key_pos = content.find(asset_token, search_pos);
        if (name_key_pos == std::string::npos) {
            return std::nullopt;
        }

        const std::optional<std::string> current_name = extract_json_string_at(content, name_key_pos + asset_token.size());
        if (current_name.has_value() && *current_name == asset_name) {
            const size_t download_key_pos = content.find("\"browser_download_url\"", name_key_pos);
            if (download_key_pos == std::string::npos) {
                return std::nullopt;
            }

            const size_t next_name_key_pos = content.find(asset_token, name_key_pos + asset_token.size());
            if (next_name_key_pos != std::string::npos && download_key_pos > next_name_key_pos) {
                search_pos = next_name_key_pos;
                continue;
            }

            return extract_json_string_at(content, download_key_pos + std::string("\"browser_download_url\"").size());
        }

        search_pos = name_key_pos + asset_token.size();
    }
}

#ifdef _WIN32
std::wstring to_wstring(std::string_view value) {
    return std::wstring(value.begin(), value.end());
}

std::optional<std::string> http_get_text(std::string_view host, std::string_view path) {
    HINTERNET session = WinHttpOpen(L"raythm-launcher/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (session == nullptr) {
        return std::nullopt;
    }

    HINTERNET connection = WinHttpConnect(session, to_wstring(host).c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connection == nullptr) {
        WinHttpCloseHandle(session);
        return std::nullopt;
    }

    HINTERNET request = WinHttpOpenRequest(connection,
                                           L"GET",
                                           to_wstring(path).c_str(),
                                           nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (request == nullptr) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return std::nullopt;
    }

    constexpr wchar_t kHeaders[] =
        L"User-Agent: raythm-launcher/1.0\r\n"
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n";
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
        return std::nullopt;
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
        return std::nullopt;
    }

    std::string response_body;
    DWORD available_size = 0;
    while (WinHttpQueryDataAvailable(request, &available_size) == TRUE && available_size > 0) {
        std::string chunk(available_size, '\0');
        DWORD bytes_read = 0;
        if (WinHttpReadData(request, chunk.data(), available_size, &bytes_read) == FALSE) {
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return std::nullopt;
        }
        chunk.resize(bytes_read);
        response_body += chunk;
        available_size = 0;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return response_body;
}
#endif

}  // namespace

namespace updater {

std::optional<latest_release_info> parse_latest_release_response(const std::string& response_body) {
    const std::optional<std::string> tag_name = extract_json_string(response_body, "tag_name");
    if (!tag_name.has_value()) {
        return std::nullopt;
    }

    const std::optional<semantic_version> version = parse_semantic_version(*tag_name);
    if (!version.has_value()) {
        return std::nullopt;
    }

    latest_release_info release{*version, *tag_name, {}};
    if (const std::optional<std::string> package_url = find_asset_download_url(response_body, kPackageAssetName)) {
        release.assets.package_url = *package_url;
    }
    if (const std::optional<std::string> checksum_url = find_asset_download_url(response_body, kChecksumAssetName)) {
        release.assets.checksum_url = *checksum_url;
    }

    return release;
}

std::optional<latest_release_info> fetch_latest_release_info() {
#ifdef _WIN32
    const std::optional<std::string> response_body = http_get_text(kLatestReleaseHost, kLatestReleasePath);
    if (!response_body.has_value()) {
        return std::nullopt;
    }
    return parse_latest_release_response(*response_body);
#else
    return std::nullopt;
#endif
}

}  // namespace updater
