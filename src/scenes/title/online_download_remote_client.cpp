#include "title/online_download_remote_client.h"

#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "network/auth_client.h"
#include "network/json_helpers.h"
#include "title/online_download_view.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace title_online_view {
namespace {
namespace json = network::json;

struct http_response {
    int status_code = 0;
    std::string body;
    std::string content_type;
    size_t content_length = 0;
    std::string error_message;
};

struct remote_song_fetch_result {
    std::vector<remote_song_payload> songs;
    bool success = false;
    std::string error_message;
};

struct remote_chart_fetch_result {
    std::vector<remote_chart_payload> charts;
    bool success = false;
    std::string error_message;
};

constexpr int kRemotePageSize = 100;

#ifdef _WIN32
struct http_url_parts {
    std::wstring host;
    std::wstring path_and_query;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
    bool secure = false;
};

constexpr int kResolveTimeoutMs = 5000;
constexpr int kConnectTimeoutMs = 5000;
constexpr int kSendTimeoutMs = 5000;
constexpr int kReceiveTimeoutMs = 5000;
#endif

bool is_absolute_remote_url(std::string_view value) {
    return value.rfind("http://", 0) == 0 || value.rfind("https://", 0) == 0;
}

void push_candidate_server_url(std::vector<std::string>& urls, std::string url) {
    url = auth::normalize_server_url(json::trim(url));
    if (url.empty()) {
        return;
    }

    for (const std::string& existing : urls) {
        if (existing == url) {
            return;
        }
    }
    urls.push_back(std::move(url));
}

std::vector<std::string> resolve_server_urls() {
    std::vector<std::string> urls;
    if (const std::optional<auth::session> saved = auth::load_saved_session(); saved.has_value()) {
        push_candidate_server_url(urls, saved->server_url);
    }

    push_candidate_server_url(urls, auth::kDefaultServerUrl);
    push_candidate_server_url(urls, auth::kLanServerUrl);
    push_candidate_server_url(urls, "http://127.0.0.1:3000");
    push_candidate_server_url(urls, "http://localhost:3000");
    push_candidate_server_url(urls, "http://127.0.0.1");
    push_candidate_server_url(urls, "http://localhost");
    return urls;
}

std::string build_paged_url(const std::string& server_url, const std::string& path, int page) {
    std::ostringstream stream;
    stream << server_url << path << "?page=" << page << "&pageSize=" << kRemotePageSize;
    return stream.str();
}

std::string build_song_page_url(const std::string& server_url,
                                catalog_mode mode,
                                int page,
                                int page_size) {
    std::ostringstream stream;
    stream << server_url << "/songs?page=" << page << "&pageSize=" << page_size;
    if (mode == catalog_mode::official) {
        stream << "&contentSource=official";
    } else if (mode == catalog_mode::community) {
        stream << "&contentSource=community";
    }
    return stream.str();
}

std::string build_chart_page_url(const std::string& server_url,
                                 const std::string& song_id,
                                 int page,
                                 int page_size) {
    std::ostringstream stream;
    stream << server_url << "/charts?page=" << page << "&pageSize=" << page_size;
    if (!song_id.empty()) {
        stream << "&songId=" << song_id;
    }
    return stream.str();
}

std::vector<std::string> prioritize_server_url(const std::string& preferred_server_url) {
    std::vector<std::string> urls;
    push_candidate_server_url(urls, preferred_server_url);
    for (const std::string& server_url : resolve_server_urls()) {
        push_candidate_server_url(urls, server_url);
    }
    return urls;
}

#ifdef _WIN32
std::wstring widen_utf8(const std::string& input) {
    if (input.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        return {};
    }

    std::wstring output(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, output.data(), required);
    output.resize(static_cast<size_t>(required) - 1);
    return output;
}

std::optional<http_url_parts> parse_url(const std::string& url) {
    const std::wstring wide_url = widen_utf8(url);
    if (wide_url.empty()) {
        return std::nullopt;
    }

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &components) == FALSE) {
        return std::nullopt;
    }

    http_url_parts parts;
    parts.host.assign(components.lpszHostName, components.dwHostNameLength);
    parts.path_and_query.assign(components.lpszUrlPath, components.dwUrlPathLength);
    parts.path_and_query.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    parts.port = components.nPort;
    parts.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    return parts;
}

