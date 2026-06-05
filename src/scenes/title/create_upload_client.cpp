#include "title/create_upload_client.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "network/auth_client.h"
#include "network/json_helpers.h"
#include "network/network_error.h"
#include "content_lifecycle.h"
#include "path_utils.h"
#include "chart_fingerprint.h"
#include "managed_content_storage.h"
#include "services/content_authorization_service.h"
#include "song_fingerprint.h"
#include "title/local_content_database.h"
#include "title/local_content_index.h"
#include "updater/update_verify.h"

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

namespace title_create_upload {
namespace {
namespace fs = std::filesystem;
namespace json = network::json;

struct http_response {
    int status_code = 0;
    std::string body;
    std::string error_message;
    std::string retry_after;
};

struct multipart_field {
    std::string name;
    std::string value;
};

struct multipart_file {
    std::string name;
    std::string filename;
    std::string content_type;
    std::vector<unsigned char> bytes;
};

struct upload_request_result {
    bool success = false;
    bool not_found = false;
    bool unauthorized = false;
    bool maintenance = false;
    bool updated_existing = false;
    std::string message;
    std::string retry_after;
    std::string remote_song_id;
    std::string remote_chart_id;
    int remote_chart_version = 0;
    std::optional<bool> can_edit;
    std::string lifecycle_status;
    std::string review_status;
    std::string revision_id;
    std::string content_hash;
};

struct permission_check_result {
    bool success = false;
    bool not_found = false;
    bool unauthorized = false;
    bool server_denied = false;
    bool can_edit = false;
    std::string lifecycle_status;
    std::string review_status;
};

#ifdef _WIN32
struct http_url_parts {
    std::wstring host;
    std::wstring path_and_query;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
    bool secure = false;
};

constexpr int kResolveTimeoutMs = 5000;
constexpr int kConnectTimeoutMs = 5000;
constexpr int kSendTimeoutMs = 15000;
constexpr int kReceiveTimeoutMs = 15000;
#endif

std::string trim_ascii(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

std::string escape_multipart_header_value(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (const char ch : value) {
        if (ch == '\\' || ch == '"') {
            result.push_back('\\');
        }
        result.push_back(ch);
    }
    return result;
}

std::string parse_error_message(const http_response& response, std::string fallback) {
    const network::error_classification error = network::classify_http_error(
        response.status_code,
        response.body,
        std::move(fallback),
        response.retry_after);
    if (!error.message.empty()) {
        return error.message;
    }
    if (!response.error_message.empty()) {
        return response.error_message;
    }
    return "Upload failed.";
}

void apply_error_classification(upload_request_result& result,
                                const network::error_classification& error) {
    result.message = error.message;
    result.maintenance = error.is_maintenance();
    result.retry_after = error.retry_after;
}

bool read_binary_file(const fs::path& path,
                      std::vector<unsigned char>& bytes,
                      std::string& error_message) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        error_message = "Failed to open a local file for upload.";
        return false;
    }

    bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (!input.good() && !input.eof()) {
        error_message = "Failed to read a local file for upload.";
        return false;
    }