std::string narrow_utf8(const std::wstring& input) {
    if (input.empty()) {
        return {};
    }
    const int required = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string output(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()),
                        output.data(), required, nullptr, nullptr);
    return output;
}

std::string query_content_type(HINTERNET request) {
    DWORD size_bytes = 0;
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_CONTENT_TYPE,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            WINHTTP_NO_OUTPUT_BUFFER,
                            &size_bytes,
                            WINHTTP_NO_HEADER_INDEX) == FALSE &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return {};
    }

    if (size_bytes == 0) {
        return {};
    }

    std::wstring buffer(size_bytes / sizeof(wchar_t), L'\0');
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_CONTENT_TYPE,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            buffer.data(),
                            &size_bytes,
                            WINHTTP_NO_HEADER_INDEX) == FALSE) {
        return {};
    }

    while (!buffer.empty() && buffer.back() == L'\0') {
        buffer.pop_back();
    }
    return narrow_utf8(buffer);
}

size_t query_content_length(HINTERNET request) {
    DWORD64 content_length = 0;
    DWORD size_bytes = sizeof(content_length);
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER64,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &content_length,
                            &size_bytes,
                            WINHTTP_NO_HEADER_INDEX) == FALSE) {
        return 0;
    }

    return static_cast<size_t>(content_length);
}

http_response send_request(const std::string& method,
                           const std::string& url,
                           const remote_binary_progress_callback& progress_callback = {}) {
    http_response response;
    const auto parts = parse_url(url);
    if (!parts.has_value()) {
        response.error_message = "Invalid URL.";
        return response;
    }

    HINTERNET session = WinHttpOpen(L"raythm/0.1",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (session == nullptr) {
        response.error_message = "Failed to initialize WinHTTP.";
        return response;
    }

    WinHttpSetTimeouts(session, kResolveTimeoutMs, kConnectTimeoutMs, kSendTimeoutMs, kReceiveTimeoutMs);

    HINTERNET connection = WinHttpConnect(session, parts->host.c_str(), parts->port, 0);
    if (connection == nullptr) {
        response.error_message = "Failed to connect to server.";
        WinHttpCloseHandle(session);
        return response;
    }

    const std::wstring wide_method = widen_utf8(method);
    HINTERNET request = WinHttpOpenRequest(connection,
                                           wide_method.c_str(),
                                           parts->path_and_query.c_str(),
                                           nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           parts->secure ? WINHTTP_FLAG_SECURE : 0);
    if (request == nullptr) {
        response.error_message = "Failed to create HTTP request.";
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }

    const BOOL sent = WinHttpSendRequest(request,
                                         WINHTTP_NO_ADDITIONAL_HEADERS,
                                         0,
                                         WINHTTP_NO_REQUEST_DATA,
                                         0,
                                         0,
                                         0);
    if (sent == FALSE || WinHttpReceiveResponse(request, nullptr) == FALSE) {
        response.error_message = "Failed to complete HTTP request.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status_code,
                            &status_size,
                            WINHTTP_NO_HEADER_INDEX) == FALSE) {
        response.error_message = "Failed to read HTTP status.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }
    response.status_code = static_cast<int>(status_code);
    response.content_type = query_content_type(request);
    response.content_length = query_content_length(request);

    DWORD available_size = 0;
    size_t total_bytes_read = 0;
    while (WinHttpQueryDataAvailable(request, &available_size) == TRUE && available_size > 0) {
        std::string chunk(static_cast<size_t>(available_size), '\0');
        DWORD bytes_read = 0;
        if (WinHttpReadData(request, chunk.data(), available_size, &bytes_read) == FALSE) {
            response.error_message = "Failed to read HTTP response.";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return response;
        }
        chunk.resize(static_cast<size_t>(bytes_read));
        response.body += chunk;
        total_bytes_read += static_cast<size_t>(bytes_read);
        if (progress_callback) {
            progress_callback(total_bytes_read, response.content_length);
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return response;
}
#else
http_response send_request(const std::string&, const std::string&) {
    return {
        0,
        {},
        {},
        "Remote browse is only supported on Windows builds right now.",
    };
}
#endif

std::optional<remote_song_payload> parse_remote_song(const std::string& object) {
    const auto id = json::extract_string(object, "id");
    const auto title = json::extract_string(object, "title");
    const auto artist = json::extract_string(object, "artist");
    if (!id.has_value() || !title.has_value() || !artist.has_value()) {
        return std::nullopt;
    }

    return remote_song_payload{
        .id = *id,
        .title = *title,
        .artist = *artist,
        .base_bpm = json::extract_float(object, "baseBpm")
            .value_or(json::extract_float(object, "base_bpm").value_or(0.0f)),
        .duration_seconds = json::extract_float(object, "durationSec")
            .value_or(json::extract_float(object, "duration_seconds").value_or(0.0f)),
        .preview_start_ms = json::extract_int(object, "previewStartMs")
            .value_or(json::extract_int(object, "preview_start_ms").value_or(0)),
        .song_version = json::extract_int(object, "songVersion")
            .value_or(json::extract_int(object, "song_version").value_or(0)),
        .content_source = json::extract_string(object, "contentSource")
            .value_or(json::extract_string(object, "content_source").value_or("community")),
        .audio_url = json::extract_string(object, "audioUrl")
            .value_or(json::extract_string(object, "audio_url").value_or("")),
        .jacket_url = json::extract_string(object, "jacketUrl")
            .value_or(json::extract_string(object, "jacket_url").value_or("")),
    };
}

std::optional<remote_chart_payload> parse_remote_chart(const std::string& object) {
    const auto id = json::extract_string(object, "id");
    std::optional<std::string> song_id = json::extract_string(object, "songId");
    if (!song_id.has_value()) {
        song_id = json::extract_string(object, "song_id");
    }
    std::optional<std::string> difficulty_name = json::extract_string(object, "difficultyName");
    if (!difficulty_name.has_value()) {
        difficulty_name = json::extract_string(object, "difficulty_name");
    }
    if (!id.has_value() || !song_id.has_value() || !difficulty_name.has_value()) {
        return std::nullopt;
    }

    const float calculated_level = json::extract_float(object, "calculatedLevel")
        .value_or(json::extract_float(object, "calculated_level")
        .value_or(json::extract_float(object, "level").value_or(0.0f)));

    return remote_chart_payload{
        .id = *id,
        .song_id = *song_id,
        .key_count = json::extract_int(object, "keyCount").value_or(4),
        .difficulty_name = *difficulty_name,
        .level = calculated_level,
        .chart_author = json::extract_string(object, "chartAuthor")
            .value_or(json::extract_string(object, "chart_author").value_or("")),
        .format_version = json::extract_int(object, "formatVersion")
            .value_or(json::extract_int(object, "format_version").value_or(0)),
        .resolution = json::extract_int(object, "resolution").value_or(0),
        .offset = json::extract_int(object, "offset").value_or(0),
        .note_count = json::extract_int(object, "noteCount")
            .value_or(json::extract_int(object, "note_count").value_or(0)),
        .min_bpm = json::extract_float(object, "minBpm")
            .value_or(json::extract_float(object, "min_bpm").value_or(0.0f)),
        .max_bpm = json::extract_float(object, "maxBpm")
            .value_or(json::extract_float(object, "max_bpm").value_or(0.0f)),
        .difficulty_ruleset_id = json::extract_string(object, "difficultyRulesetId")
            .value_or(json::extract_string(object, "difficulty_ruleset_id").value_or("")),
        .difficulty_ruleset_version = json::extract_int(object, "difficultyRulesetVersion")
            .value_or(json::extract_int(object, "difficulty_ruleset_version").value_or(0)),
        .chart_fingerprint = json::extract_string(object, "chartFingerprint")
            .value_or(json::extract_string(object, "chart_fingerprint").value_or("")),
        .chart_sha256 = json::extract_string(object, "chartSha256")
            .value_or(json::extract_string(object, "chart_sha256").value_or("")),
        .content_source = json::extract_string(object, "contentSource").value_or("community"),
    };
}

remote_song_fetch_result fetch_remote_songs(const std::string& server_url) {
    remote_song_fetch_result result;
    int page = 1;
    int total = -1;

    while (true) {
        const http_response response = send_request("GET", build_paged_url(server_url, "/songs", page));
        if (!response.error_message.empty()) {
            result.error_message = response.error_message;
            return result;
        }
        if (response.status_code < 200 || response.status_code >= 300) {
            result.error_message =
                "raythm-Server returned HTTP " + std::to_string(response.status_code) + " for /songs.";
            return result;
        }

        const std::optional<std::string> items_array = json::extract_array(response.body, "items");
        if (!items_array.has_value()) {
            result.error_message = "raythm-Server returned an unexpected song catalog response.";
            return result;
        }

        const std::vector<std::string> objects = json::extract_objects_from_array(*items_array);
        for (const std::string& object : objects) {
            const auto song = parse_remote_song(object);
            if (song.has_value()) {
                result.songs.push_back(*song);
            }
        }

        total = json::extract_int(response.body, "total").value_or(static_cast<int>(result.songs.size()));
        if (result.songs.empty() || static_cast<int>(result.songs.size()) >= total ||
            static_cast<int>(objects.size()) < kRemotePageSize) {
            break;
        }
        ++page;
    }

    result.success = true;
    return result;
}

remote_song_page_fetch_result fetch_remote_song_page_from_server(const std::string& server_url,
                                                                 catalog_mode mode,
                                                                 int page,
                                                                 int page_size) {
    remote_song_page_fetch_result result;
    result.server_url = server_url;
    result.page = page;
    result.page_size = page_size;

    const http_response response = send_request("GET", build_song_page_url(server_url, mode, page, page_size));
    if (!response.error_message.empty()) {
        result.error_message = response.error_message;
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        result.error_message =
            "raythm-Server returned HTTP " + std::to_string(response.status_code) + " for /songs.";
        return result;
    }

    const std::optional<std::string> items_array = json::extract_array(response.body, "items");
    if (!items_array.has_value()) {
        result.error_message = "raythm-Server returned an unexpected song catalog response.";
        return result;
    }

    const std::vector<std::string> objects = json::extract_objects_from_array(*items_array);
    for (const std::string& object : objects) {
        const auto song = parse_remote_song(object);
        if (song.has_value()) {
            result.songs.push_back(*song);
        }
    }

    result.total = json::extract_int(response.body, "total").value_or(static_cast<int>(result.songs.size()));
    result.success = true;
    return result;
}

remote_chart_fetch_result fetch_remote_charts(const std::string& server_url) {
    remote_chart_fetch_result result;
    int page = 1;
    int total = -1;

    while (true) {
        const http_response response = send_request("GET", build_paged_url(server_url, "/charts", page));
        if (!response.error_message.empty()) {
            result.error_message = response.error_message;
            return result;
        }
        if (response.status_code < 200 || response.status_code >= 300) {
            result.error_message =
                "raythm-Server returned HTTP " + std::to_string(response.status_code) + " for /charts.";
            return result;
        }

        const std::optional<std::string> items_array = json::extract_array(response.body, "items");
        if (!items_array.has_value()) {
            result.error_message = "raythm-Server returned an unexpected chart catalog response.";
            return result;
        }

        const std::vector<std::string> objects = json::extract_objects_from_array(*items_array);
        for (const std::string& object : objects) {
            const auto chart = parse_remote_chart(object);
            if (chart.has_value()) {
                result.charts.push_back(*chart);
            }
        }

        total = json::extract_int(response.body, "total").value_or(static_cast<int>(result.charts.size()));
        if (result.charts.empty() || static_cast<int>(result.charts.size()) >= total ||
            static_cast<int>(objects.size()) < kRemotePageSize) {
            break;
        }
        ++page;
    }

    result.success = true;
    return result;
}

remote_chart_page_fetch_result fetch_remote_chart_page_from_server(const std::string& server_url,
                                                                   const std::string& song_id,
                                                                   int page,
                                                                   int page_size) {
    remote_chart_page_fetch_result result;
    result.server_url = server_url;
    result.song_id = song_id;
    result.page = page;
    result.page_size = page_size;

    const http_response response = send_request("GET", build_chart_page_url(server_url, song_id, page, page_size));
    if (!response.error_message.empty()) {
        result.error_message = response.error_message;
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        result.error_message =
            "raythm-Server returned HTTP " + std::to_string(response.status_code) + " for /charts.";
        return result;
    }

    const std::optional<std::string> items_array = json::extract_array(response.body, "items");
    if (!items_array.has_value()) {
        result.error_message = "raythm-Server returned an unexpected chart catalog response.";
        return result;
    }

    const std::vector<std::string> objects = json::extract_objects_from_array(*items_array);
    for (const std::string& object : objects) {
        const auto chart = parse_remote_chart(object);
        if (chart.has_value()) {
            result.charts.push_back(*chart);
        }
    }

    result.total = json::extract_int(response.body, "total").value_or(static_cast<int>(result.charts.size()));
    result.success = true;
    return result;
}

remote_song_lookup_result fetch_remote_song_by_id_from_server(const std::string& server_url,
                                                              const std::string& song_id) {
    remote_song_lookup_result result;
    result.server_url = server_url;
    const http_response response = send_request("GET", server_url + "/songs/" + song_id);
    if (!response.error_message.empty()) {
        result.error_message = response.error_message;
        return result;
    }
    if (response.status_code == 404) {
        result.not_found = true;
        result.error_message = "Song not found.";
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        result.error_message =
            "raythm-Server returned HTTP " + std::to_string(response.status_code) + " for /songs/:songId.";
        return result;
    }

    const auto song_object = json::extract_object(response.body, "song");
    if (!song_object.has_value()) {
        result.error_message = "raythm-Server returned an unexpected song detail response.";
        return result;
    }

    const auto song = parse_remote_song(*song_object);
    if (!song.has_value()) {
        result.error_message = "raythm-Server returned an unexpected song detail response.";
        return result;
    }

    result.song = *song;
    result.success = true;
    return result;
}

}  // namespace

std::string make_absolute_remote_url(const std::string& server_url, const std::string& value) {
    if (value.empty()) {
        return {};
    }
    if (is_absolute_remote_url(value)) {
        return value;
    }
    if (!value.empty() && value.front() == '/') {
        return server_url + value;
    }
    return server_url + "/" + value;
}

remote_catalog_fetch_result fetch_remote_catalog() {
    remote_catalog_fetch_result result;
    const std::vector<std::string> server_urls = resolve_server_urls();

    for (const std::string& server_url : server_urls) {
        const remote_song_fetch_result songs = fetch_remote_songs(server_url);
        if (!songs.success) {
            if (result.error_message.empty()) {
                result.error_message = songs.error_message;
                result.server_url = server_url;
            }
            continue;
        }

        const remote_chart_fetch_result charts = fetch_remote_charts(server_url);
        if (!charts.success) {
            if (result.error_message.empty()) {
                result.error_message = charts.error_message;
                result.server_url = server_url;
            }
            continue;
        }

        result.songs = songs.songs;
        result.charts = charts.charts;
        result.server_url = server_url;
        result.success = true;
        result.error_message.clear();
        return result;
    }

    if (result.error_message.empty()) {
        result.error_message = "Could not connect to raythm-Server.";
    }
    return result;
}

remote_song_page_fetch_result fetch_remote_song_page(catalog_mode mode,
                                                     int page,
                                                     int page_size,
                                                     const std::string& preferred_server_url) {
    remote_song_page_fetch_result result;
    result.page = page;
    result.page_size = page_size;
    for (const std::string& server_url : prioritize_server_url(preferred_server_url)) {
        result = fetch_remote_song_page_from_server(server_url, mode, page, page_size);
        if (result.success) {
            return result;
        }
    }
    if (result.error_message.empty()) {
        result.error_message = "Could not connect to raythm-Server.";
    }
    return result;
}

remote_chart_page_fetch_result fetch_remote_chart_page(const std::string& server_url,
                                                       const std::string& song_id,
                                                       int page,
                                                       int page_size) {
    return fetch_remote_chart_page_from_server(server_url, song_id, page, page_size);
}

remote_song_lookup_result fetch_remote_song_by_id(const std::string& song_id,
                                                  const std::string& preferred_server_url) {
    remote_song_lookup_result result;
    for (const std::string& server_url : prioritize_server_url(preferred_server_url)) {
        result = fetch_remote_song_by_id_from_server(server_url, song_id);
        if (result.success) {
            return result;
        }
        if (result.not_found) {
            return result;
        }
    }
    if (result.error_message.empty()) {
        result.error_message = "Could not connect to raythm-Server.";
    }
    return result;
}

remote_binary_fetch_result fetch_remote_binary(
    const std::string& url,
    const remote_binary_progress_callback& progress_callback) {
    remote_binary_fetch_result result;
    const http_response response = send_request("GET", url, progress_callback);
    if (!response.error_message.empty()) {
        result.error_message = response.error_message;
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        result.error_message = "raythm-Server returned HTTP " + std::to_string(response.status_code) + ".";
        return result;
    }

    result.bytes.assign(response.body.begin(), response.body.end());
    result.content_type = response.content_type;
    result.success = true;
    return result;
}

}  // namespace title_online_view