    return true;
}

bool read_upload_file(const fs::path& path,
                      std::vector<unsigned char>& bytes,
                      std::string& error_message);
std::string bytes_sha256_hex(const std::vector<unsigned char>& bytes);
std::string bytes_text(const std::vector<unsigned char>& bytes);

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string detect_audio_content_type(const fs::path& path) {
    const std::string extension = lower_ascii(path.extension().string());
    if (extension == ".ogg") {
        return "audio/ogg";
    }
    return "audio/mpeg";
}

std::string detect_image_content_type(const fs::path& path) {
    const std::string extension = lower_ascii(path.extension().string());
    if (extension == ".png") {
        return "image/png";
    }
    if (extension == ".webp") {
        return "image/webp";
    }
    return "image/jpeg";
}

std::string format_float_field(float value) {
    std::ostringstream stream;
    stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
    return stream.str();
}

std::string timing_events_json_field(const std::vector<timing_event>& events) {
    std::ostringstream stream;
    stream << '[';
    for (size_t index = 0; index < events.size(); ++index) {
        const timing_event& event = events[index];
        if (index > 0) {
            stream << ',';
        }
        stream << "{\"type\":\"";
        if (event.type == timing_event_type::bpm) {
            stream << "bpm\",\"tick\":" << event.tick << ",\"bpm\":" << format_float_field(event.bpm);
        } else {
            stream << "meter\",\"tick\":" << event.tick << ",\"numerator\":" << event.numerator
                   << ",\"denominator\":" << event.denominator;
        }
        stream << '}';
    }
    stream << ']';
    return stream.str();
}

void push_string_field(std::vector<multipart_field>& fields,
                       const std::string& name,
                       const std::string& value) {
    if (!value.empty()) {
        fields.push_back({name, value});
    }
}

void push_int_field(std::vector<multipart_field>& fields,
                    const std::string& name,
                    int value) {
    fields.push_back({name, std::to_string(value)});
}

void push_positive_int_field(std::vector<multipart_field>& fields,
                             const std::string& name,
                             int value) {
    if (value > 0) {
        push_int_field(fields, name, value);
    }
}

void push_positive_float_field(std::vector<multipart_field>& fields,
                               const std::string& name,
                               float value) {
    if (value > 0.0f) {
        fields.push_back({name, format_float_field(value)});
    }
}

std::string make_multipart_boundary() {
    const auto nonce = static_cast<unsigned long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return "----raythmBoundary" + std::to_string(nonce);
}

std::string build_multipart_body(const std::vector<multipart_field>& fields,
                                 const std::vector<multipart_file>& files,
                                 const std::string& boundary) {
    std::string body;
    const auto append = [&](std::string_view value) {
        body.append(value.data(), value.size());
    };

    for (const multipart_field& field : fields) {
        append("--");
        append(boundary);
        append("\r\nContent-Disposition: form-data; name=\"");
        append(escape_multipart_header_value(field.name));
        append("\"\r\n\r\n");
        append(field.value);
        append("\r\n");
    }

    for (const multipart_file& file : files) {
        append("--");
        append(boundary);
        append("\r\nContent-Disposition: form-data; name=\"");
        append(escape_multipart_header_value(file.name));
        append("\"; filename=\"");
        append(escape_multipart_header_value(file.filename));
        append("\"\r\nContent-Type: ");
        append(file.content_type);
        append("\r\n\r\n");
        if (!file.bytes.empty()) {
            body.append(reinterpret_cast<const char*>(file.bytes.data()), file.bytes.size());
        }
        append("\r\n");
    }

    append("--");
    append(boundary);
    append("--\r\n");
    return body;
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

std::string narrow_utf8(std::wstring input) {
    while (!input.empty() && input.back() == L'\0') {
        input.pop_back();
    }
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

std::string describe_winhttp_error(DWORD error_code) {
    switch (error_code) {
        case ERROR_WINHTTP_TIMEOUT:
            return "The connection to raythm-server timed out.";
        case ERROR_WINHTTP_CANNOT_CONNECT:
            return "Could not connect to raythm-server.";
        case ERROR_WINHTTP_CONNECTION_ERROR:
            return "The connection to raythm-server was interrupted.";
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
            return "The server name could not be resolved.";
        case ERROR_WINHTTP_INVALID_URL:
            return "The server URL is invalid.";
        case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:
            return "The server URL scheme is not supported.";
        case ERROR_WINHTTP_SECURE_FAILURE:
            return "A secure connection to raythm-server could not be established.";
        default:
            return "Failed to communicate with raythm-server.";
    }
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

std::string query_retry_after(HINTERNET request) {
    DWORD size_bytes = 0;
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_CUSTOM,
                            L"Retry-After",
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
                            WINHTTP_QUERY_CUSTOM,
                            L"Retry-After",
                            buffer.data(),
                            &size_bytes,
                            WINHTTP_NO_HEADER_INDEX) == FALSE) {
        return {};
    }

    return narrow_utf8(std::move(buffer));
}

http_response send_request(const std::string& method,
                           const std::string& url,
                           const std::string& body,
                           const std::vector<std::pair<std::string, std::string>>& headers) {
    http_response response;
    const auto parts = parse_url(url);
    if (!parts.has_value()) {
        response.error_message = "Invalid server URL.";
        return response;
    }

    HINTERNET session = WinHttpOpen(L"raythm/0.1",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (session == nullptr) {
        response.error_message = describe_winhttp_error(GetLastError());
        return response;
    }

    WinHttpSetTimeouts(session, kResolveTimeoutMs, kConnectTimeoutMs, kSendTimeoutMs, kReceiveTimeoutMs);

    HINTERNET connection = WinHttpConnect(session, parts->host.c_str(), parts->port, 0);
    if (connection == nullptr) {
        response.error_message = describe_winhttp_error(GetLastError());
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
        response.error_message = describe_winhttp_error(GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }

    std::wstring header_block;
    for (const auto& [name, value] : headers) {
        header_block += widen_utf8(name);
        header_block += L": ";
        header_block += widen_utf8(value);
        header_block += L"\r\n";
    }

    LPVOID request_body = body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data());
    const DWORD request_body_size = static_cast<DWORD>(body.size());
    const BOOL sent = WinHttpSendRequest(
        request,
        header_block.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : header_block.c_str(),
        header_block.empty() ? 0 : static_cast<DWORD>(-1L),
        request_body,
        request_body_size,
        request_body_size,
        0);
    if (sent == FALSE || WinHttpReceiveResponse(request, nullptr) == FALSE) {
        response.error_message = describe_winhttp_error(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }

    DWORD status_code = 0;
    DWORD status_code_size = sizeof(status_code);
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status_code,
                            &status_code_size,
                            WINHTTP_NO_HEADER_INDEX) == FALSE) {
        response.error_message = "Failed to read HTTP status.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }
    response.status_code = static_cast<int>(status_code);
    response.retry_after = query_retry_after(request);

    DWORD available_size = 0;
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
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return response;
}
#else
http_response send_request(const std::string&,
                           const std::string&,
                           const std::string&,
                           const std::vector<std::pair<std::string, std::string>>&) {
    return {
        .status_code = 0,
        .body = {},
        .error_message = "Community upload is only supported on Windows builds right now.",
        .retry_after = {},
    };
}
#endif

upload_request_result parse_song_upload_response(const http_response& response,
                                                 bool updated_existing) {
    upload_request_result result;
    result.updated_existing = updated_existing;

    if (!response.error_message.empty()) {
        result.message = response.error_message;
        return result;
    }

    const network::error_classification classified = network::classify_http_error(
        response.status_code,
        response.body,
        "Song upload failed.",
        response.retry_after);
    if (classified.is_maintenance()) {
        apply_error_classification(result, classified);
        return result;
    }

    if (response.status_code == 404) {
        result.not_found = true;
        result.message = parse_error_message(response, "Song not found.");
        return result;
    }
    if (response.status_code == 401) {
        result.unauthorized = true;
        result.message = parse_error_message(response, "Log in before uploading community content.");
        return result;
    }
    if (response.status_code == 409) {
        result.message = parse_error_message(response, "Song ID already exists on the server.");
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        result.message = parse_error_message(response, "Song upload failed.");
        return result;
    }

    const std::optional<std::string> song_object = json::extract_object(response.body, "song");
    if (!song_object.has_value()) {
        result.message = "Song upload succeeded, but the response was invalid.";
        return result;
    }

    const std::optional<std::string> song_id = json::extract_string(*song_object, "id");
    if (!song_id.has_value() || song_id->empty()) {
        result.message = "Song upload succeeded, but the server did not return a song ID.";
        return result;
    }

    result.success = true;
    result.remote_song_id = *song_id;
    result.can_edit = json::extract_bool(*song_object, "canEdit");
    result.lifecycle_status = json::extract_string(*song_object, "lifecycleStatus").value_or("");
    result.review_status = json::extract_string(response.body, "reviewStatus").value_or(
        json::extract_string(*song_object, "reviewStatus").value_or(""));
    result.revision_id = json::extract_string(*song_object, "revisionId").value_or(
        json::extract_string(*song_object, "revision").value_or(""));
    result.content_hash = json::extract_string(*song_object, "songJsonFingerprint").value_or(
        json::extract_string(*song_object, "songJsonHash").value_or(
            json::extract_string(*song_object, "contentHash").value_or("")));
    if (content_lifecycle::is_pending_review(result.review_status, result.lifecycle_status)) {
        result.message = updated_existing ? "Song update submitted for review." : "Song upload submitted for review.";
    } else {
        result.message = updated_existing ? "Song upload updated." : "Song uploaded.";
    }
    return result;
}

permission_check_result parse_permission_response(const http_response& response,
                                                  const std::string& object_key,
                                                  const std::string& user_id) {
    permission_check_result result;
    if (!response.error_message.empty()) {
        return result;
    }
    if (response.status_code == 401) {
        result.unauthorized = true;
        return result;
    }
    if (response.status_code == 404) {
        result.not_found = true;
        return result;
    }
    if (response.status_code == 403) {
        result.server_denied = true;
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        return result;
    }

    const std::optional<std::string> object = json::extract_object(response.body, object_key);
    if (!object.has_value()) {
        return result;
    }

    bool uploaded_by_user = false;
    if (const std::optional<std::string> uploader_object = json::extract_object(*object, "uploader");
        uploader_object.has_value()) {
        uploaded_by_user = json::extract_string(*uploader_object, "id").value_or("") == user_id;
    }

    const std::optional<bool> can_edit = json::extract_bool(*object, "canEdit");
    if (!can_edit.has_value() && !uploaded_by_user) {
        return result;
    }

    result.success = true;
    result.can_edit = can_edit.value_or(uploaded_by_user);
    result.lifecycle_status = json::extract_string(*object, "lifecycleStatus").value_or("");
    result.review_status = json::extract_string(*object, "reviewStatus").value_or("");
    return result;
}

permission_check_result fetch_song_permission(const auth::session& session,
                                              const std::string& remote_song_id) {
    const http_response response = send_request("GET",
                                                session.server_url + "/songs/" + remote_song_id,
                                                "",
                                                {
                                                    {"Authorization", "Bearer " + session.access_token},
                                                    {"Accept", "application/json"},
                                                });
    return parse_permission_response(response, "song", session.user.id);
}

permission_check_result fetch_chart_permission(const auth::session& session,
                                               const std::string& remote_chart_id) {
    const http_response response = send_request("GET",
                                                session.server_url + "/charts/" + remote_chart_id,
                                                "",
                                                {
                                                    {"Authorization", "Bearer " + session.access_token},
                                                    {"Accept", "application/json"},
                                                });
    return parse_permission_response(response, "chart", session.user.id);
}

bool identity_matches_session(const std::string& identity_server_url, const auth::session& session) {
    return !identity_server_url.empty() &&
           auth::normalize_server_url(identity_server_url) == session.server_url;
}

std::optional<online_content::song_identity> matching_song_identity(const auth::session& session,
                                                                    const song_select::song_entry& song) {
    if (song.online_identity.has_value() &&
        identity_matches_session(song.online_identity->server_url, session) &&
        !song.online_identity->remote_song_id.empty()) {
        return song.online_identity;
    }
    return std::nullopt;
}

std::optional<online_content::chart_identity> matching_chart_identity(const auth::session& session,
                                                                      const song_select::chart_option& chart) {
    if (chart.online_identity.has_value() &&
        identity_matches_session(chart.online_identity->server_url, session) &&
        !chart.online_identity->remote_chart_id.empty()) {
        return chart.online_identity;
    }
    for (const online_content::chart_identity& link : chart.remote_links) {
        if (identity_matches_session(link.server_url, session) && !link.remote_chart_id.empty()) {
            return link;
        }
    }
    return std::nullopt;
}

std::string upload_content_source(const song_select::song_entry& song) {
    if (song.online_identity.has_value()) {
        return online_content::source_label(song.online_identity->content_source);
    }
    return song.source == content_source::official ? "official" : "community";
}

std::string upload_content_source(const song_select::song_entry& song,
                                  const song_select::chart_option& chart) {
    if (chart.online_identity.has_value()) {
        return online_content::source_label(chart.online_identity->content_source);
    }
    if (song.online_identity.has_value()) {
        return online_content::source_label(song.online_identity->content_source);
    }
    if (chart.source == content_source::official ||
        song.source == content_source::official) {
        return "official";
    }
    return "community";
}

upload_request_result parse_chart_upload_response(const http_response& response,
                                                  bool updated_existing) {
    upload_request_result result;
    result.updated_existing = updated_existing;

    if (!response.error_message.empty()) {
        result.message = response.error_message;
        return result;
    }

    const network::error_classification classified = network::classify_http_error(
        response.status_code,
        response.body,
        "Chart upload failed.",
        response.retry_after);
    if (classified.is_maintenance()) {
        apply_error_classification(result, classified);
        return result;
    }

    if (response.status_code == 404) {
        result.not_found = true;
        result.message = parse_error_message(response, "Chart target was not found.");
        return result;
    }
    if (response.status_code == 401) {
        result.unauthorized = true;
        result.message = parse_error_message(response, "Log in before uploading community content.");
        return result;
    }
    if (response.status_code == 409) {
        result.message = parse_error_message(response, "Chart ID already exists on the server.");
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        result.message = parse_error_message(response, "Chart upload failed.");
        return result;
    }

    const std::optional<std::string> chart_object = json::extract_object(response.body, "chart");
    if (!chart_object.has_value()) {
        result.message = "Chart upload succeeded, but the response was invalid.";
        return result;
    }

    const std::optional<std::string> chart_id = json::extract_string(*chart_object, "id");
    if (!chart_id.has_value() || chart_id->empty()) {
        result.message = "Chart upload succeeded, but the server did not return a chart ID.";
        return result;
    }

    result.success = true;
    result.remote_chart_id = *chart_id;
    result.remote_chart_version = json::extract_int(*chart_object, "chartVersion").value_or(0);
    result.can_edit = json::extract_bool(*chart_object, "canEdit");
    result.lifecycle_status = json::extract_string(*chart_object, "lifecycleStatus").value_or("");
    result.review_status = json::extract_string(response.body, "reviewStatus").value_or(
        json::extract_string(*chart_object, "reviewStatus").value_or(""));
    result.revision_id = json::extract_string(*chart_object, "revisionId").value_or(
        json::extract_string(*chart_object, "revision").value_or(""));
    result.content_hash = json::extract_string(*chart_object, "chartFingerprint").value_or(
        json::extract_string(*chart_object, "chartSha256").value_or(
            json::extract_string(*chart_object, "contentHash").value_or("")));
    if (content_lifecycle::is_pending_review(result.review_status, result.lifecycle_status)) {
        result.message = updated_existing ? "Chart update submitted for review." : "Chart upload submitted for review.";
    } else {
        result.message = updated_existing ? "Chart upload updated." : "Chart uploaded.";
    }
    return result;
}

void cache_song_permission(const auth::session& session,
                           const std::string& remote_song_id,
                           const std::optional<bool>& can_edit) {
    local_content_database::put_account_permission({
        .server_url = session.server_url,
        .type = local_content_database::remote_content_type::song,
        .remote_id = remote_song_id,
        .user_id = session.user.id,
        .can_edit = can_edit,
    });
}

void cache_chart_permission(const auth::session& session,
                            const std::string& remote_chart_id,
                            const std::optional<bool>& can_edit) {
    local_content_database::put_account_permission({
        .server_url = session.server_url,
        .type = local_content_database::remote_content_type::chart,
        .remote_id = remote_chart_id,
        .user_id = session.user.id,
        .can_edit = can_edit,
    });
}

content_authorization_service::fresh_permission_result classify_permission(
    const permission_check_result& permission) {
    return content_authorization_service::classify_fresh_permission(
        permission.success,
        permission.can_edit,
        permission.server_denied);
}

std::optional<auth::session> require_saved_session(std::string& error_message) {
    const std::optional<auth::session> session = auth::load_saved_session();
    if (!session.has_value() || session->access_token.empty()) {
        error_message = "Log in before uploading community content.";
        return std::nullopt;
    }

    return auth::session{
        .server_url = auth::normalize_server_url(session->server_url),
        .access_token = session->access_token,
        .refresh_token = session->refresh_token,
        .user = session->user,
    };
}

std::optional<auth::session> restore_upload_session(std::string& error_message) {
    const auth::operation_result restored = auth::restore_saved_session();
    if (!restored.success || !restored.session_data.has_value()) {
        error_message = restored.message.empty()
            ? "Log in before uploading community content."
            : restored.message;
        return std::nullopt;
    }

    return auth::session{
        .server_url = auth::normalize_server_url(restored.session_data->server_url),
        .access_token = restored.session_data->access_token,
        .refresh_token = restored.session_data->refresh_token,
        .user = restored.session_data->user,
    };
}

void append_song_contract_fields(std::vector<multipart_field>& fields,
                                 const song_select::song_entry& song,
                                 const std::vector<unsigned char>& song_json_bytes,
                                 const std::vector<unsigned char>& audio_bytes,
                                 const std::vector<unsigned char>& jacket_bytes);

void append_chart_contract_fields(std::vector<multipart_field>& fields,
                                  const song_select::song_entry& song,
                                  const song_select::chart_option& chart,
                                  const std::vector<unsigned char>& chart_bytes);

upload_request_result send_song_upload_request(const auth::session& session,
                                               const song_select::song_entry& song,
                                               const std::optional<std::string>& remote_song_id) {
    upload_request_result result;

    const song_meta& meta = song.song.meta;
    if (meta.title.empty() || meta.artist.empty() || meta.base_bpm <= 0.0f) {
        result.message = "Selected song is missing required metadata for upload.";
        return result;
    }
    if (meta.audio_file.empty() || meta.jacket_file.empty()) {
        result.message = "Selected song needs both audio and jacket files to upload.";
        return result;
    }

    const fs::path song_dir = path_utils::from_utf8(song.song.directory);
    const fs::path song_json_path = song_dir / "song.json";
    const fs::path audio_path = song_dir / path_utils::from_utf8(meta.audio_file);
    const fs::path jacket_path = song_dir / path_utils::from_utf8(meta.jacket_file);

    std::vector<unsigned char> song_json_bytes;
    std::vector<unsigned char> audio_bytes;
    std::vector<unsigned char> jacket_bytes;
    if (!read_upload_file(song_json_path, song_json_bytes, result.message)) {
        result.message = "Selected song metadata file was not found.";
        return result;
    }
    if (!read_upload_file(audio_path, audio_bytes, result.message)) {
        result.message = "Selected song audio file was not found.";
        return result;
    }
    if (!read_upload_file(jacket_path, jacket_bytes, result.message)) {
        result.message = "Selected song jacket file was not found.";
        return result;
    }

    std::vector<multipart_field> fields;
    fields.push_back({"title", meta.title});
    fields.push_back({"artist", meta.artist});
    std::vector<std::string> genres = meta.genres;
    if (genres.empty() && !meta.genre.empty()) {
        genres.push_back(meta.genre);
    }
    for (const std::string& genre : genres) {
        push_string_field(fields, "genres", genre);
    }
    for (const std::string& keyword : meta.keywords) {
        push_string_field(fields, "keywords", keyword);
    }
    fields.push_back({"baseBpm", format_float_field(meta.base_bpm)});
    if (meta.has_offset) {
        fields.push_back({"offset", std::to_string(meta.offset)});
    }
    if (!meta.timing_events.empty()) {
        fields.push_back({"timingEvents", timing_events_json_field(meta.timing_events)});
    }
    if (meta.duration_seconds > 0.0f) {
        fields.push_back({"durationSec", std::to_string(static_cast<int>(meta.duration_seconds + 0.5f))});
    }
    fields.push_back({"previewStartMs", std::to_string(std::max(0, meta.preview_start_ms))});
    fields.push_back({"visibility", "public"});
    append_song_contract_fields(fields, song, song_json_bytes, audio_bytes, jacket_bytes);

    std::vector<multipart_file> files;
    files.push_back({
        .name = "audio",
        .filename = audio_path.filename().string(),
        .content_type = detect_audio_content_type(audio_path),
        .bytes = std::move(audio_bytes),
    });
    files.push_back({
        .name = "jacket",
        .filename = jacket_path.filename().string(),
        .content_type = detect_image_content_type(jacket_path),
        .bytes = std::move(jacket_bytes),
    });

    const std::string boundary = make_multipart_boundary();
    const std::string body = build_multipart_body(fields, files, boundary);
    const std::string method = remote_song_id.has_value() ? "PUT" : "POST";
    const std::string url = remote_song_id.has_value()
        ? session.server_url + "/songs/" + *remote_song_id
        : session.server_url + "/songs";
    const http_response response = send_request(method, url, body, {
        {"Authorization", "Bearer " + session.access_token},
        {"Accept", "application/json"},
        {"Content-Type", "multipart/form-data; boundary=" + boundary},
    });
    return parse_song_upload_response(response, remote_song_id.has_value());
}

std::optional<std::string> resolve_remote_song_id_for_chart_upload(const auth::session& session,
                                                                   const song_select::song_entry& song,
                                                                   const song_select::chart_option& chart) {
    const std::optional<local_content_index::online_song_binding> binding =
        local_content_index::find_song_by_local(session.server_url, song.song.meta.song_id);
    if (binding.has_value() && !binding->remote_song_id.empty()) {
        return binding->remote_song_id;
    }
    if (const std::optional<local_content_index::online_chart_binding> chart_binding =
            local_content_index::find_chart_by_local(session.server_url, chart.meta.chart_id);
        chart_binding.has_value() && !chart_binding->remote_song_id.empty()) {
        return chart_binding->remote_song_id;
    }
    if (const std::optional<online_content::song_identity> identity = matching_song_identity(session, song);
        identity.has_value() && !identity->remote_song_id.empty()) {
        return identity->remote_song_id;
    }
    if (const std::optional<online_content::chart_identity> chart_identity = matching_chart_identity(session, chart);
        chart_identity.has_value() && !chart_identity->remote_song_id.empty()) {
        return chart_identity->remote_song_id;
    }
    return std::nullopt;
}

void append_song_contract_fields(std::vector<multipart_field>& fields,
                                 const song_select::song_entry& song,
                                 const std::vector<unsigned char>& song_json_bytes,
                                 const std::vector<unsigned char>& audio_bytes,
                                 const std::vector<unsigned char>& jacket_bytes) {
    const song_meta& meta = song.song.meta;
    fields.push_back({"metadataSchemaVersion", "2"});
    fields.push_back({"contentSource", upload_content_source(song)});
    push_string_field(fields, "clientSongId", meta.song_id);
    push_positive_int_field(fields, "songVersion", meta.song_version);

    const std::string song_json_text = bytes_text(song_json_bytes);
    fields.push_back({"songJsonSha256", bytes_sha256_hex(song_json_bytes)});
    fields.push_back({"songJsonFingerprint",
                      updater::compute_sha256_hex(std::string_view(song_fingerprint::build(song_json_text)))});
    fields.push_back({"audioSha256", bytes_sha256_hex(audio_bytes)});
    fields.push_back({"jacketSha256", bytes_sha256_hex(jacket_bytes)});
}

void append_chart_contract_fields(std::vector<multipart_field>& fields,
                                  const song_select::song_entry& song,
                                  const song_select::chart_option& chart,
                                  const std::vector<unsigned char>& chart_bytes) {
    fields.push_back({"metadataSchemaVersion", "2"});
    fields.push_back({"contentSource", upload_content_source(song, chart)});
    push_string_field(fields, "clientChartId", chart.meta.chart_id);
    push_string_field(fields, "clientSongId", song.song.meta.song_id);
    push_positive_int_field(fields, "keyCount", chart.meta.key_count);
    push_string_field(fields, "difficultyName", chart.meta.difficulty);
    push_string_field(fields, "chartAuthor", chart.meta.chart_author);
    push_positive_int_field(fields, "formatVersion", chart.meta.format_version);
    push_positive_int_field(fields, "resolution", chart.meta.resolution);
    push_int_field(fields, "offset", chart.meta.offset);
    push_positive_int_field(fields, "noteCount", chart.note_count);
    push_positive_float_field(fields, "minBpm", chart.min_bpm);
    push_positive_float_field(fields, "maxBpm", chart.max_bpm);
    push_positive_float_field(fields, "calculatedLevel", chart.meta.level);
    fields.push_back({"difficultyRulesetId", "raythm-local"});
    fields.push_back({"difficultyRulesetVersion", "13"});

    const std::string chart_text = bytes_text(chart_bytes);
    fields.push_back({"chartSha256", bytes_sha256_hex(chart_bytes)});
    fields.push_back({"chartFingerprint",
                      updater::compute_sha256_hex(std::string_view(chart_fingerprint::build(chart_text)))});
}

upload_request_result send_chart_upload_request(const auth::session& session,
                                                const song_select::song_entry& song,
                                                const song_select::chart_option& chart,
                                                const std::string& remote_song_id,
                                                const std::optional<std::string>& remote_chart_id) {
    upload_request_result result;

    const fs::path chart_path = path_utils::from_utf8(chart.path);

    std::vector<unsigned char> chart_bytes;
    if (!read_upload_file(chart_path, chart_bytes, result.message)) {
        result.message = "Selected chart file was not found.";
        return result;
    }

    std::vector<multipart_field> fields;
    fields.push_back({"songId", remote_song_id});
    fields.push_back({"visibility", "public"});
    append_chart_contract_fields(fields, song, chart, chart_bytes);

    std::vector<multipart_file> files;
    files.push_back({
        .name = "chart",
        .filename = chart_path.filename().string(),
        .content_type = "text/plain; charset=utf-8",
        .bytes = std::move(chart_bytes),
    });

    const std::string boundary = make_multipart_boundary();
    const std::string body = build_multipart_body(fields, files, boundary);
    const std::string method = remote_chart_id.has_value() ? "PUT" : "POST";
    const std::string url = remote_chart_id.has_value()
        ? session.server_url + "/charts/" + *remote_chart_id
        : session.server_url + "/charts";
    const http_response response = send_request(method, url, body, {
        {"Authorization", "Bearer " + session.access_token},
        {"Accept", "application/json"},
        {"Content-Type", "multipart/form-data; boundary=" + boundary},
    });
    return parse_chart_upload_response(response, remote_chart_id.has_value());
}

std::optional<online_content::source> online_source_for_upload(const song_select::song_entry& song,
                                                               const song_select::chart_option& chart) {
    return online_content::source_from_string(upload_content_source(song, chart));
}

managed_content_storage::chart_promotion_result promote_uploaded_chart_if_possible(
    const auth::session& session,
    const song_select::song_entry& song,
    const song_select::chart_option& chart,
    const upload_request_result& request_result,
    const std::string& remote_song_id,
    std::string& error_message) {
    managed_content_storage::chart_promotion_result result;
    if (!request_result.success || request_result.remote_chart_id.empty()) {
        return result;
    }
    if (chart.storage == storage_policy::managed_package) {
        return result;
    }

    const fs::path song_dir = path_utils::from_utf8(song.song.directory);
    if (!managed_content_storage::read_manifest(song_dir).has_value()) {
        return result;
    }

    const fs::path chart_path = path_utils::from_utf8(chart.path);
    const std::optional<std::string> chart_hash = updater::compute_sha256_hex(chart_path);
    const std::optional<std::string> chart_fingerprint_hash = chart_fingerprint::compute_sha256_hex(chart_path);
    const online_content::source source =
        online_source_for_upload(song, chart).value_or(online_content::source::community);
    managed_content_storage::chart_identity identity{
        .source = source,
        .server_url = session.server_url,
        .remote_song_id = remote_song_id,
        .remote_chart_id = request_result.remote_chart_id,
        .song_version = song.song.meta.song_version,
        .chart_version = request_result.remote_chart_version,
        .revision_id = request_result.revision_id,
        .chart_hash = chart_hash.value_or(""),
        .chart_fingerprint = chart_fingerprint_hash.value_or(""),
        .remote_chart_hash = "",
        .remote_chart_fingerprint = !request_result.content_hash.empty()
            ? request_result.content_hash
            : chart_fingerprint_hash.value_or(""),
    };
    return managed_content_storage::promote_plain_chart_to_managed(
        song_dir, identity, chart_path, true, error_message);
}

bool sync_managed_song_manifest_after_upload(const song_select::song_entry& song,
                                             const upload_request_result& request_result) {
    const fs::path song_dir = path_utils::from_utf8(song.song.directory);
    std::optional<managed_content_storage::package_manifest> manifest =
        managed_content_storage::read_manifest(song_dir);
    if (!manifest.has_value()) {
        return false;
    }

    const song_meta& meta = song.song.meta;
    std::vector<unsigned char> song_json_bytes;
    std::vector<unsigned char> audio_bytes;
    std::vector<unsigned char> jacket_bytes;
    std::string error_message;
    if (!read_upload_file(song_dir / "song.json", song_json_bytes, error_message) ||
        !read_upload_file(song_dir / path_utils::from_utf8(meta.audio_file), audio_bytes, error_message) ||
        !read_upload_file(song_dir / path_utils::from_utf8(meta.jacket_file), jacket_bytes, error_message)) {
        return false;
    }

    const std::string song_json_text = bytes_text(song_json_bytes);
    manifest->song.remote_song_id = request_result.remote_song_id;
    manifest->song.song_version = song.song.meta.song_version;
    manifest->song.revision_id = request_result.revision_id;
    manifest->song_json_hash = bytes_sha256_hex(song_json_bytes);
    manifest->song_json_fingerprint =
        updater::compute_sha256_hex(std::string_view(song_fingerprint::build(song_json_text)));
    manifest->audio_hash = bytes_sha256_hex(audio_bytes);
    manifest->jacket_hash = bytes_sha256_hex(jacket_bytes);
    manifest->remote_song_json_hash = manifest->song_json_hash;
    manifest->remote_song_json_fingerprint = !request_result.content_hash.empty()
        ? request_result.content_hash
        : manifest->song_json_fingerprint;
    manifest->remote_audio_hash = manifest->audio_hash;
    manifest->remote_jacket_hash = manifest->jacket_hash;
    return managed_content_storage::write_manifest(*manifest, error_message);
}

bool sync_managed_chart_manifest_after_upload(const song_select::song_entry& song,
                                              const song_select::chart_option& chart,
                                              const upload_request_result& request_result) {
    const fs::path song_dir = path_utils::from_utf8(song.song.directory);
    std::optional<managed_content_storage::package_manifest> manifest =
        managed_content_storage::read_manifest(song_dir);
    if (!manifest.has_value()) {
        return false;
    }

    std::vector<unsigned char> chart_bytes;
    std::string error_message;
    if (!read_upload_file(path_utils::from_utf8(chart.path), chart_bytes, error_message)) {
        return false;
    }

    managed_content_storage::chart_manifest_entry* manifest_chart = nullptr;
    for (managed_content_storage::chart_manifest_entry& candidate : manifest->charts) {
        if (candidate.local_chart_id == chart.meta.chart_id ||
            candidate.remote_chart_id == request_result.remote_chart_id) {
            manifest_chart = &candidate;
            break;
        }
    }
    if (manifest_chart == nullptr) {
        return false;
    }

    const std::string chart_text = bytes_text(chart_bytes);
    manifest_chart->remote_chart_id = request_result.remote_chart_id;
    manifest_chart->chart_version = request_result.remote_chart_version;
    manifest_chart->revision_id = request_result.revision_id;
    manifest_chart->chart_hash = bytes_sha256_hex(chart_bytes);
    manifest_chart->chart_fingerprint =
        updater::compute_sha256_hex(std::string_view(chart_fingerprint::build(chart_text)));
    manifest_chart->remote_chart_hash = manifest_chart->chart_hash;
    manifest_chart->remote_chart_fingerprint = !request_result.content_hash.empty()
        ? request_result.content_hash
        : manifest_chart->chart_fingerprint;
    return managed_content_storage::write_manifest(*manifest, error_message);
}

bool read_upload_file(const fs::path& path,
                      std::vector<unsigned char>& bytes,
                      std::string& error_message) {
    const managed_content_storage::managed_file_read_result managed =
        managed_content_storage::read_managed_file(path);
    if (managed.managed) {
        if (!managed.success) {
            error_message = managed.error_message.empty()
                ? "Failed to read a managed file for upload."
                : managed.error_message;
            return false;
        }
        bytes = managed.bytes;
        return true;
    }

    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        error_message = "Selected local file was not found.";
        return false;
    }
    return read_binary_file(path, bytes, error_message);
}

std::string bytes_sha256_hex(const std::vector<unsigned char>& bytes) {
    return updater::compute_sha256_hex(std::string_view(
        reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

std::string bytes_text(const std::vector<unsigned char>& bytes) {
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

}  // namespace

upload_result upload_song(const song_select::song_entry& song) {
    upload_result result;

    std::string error_message;
    const std::optional<auth::session> session_opt = require_saved_session(error_message);
    if (!session_opt.has_value()) {
        result.message = std::move(error_message);
        return result;
    }
    auth::session session = *session_opt;

    const std::optional<local_content_index::online_song_binding> existing_song_binding =
        local_content_index::find_song_by_local(session.server_url, song.song.meta.song_id);
    const std::optional<online_content::song_identity> song_identity =
        matching_song_identity(session, song);
    std::optional<std::string> existing_remote_song_id =
        existing_song_binding.has_value() && !existing_song_binding->remote_song_id.empty()
            ? std::optional<std::string>(existing_song_binding->remote_song_id)
            : (song_identity.has_value() ? std::optional<std::string>(song_identity->remote_song_id) : std::nullopt);
    std::optional<bool> known_song_can_edit =
        song_identity.has_value() && song_identity->can_edit.has_value()
            ? song_identity->can_edit
            : std::nullopt;
    const std::optional<local_content_database::remote_metadata> existing_song_metadata =
        existing_remote_song_id.has_value()
            ? local_content_database::find_remote_metadata(local_content_database::remote_content_type::song,
                                                           session.server_url,
                                                           *existing_remote_song_id)
            : std::nullopt;
    std::string known_song_lifecycle =
        song_identity.has_value() && !song_identity->lifecycle_status.empty()
            ? song_identity->lifecycle_status
            : (existing_song_metadata.has_value() ? existing_song_metadata->lifecycle_status : "");
    std::string known_song_review =
        song_identity.has_value() && !song_identity->review_status.empty()
            ? song_identity->review_status
            : (existing_song_metadata.has_value() ? existing_song_metadata->review_status : "");
    if (existing_remote_song_id.has_value()) {
        permission_check_result permission =
            fetch_song_permission(session, *existing_remote_song_id);
        if (permission.unauthorized) {
            std::string restore_error;
            const std::optional<auth::session> restored = restore_upload_session(restore_error);
            if (!restored.has_value()) {
                result.message = std::move(restore_error);
                return result;
            }
            session = *restored;
            permission = fetch_song_permission(session, *existing_remote_song_id);
        }
        if (permission.not_found) {
            local_content_index::remove_song_binding(session.server_url, song.song.meta.song_id);
            existing_remote_song_id = std::nullopt;
            known_song_can_edit = std::nullopt;
            known_song_lifecycle.clear();
            known_song_review.clear();
        } else {
            if (permission.success) {
                known_song_can_edit = permission.can_edit;
                cache_song_permission(session, *existing_remote_song_id, permission.can_edit);
                if (!permission.lifecycle_status.empty()) {
                    known_song_lifecycle = permission.lifecycle_status;
                }
                if (!permission.review_status.empty()) {
                    known_song_review = permission.review_status;
                }
            }
            const auto permission_result = classify_permission(permission);
            if (permission_result == content_authorization_service::fresh_permission_result::denied) {
                result.message = "The server says this account cannot edit the linked song.";
                return result;
            }
            if (permission_result != content_authorization_service::fresh_permission_result::allowed) {
                result.message = "Could not verify song edit permission.";
                return result;
            }
        }
    }
    upload_request_result request_result =
        send_song_upload_request(session, song, existing_remote_song_id);
    if (request_result.unauthorized) {
        std::string restore_error;
        const std::optional<auth::session> restored = restore_upload_session(restore_error);
        if (!restored.has_value()) {
            result.message = std::move(restore_error);
            return result;
        }
        session = *restored;
        request_result = send_song_upload_request(session, song, existing_remote_song_id);
    }
    if (request_result.not_found && existing_remote_song_id.has_value()) {
        local_content_index::remove_song_binding(session.server_url, song.song.meta.song_id);
        request_result = send_song_upload_request(session, song, std::nullopt);
        if (request_result.unauthorized) {
            std::string restore_error;
            const std::optional<auth::session> restored = restore_upload_session(restore_error);
            if (!restored.has_value()) {
                result.message = std::move(restore_error);
                return result;
            }
            session = *restored;
            request_result = send_song_upload_request(session, song, std::nullopt);
        }
    }

    result.success = request_result.success;
    result.maintenance = request_result.maintenance;
    result.message = request_result.message;
    result.retry_after = request_result.retry_after;
    result.should_refresh_online_catalog = request_result.success;
    if (!request_result.success) {
        return result;
    }
    cache_song_permission(session, request_result.remote_song_id, request_result.can_edit);
    (void)sync_managed_song_manifest_after_upload(song, request_result);

    local_content_index::put_song_binding({
        .server_url = session.server_url,
        .local_song_id = song.song.meta.song_id,
        .remote_song_id = request_result.remote_song_id,
        .origin = request_result.updated_existing
            ? (existing_song_binding.has_value()
                ? existing_song_binding->origin
                : local_content_index::online_origin::linked)
            : local_content_index::online_origin::owned_upload,
    });
    local_content_database::put_remote_metadata({
        .server_url = session.server_url,
        .type = local_content_database::remote_content_type::song,
        .remote_id = request_result.remote_song_id,
        .content_source = upload_content_source(song),
        .lifecycle_status = !request_result.lifecycle_status.empty() ? request_result.lifecycle_status : known_song_lifecycle,
        .review_status = !request_result.review_status.empty() ? request_result.review_status : known_song_review,
        .remote_version = song.song.meta.song_version,
        .revision_id = request_result.revision_id,
        .content_hash = request_result.content_hash,
    });
    if (!request_result.updated_existing) {
        result.message += " Charts for this song can now be uploaded.";
    }

    return result;
}

upload_result upload_chart(const song_select::song_entry& song,
                           const song_select::chart_option& chart) {
    upload_result result;

    std::string error_message;
    const std::optional<auth::session> session_opt = require_saved_session(error_message);
    if (!session_opt.has_value()) {
        result.message = std::move(error_message);
        return result;
    }
    auth::session session = *session_opt;

    std::optional<std::string> remote_song_id = resolve_remote_song_id_for_chart_upload(session, song, chart);
    if (!remote_song_id.has_value()) {
        result.message = "Upload the song first so the server can assign a song ID.";
        return result;
    }
    const std::optional<local_content_index::online_song_binding> existing_song_binding =
        local_content_index::find_song_by_local(session.server_url, song.song.meta.song_id);
    const std::optional<online_content::song_identity> song_identity =
        matching_song_identity(session, song);
    const std::optional<online_content::chart_identity> chart_identity =
        matching_chart_identity(session, chart);
    std::optional<bool> known_song_can_edit =
        song_identity.has_value() && song_identity->can_edit.has_value()
            ? song_identity->can_edit
            : std::nullopt;
    const std::optional<local_content_database::remote_metadata> existing_song_metadata =
        remote_song_id.has_value()
            ? local_content_database::find_remote_metadata(local_content_database::remote_content_type::song,
                                                           session.server_url,
                                                           *remote_song_id)
            : std::nullopt;
    std::string known_song_lifecycle =
        song_identity.has_value() && !song_identity->lifecycle_status.empty()
            ? song_identity->lifecycle_status
            : (existing_song_metadata.has_value() ? existing_song_metadata->lifecycle_status : "");
    std::string known_song_review =
        song_identity.has_value() && !song_identity->review_status.empty()
            ? song_identity->review_status
            : (existing_song_metadata.has_value() ? existing_song_metadata->review_status : "");
    const std::optional<local_content_index::online_chart_binding> existing_chart_binding =
        local_content_index::find_chart_by_local(session.server_url, chart.meta.chart_id);
    std::optional<std::string> existing_remote_chart_id =
        existing_chart_binding.has_value() && !existing_chart_binding->remote_chart_id.empty()
            ? std::optional<std::string>(existing_chart_binding->remote_chart_id)
            : (chart_identity.has_value() ? std::optional<std::string>(chart_identity->remote_chart_id) : std::nullopt);
    std::optional<bool> known_chart_can_edit =
        chart_identity.has_value() && chart_identity->can_edit.has_value()
            ? chart_identity->can_edit
            : std::nullopt;
    const std::optional<local_content_database::remote_metadata> existing_chart_metadata =
        existing_remote_chart_id.has_value()
            ? local_content_database::find_remote_metadata(local_content_database::remote_content_type::chart,
                                                           session.server_url,
                                                           *existing_remote_chart_id)
            : std::nullopt;
    std::string known_chart_lifecycle =
        chart_identity.has_value() && !chart_identity->lifecycle_status.empty()
            ? chart_identity->lifecycle_status
            : (existing_chart_metadata.has_value() ? existing_chart_metadata->lifecycle_status : "");
    std::string known_chart_review =
        chart_identity.has_value() && !chart_identity->review_status.empty()
            ? chart_identity->review_status
            : (existing_chart_metadata.has_value() ? existing_chart_metadata->review_status : "");
    if (!existing_remote_chart_id.has_value()) {
        permission_check_result permission =
            fetch_song_permission(session, *remote_song_id);
        if (permission.unauthorized) {
            std::string restore_error;
            const std::optional<auth::session> restored = restore_upload_session(restore_error);
            if (!restored.has_value()) {
                result.message = std::move(restore_error);
                return result;
            }
            session = *restored;
            permission = fetch_song_permission(session, *remote_song_id);
        }
        if (permission.not_found) {
            local_content_index::remove_song_binding(session.server_url, song.song.meta.song_id);
            local_content_index::remove_chart_bindings_for_remote_song(session.server_url, *remote_song_id);
            result.message = "Upload the song first so the server can assign a song ID.";
            return result;
        }
        if (permission.success) {
            known_song_can_edit = permission.can_edit;
            cache_song_permission(session, *remote_song_id, permission.can_edit);
            if (!permission.lifecycle_status.empty()) {
                known_song_lifecycle = permission.lifecycle_status;
            }
            if (!permission.review_status.empty()) {
                known_song_review = permission.review_status;
            }
        }
        const auto permission_result = classify_permission(permission);
        if (permission_result == content_authorization_service::fresh_permission_result::denied) {
            result.message = "The server says this account cannot edit the linked song.";
            return result;
        }
        if (permission_result != content_authorization_service::fresh_permission_result::allowed) {
            result.message = "Could not verify song edit permission.";
            return result;
        }
    }
    if (existing_remote_chart_id.has_value()) {
        permission_check_result permission =
            fetch_chart_permission(session, *existing_remote_chart_id);
        if (permission.unauthorized) {
            std::string restore_error;
            const std::optional<auth::session> restored = restore_upload_session(restore_error);
            if (!restored.has_value()) {
                result.message = std::move(restore_error);
                return result;
            }
            session = *restored;
            permission = fetch_chart_permission(session, *existing_remote_chart_id);
        }
        if (permission.not_found) {
            local_content_index::remove_chart_binding(session.server_url, chart.meta.chart_id);
            existing_remote_chart_id = std::nullopt;
            known_chart_can_edit = std::nullopt;
            known_chart_lifecycle.clear();
            known_chart_review.clear();
        } else {
            if (permission.success) {
                known_chart_can_edit = permission.can_edit;
                cache_chart_permission(session, *existing_remote_chart_id, permission.can_edit);
                if (!permission.lifecycle_status.empty()) {
                    known_chart_lifecycle = permission.lifecycle_status;
                }
                if (!permission.review_status.empty()) {
                    known_chart_review = permission.review_status;
                }
            }
            const auto permission_result = classify_permission(permission);
            if (permission_result == content_authorization_service::fresh_permission_result::denied) {
                result.message = "The server says this account cannot edit the linked chart.";
                return result;
            }
            if (permission_result != content_authorization_service::fresh_permission_result::allowed) {
                result.message = "Could not verify chart edit permission.";
                return result;
            }
        }
    }
    const std::optional<std::string> effective_remote_chart_id = existing_remote_chart_id;

    upload_request_result request_result =
        send_chart_upload_request(session, song, chart, *remote_song_id, effective_remote_chart_id);
    if (request_result.unauthorized) {
        std::string restore_error;
        const std::optional<auth::session> restored = restore_upload_session(restore_error);
        if (!restored.has_value()) {
            result.message = std::move(restore_error);
            return result;
        }
        session = *restored;
        request_result = send_chart_upload_request(session, song, chart, *remote_song_id, effective_remote_chart_id);
    }
    if (request_result.not_found && existing_remote_chart_id.has_value()) {
        local_content_index::remove_chart_binding(session.server_url, chart.meta.chart_id);
        request_result =
            send_chart_upload_request(session, song, chart, *remote_song_id, std::nullopt);
        if (request_result.unauthorized) {
            std::string restore_error;
            const std::optional<auth::session> restored = restore_upload_session(restore_error);
            if (!restored.has_value()) {
                result.message = std::move(restore_error);
                return result;
            }
            session = *restored;
            request_result =
                send_chart_upload_request(session, song, chart, *remote_song_id, std::nullopt);
        }
    }
    if (request_result.not_found && remote_song_id.has_value()) {
        local_content_index::remove_song_binding(session.server_url, song.song.meta.song_id);
        local_content_index::remove_chart_bindings_for_remote_song(session.server_url, *remote_song_id);
        result.message = "Upload the song first so the server can assign a song ID.";
        return result;
    }

    result.success = request_result.success;
    result.maintenance = request_result.maintenance;
    result.message = request_result.message;
    result.retry_after = request_result.retry_after;
    result.should_refresh_online_catalog = request_result.success;
    if (!request_result.success) {
        return result;
    }
    cache_chart_permission(session, request_result.remote_chart_id, request_result.can_edit);
    const bool managed_chart_synced =
        sync_managed_chart_manifest_after_upload(song, chart, request_result);
    std::string local_chart_id = chart.meta.chart_id;
    std::string promotion_error;
    const managed_content_storage::chart_promotion_result promotion =
        promote_uploaded_chart_if_possible(session, song, chart, request_result, *remote_song_id, promotion_error);
    if (promotion.success && !promotion.local_chart_id.empty()) {
        local_content_index::remove_chart_binding(session.server_url, chart.meta.chart_id);
        local_chart_id = promotion.local_chart_id;
    } else if (!managed_chart_synced && !promotion_error.empty()) {
        result.message += " Local managed storage update failed: " + promotion_error;
    }

    if (!local_content_index::find_song_by_local(session.server_url, song.song.meta.song_id).has_value()) {
        local_content_index::put_song_binding({
            .server_url = session.server_url,
            .local_song_id = song.song.meta.song_id,
            .remote_song_id = *remote_song_id,
            .origin = local_content_index::online_origin::linked,
        });
        local_content_database::put_remote_metadata({
            .server_url = session.server_url,
            .type = local_content_database::remote_content_type::song,
            .remote_id = *remote_song_id,
            .content_source = upload_content_source(song),
            .lifecycle_status = known_song_lifecycle,
            .review_status = known_song_review,
            .remote_version = song.song.meta.song_version,
            .revision_id = existing_song_metadata.has_value() ? existing_song_metadata->revision_id : "",
            .content_hash = existing_song_metadata.has_value() ? existing_song_metadata->content_hash : "",
        });
    }
    local_content_index::put_chart_binding({
        .server_url = session.server_url,
        .local_chart_id = local_chart_id,
        .remote_chart_id = request_result.remote_chart_id,
        .remote_song_id = *remote_song_id,
        .remote_chart_version = request_result.remote_chart_version,
        .origin = request_result.updated_existing
            ? (existing_chart_binding.has_value()
                ? existing_chart_binding->origin
                : local_content_index::online_origin::linked)
            : local_content_index::online_origin::owned_upload,
    });
    local_content_database::put_remote_metadata({
        .server_url = session.server_url,
        .type = local_content_database::remote_content_type::chart,
        .remote_id = request_result.remote_chart_id,
        .content_source = upload_content_source(song, chart),
        .lifecycle_status = !request_result.lifecycle_status.empty() ? request_result.lifecycle_status : known_chart_lifecycle,
        .review_status = !request_result.review_status.empty() ? request_result.review_status : known_chart_review,
        .remote_version = request_result.remote_chart_version,
        .revision_id = request_result.revision_id,
        .content_hash = request_result.content_hash,
    });

    return result;
}

bool refresh_song_edit_permission(const std::string& remote_song_id) {
    if (remote_song_id.empty()) {
        return false;
    }
    std::string error_message;
    std::optional<auth::session> session = require_saved_session(error_message);
    if (!session.has_value()) {
        return false;
    }
    permission_check_result permission = fetch_song_permission(*session, remote_song_id);
    if (permission.unauthorized) {
        session = restore_upload_session(error_message);
        if (!session.has_value()) {
            return false;
        }
        permission = fetch_song_permission(*session, remote_song_id);
    }
    if (!permission.success && !permission.server_denied) {
        return false;
    }
    cache_song_permission(*session, remote_song_id, permission.server_denied
        ? std::optional<bool>(false)
        : std::optional<bool>(permission.can_edit));
    return true;
}

bool refresh_chart_edit_permission(const std::string& remote_chart_id) {
    if (remote_chart_id.empty()) {
        return false;
    }
    std::string error_message;
    std::optional<auth::session> session = require_saved_session(error_message);
    if (!session.has_value()) {
        return false;
    }
    permission_check_result permission = fetch_chart_permission(*session, remote_chart_id);
    if (permission.unauthorized) {
        session = restore_upload_session(error_message);
        if (!session.has_value()) {
            return false;
        }
        permission = fetch_chart_permission(*session, remote_chart_id);
    }
    if (!permission.success && !permission.server_denied) {
        return false;
    }
    cache_chart_permission(*session, remote_chart_id, permission.server_denied
        ? std::optional<bool>(false)
        : std::optional<bool>(permission.can_edit));
    return true;
}

}  // namespace title_create_upload
