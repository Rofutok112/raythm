#include "title/create_upload_client.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "app_paths.h"
#include "network/auth_client.h"
#include "network/json_helpers.h"
#include "path_utils.h"
#include "title/online_download_remote_client.h"

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
};

struct song_mapping_entry {
    std::string server_url;
    std::string local_song_id;
    std::string remote_song_id;
};

struct chart_mapping_entry {
    std::string server_url;
    std::string local_chart_id;
    std::string local_song_id;
    std::string remote_chart_id;
    std::string remote_song_id;
};

struct upload_mapping_store {
    std::vector<song_mapping_entry> songs;
    std::vector<chart_mapping_entry> charts;
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
    bool updated_existing = false;
    std::string message;
    std::string remote_song_id;
    std::string remote_chart_id;
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

std::string strip_trailing_cr(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
        value.pop_back();
    }
    return value;
}

std::vector<std::string> split_tab_fields(const std::string& line) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t end = line.find('\t', start);
        if (end == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    return fields;
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
    if (const std::optional<std::string> message = json::extract_string(response.body, "message");
        message.has_value() && !message->empty()) {
        return *message;
    }
    if (!response.error_message.empty()) {
        return response.error_message;
    }
    return fallback;
}

upload_mapping_store load_upload_mappings() {
    upload_mapping_store store;

    std::ifstream input(app_paths::upload_mapping_path(), std::ios::binary);
    if (!input.is_open()) {
        return store;
    }

    enum class section {
        none,
        songs,
        charts,
    };

    section current_section = section::none;
    std::string line;
    while (std::getline(input, line)) {
        line = strip_trailing_cr(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line == "[songs]") {
            current_section = section::songs;
            continue;
        }
        if (line == "[charts]") {
            current_section = section::charts;
            continue;
        }

        const std::vector<std::string> fields = split_tab_fields(line);
        if (current_section == section::songs && fields.size() >= 3) {
            store.songs.push_back(song_mapping_entry{
                .server_url = fields[0],
                .local_song_id = fields[1],
                .remote_song_id = fields[2],
            });
        } else if (current_section == section::charts && fields.size() >= 5) {
            store.charts.push_back(chart_mapping_entry{
                .server_url = fields[0],
                .local_chart_id = fields[1],
                .local_song_id = fields[2],
                .remote_chart_id = fields[3],
                .remote_song_id = fields[4],
            });
        }
    }

    return store;
}

bool save_upload_mappings(const upload_mapping_store& store) {
    app_paths::ensure_directories();

    std::ofstream output(app_paths::upload_mapping_path(), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "# raythm upload mappings v1\n";
    output << "[songs]\n";
    for (const song_mapping_entry& entry : store.songs) {
        output << entry.server_url << '\t'
               << entry.local_song_id << '\t'
               << entry.remote_song_id << '\n';
    }
    output << "[charts]\n";
    for (const chart_mapping_entry& entry : store.charts) {
        output << entry.server_url << '\t'
               << entry.local_chart_id << '\t'
               << entry.local_song_id << '\t'
               << entry.remote_chart_id << '\t'
               << entry.remote_song_id << '\n';
    }

    return output.good();
}

std::optional<std::string> find_song_mapping(const upload_mapping_store& store,
                                             const std::string& server_url,
                                             const std::string& local_song_id) {
    for (const song_mapping_entry& entry : store.songs) {
        if (entry.server_url == server_url && entry.local_song_id == local_song_id) {
            return entry.remote_song_id;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_chart_mapping(const upload_mapping_store& store,
                                              const std::string& server_url,
                                              const std::string& local_chart_id) {
    for (const chart_mapping_entry& entry : store.charts) {
        if (entry.server_url == server_url && entry.local_chart_id == local_chart_id) {
            return entry.remote_chart_id;
        }
    }
    return std::nullopt;
}

void remove_chart_mapping(upload_mapping_store& store,
                          const std::string& server_url,
                          const std::string& local_chart_id) {
    std::erase_if(store.charts, [&](const chart_mapping_entry& entry) {
        return entry.server_url == server_url && entry.local_chart_id == local_chart_id;
    });
}

void remove_song_mapping(upload_mapping_store& store,
                         const std::string& server_url,
                         const std::string& local_song_id) {
    std::erase_if(store.songs, [&](const song_mapping_entry& entry) {
        return entry.server_url == server_url && entry.local_song_id == local_song_id;
    });
    std::erase_if(store.charts, [&](const chart_mapping_entry& entry) {
        return entry.server_url == server_url && entry.local_song_id == local_song_id;
    });
}

void store_song_mapping(upload_mapping_store& store,
                        const std::string& server_url,
                        const std::string& local_song_id,
                        const std::string& remote_song_id) {
    if (local_song_id.empty() || remote_song_id.empty()) {
        return;
    }

    bool found = false;
    for (song_mapping_entry& entry : store.songs) {
        if (entry.server_url == server_url && entry.local_song_id == local_song_id) {
            found = true;
            if (entry.remote_song_id != remote_song_id) {
                entry.remote_song_id = remote_song_id;
                std::erase_if(store.charts, [&](const chart_mapping_entry& chart_entry) {
                    return chart_entry.server_url == server_url &&
                           chart_entry.local_song_id == local_song_id;
                });
            }
            break;
        }
    }

    if (!found) {
        store.songs.push_back(song_mapping_entry{
            .server_url = server_url,
            .local_song_id = local_song_id,
            .remote_song_id = remote_song_id,
        });
    }
}

void store_chart_mapping(upload_mapping_store& store,
                         const std::string& server_url,
                         const std::string& local_chart_id,
                         const std::string& local_song_id,
                         const std::string& remote_chart_id,
                         const std::string& remote_song_id) {
    if (local_chart_id.empty() || remote_chart_id.empty()) {
        return;
    }

    for (chart_mapping_entry& entry : store.charts) {
        if (entry.server_url == server_url && entry.local_chart_id == local_chart_id) {
            entry.local_song_id = local_song_id;
            entry.remote_chart_id = remote_chart_id;
            entry.remote_song_id = remote_song_id;
            return;
        }
    }

    store.charts.push_back(chart_mapping_entry{
        .server_url = server_url,
        .local_chart_id = local_chart_id,
        .local_song_id = local_song_id,
        .remote_chart_id = remote_chart_id,
        .remote_song_id = remote_song_id,
    });
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

bool read_text_file(const fs::path& path,
                    std::string& content,
                    std::string& error_message) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        error_message = "Failed to open a local chart file for upload.";
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        error_message = "Failed to read a local chart file for upload.";
        return false;
    }

    content = buffer.str();
    return true;
}

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

std::string build_external_links_json(const song_meta& meta) {
    struct external_link {
        const char* label;
        const std::string* url;
    };

    const external_link links[] = {
        {"YouTube", &meta.sns_youtube},
        {"Niconico", &meta.sns_niconico},
        {"X", &meta.sns_x},
    };

    std::ostringstream stream;
    stream << '[';
    bool first = true;
    for (const external_link& link : links) {
        if (link.url == nullptr || link.url->empty()) {
            continue;
        }

        if (!first) {
            stream << ',';
        }
        first = false;
        stream << "{\"url\":\"" << json::escape_string(*link.url)
               << "\",\"label\":\"" << json::escape_string(link.label) << "\"}";
    }
    stream << ']';
    return stream.str();
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
        0,
        {},
        "Community upload is only supported on Windows builds right now.",
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
    result.message = updated_existing ? "Song upload updated." : "Song uploaded.";
    return result;
}

upload_request_result parse_chart_upload_response(const http_response& response,
                                                  bool updated_existing) {
    upload_request_result result;
    result.updated_existing = updated_existing;

    if (!response.error_message.empty()) {
        result.message = response.error_message;
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
    result.message = updated_existing ? "Chart upload updated." : "Chart uploaded.";
    return result;
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
    const fs::path audio_path = song_dir / path_utils::from_utf8(meta.audio_file);
    const fs::path jacket_path = song_dir / path_utils::from_utf8(meta.jacket_file);
    if (!fs::exists(audio_path) || !fs::is_regular_file(audio_path)) {
        result.message = "Selected song audio file was not found.";
        return result;
    }
    if (!fs::exists(jacket_path) || !fs::is_regular_file(jacket_path)) {
        result.message = "Selected song jacket file was not found.";
        return result;
    }

    std::vector<unsigned char> audio_bytes;
    std::vector<unsigned char> jacket_bytes;
    if (!read_binary_file(audio_path, audio_bytes, result.message) ||
        !read_binary_file(jacket_path, jacket_bytes, result.message)) {
        return result;
    }

    std::vector<multipart_field> fields;
    fields.push_back({"title", meta.title});
    fields.push_back({"artist", meta.artist});
    {
        std::ostringstream bpm_stream;
        bpm_stream << meta.base_bpm;
        fields.push_back({"baseBpm", bpm_stream.str()});
    }
    if (meta.duration_seconds > 0.0f) {
        fields.push_back({"durationSec", std::to_string(static_cast<int>(meta.duration_seconds + 0.5f))});
    }
    fields.push_back({"previewStartMs", std::to_string(std::max(0, meta.preview_start_ms))});
    fields.push_back({"visibility", "public"});
    const std::string external_links_json = build_external_links_json(meta);
    if (external_links_json != "[]") {
        fields.push_back({"externalLinks", external_links_json});
    }

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

std::optional<std::string> find_remote_song_id(const auth::session& session,
                                               upload_mapping_store& store,
                                               const song_select::song_entry& song) {
    if (const std::optional<std::string> mapped = find_song_mapping(
            store, session.server_url, song.song.meta.song_id);
        mapped.has_value()) {
        return mapped;
    }

    if (song.song.meta.song_id.empty()) {
        return std::nullopt;
    }

    const title_online_view::remote_song_lookup_result lookup =
        title_online_view::fetch_remote_song_by_id(song.song.meta.song_id, session.server_url);
    if (!lookup.success || lookup.not_found || lookup.song.id.empty()) {
        return std::nullopt;
    }

    store_song_mapping(store, session.server_url, song.song.meta.song_id, lookup.song.id);
    return lookup.song.id;
}

std::optional<std::string> rewrite_chart_song_id(const std::string& content,
                                                 const std::string& remote_song_id,
                                                 std::string& error_message) {
    if (remote_song_id.empty()) {
        error_message = "Chart upload is missing a remote song ID.";
        return std::nullopt;
    }

    std::vector<std::string> lines;
    {
        std::string normalized = content;
        size_t position = 0;
        while ((position = normalized.find("\r\n", position)) != std::string::npos) {
            normalized.replace(position, 2, "\n");
        }

        std::istringstream stream(normalized);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
    }

    int metadata_header_index = -1;
    int metadata_end_index = static_cast<int>(lines.size());
    for (int index = 0; index < static_cast<int>(lines.size()); ++index) {
        const std::string trimmed = trim_ascii(lines[static_cast<size_t>(index)]);
        if (metadata_header_index < 0) {
            if (trimmed == "[Metadata]") {
                metadata_header_index = index;
            }
            continue;
        }
        if (!trimmed.empty() && trimmed.front() == '[' && trimmed.back() == ']') {
            metadata_end_index = index;
            break;
        }
    }

    if (metadata_header_index < 0) {
        error_message = "Selected chart does not contain a [Metadata] section.";
        return std::nullopt;
    }

    std::vector<std::string> ordered_keys;
    std::vector<std::pair<std::string, std::string>> metadata_values;
    auto find_metadata = [&](const std::string& key) {
        return std::find_if(metadata_values.begin(), metadata_values.end(), [&](const auto& entry) {
            return entry.first == key;
        });
    };

    for (int index = metadata_header_index + 1; index < metadata_end_index; ++index) {
        const std::string& line = lines[static_cast<size_t>(index)];
        const size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = trim_ascii(line.substr(0, separator));
        const std::string value = trim_ascii(line.substr(separator + 1));
        if (key.empty()) {
            continue;
        }

        if (find_metadata(key) == metadata_values.end()) {
            ordered_keys.push_back(key);
            metadata_values.emplace_back(key, value);
        }
    }

    auto song_id_entry = find_metadata("songId");
    if (song_id_entry == metadata_values.end()) {
        ordered_keys.push_back("songId");
        metadata_values.emplace_back("songId", remote_song_id);
    } else {
        song_id_entry->second = remote_song_id;
    }

    std::ostringstream rebuilt;
    for (int index = 0; index <= metadata_header_index; ++index) {
        rebuilt << lines[static_cast<size_t>(index)] << '\n';
    }
    for (const std::string& key : ordered_keys) {
        const auto entry = find_metadata(key);
        if (entry == metadata_values.end()) {
            continue;
        }
        rebuilt << entry->first << '=' << entry->second << '\n';
    }
    for (int index = metadata_end_index; index < static_cast<int>(lines.size()); ++index) {
        rebuilt << lines[static_cast<size_t>(index)];
        if (index + 1 < static_cast<int>(lines.size())) {
            rebuilt << '\n';
        }
    }

    return rebuilt.str();
}

upload_request_result send_chart_upload_request(const auth::session& session,
                                                const song_select::song_entry& song,
                                                const song_select::chart_option& chart,
                                                const std::string& remote_song_id,
                                                const std::optional<std::string>& remote_chart_id) {
    upload_request_result result;

    const fs::path chart_path = path_utils::from_utf8(chart.path);
    if (!fs::exists(chart_path) || !fs::is_regular_file(chart_path)) {
        result.message = "Selected chart file was not found.";
        return result;
    }

    std::string chart_content;
    if (!read_text_file(chart_path, chart_content, result.message)) {
        return result;
    }

    std::optional<std::string> rewritten_chart = rewrite_chart_song_id(chart_content, remote_song_id, result.message);
    if (!rewritten_chart.has_value()) {
        return result;
    }

    std::vector<multipart_field> fields;
    fields.push_back({"visibility", "public"});

    std::vector<unsigned char> chart_bytes(rewritten_chart->begin(), rewritten_chart->end());
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

}  // namespace

upload_result upload_song(const song_select::song_entry& song) {
    upload_result result;

    if (song.status != content_status::local) {
        result.message = "Only Local songs can be uploaded.";
        return result;
    }

    std::string error_message;
    const std::optional<auth::session> session_opt = require_saved_session(error_message);
    if (!session_opt.has_value()) {
        result.message = std::move(error_message);
        return result;
    }
    auth::session session = *session_opt;

    upload_mapping_store store = load_upload_mappings();
    const std::optional<std::string> existing_remote_song_id =
        find_song_mapping(store, session.server_url, song.song.meta.song_id);

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
        remove_song_mapping(store, session.server_url, song.song.meta.song_id);
        save_upload_mappings(store);
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
    result.message = request_result.message;
    result.should_refresh_online_catalog = request_result.success;
    if (!request_result.success) {
        return result;
    }

    store_song_mapping(store, session.server_url, song.song.meta.song_id, request_result.remote_song_id);
    if (!save_upload_mappings(store)) {
        result.message += " Local upload mapping was not saved.";
    } else if (!request_result.updated_existing) {
        result.message += " Charts for this song can now be uploaded.";
    }

    return result;
}

upload_result upload_chart(const song_select::song_entry& song,
                           const song_select::chart_option& chart) {
    upload_result result;

    if (song.status != content_status::local || chart.status != content_status::local) {
        result.message = "Only Local charts from Local songs can be uploaded.";
        return result;
    }

    std::string error_message;
    const std::optional<auth::session> session_opt = require_saved_session(error_message);
    if (!session_opt.has_value()) {
        result.message = std::move(error_message);
        return result;
    }
    auth::session session = *session_opt;

    upload_mapping_store store = load_upload_mappings();
    const std::optional<std::string> remote_song_id = find_remote_song_id(session, store, song);
    if (!remote_song_id.has_value()) {
        result.message = "Upload the song first so the server can assign a song ID.";
        return result;
    }

    const std::optional<std::string> existing_remote_chart_id =
        find_chart_mapping(store, session.server_url, chart.meta.chart_id);
    upload_request_result request_result =
        send_chart_upload_request(session, song, chart, *remote_song_id, existing_remote_chart_id);
    if (request_result.unauthorized) {
        std::string restore_error;
        const std::optional<auth::session> restored = restore_upload_session(restore_error);
        if (!restored.has_value()) {
            result.message = std::move(restore_error);
            return result;
        }
        session = *restored;
        request_result = send_chart_upload_request(session, song, chart, *remote_song_id, existing_remote_chart_id);
    }
    if (request_result.not_found && existing_remote_chart_id.has_value()) {
        remove_chart_mapping(store, session.server_url, chart.meta.chart_id);
        save_upload_mappings(store);
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

    result.success = request_result.success;
    result.message = request_result.message;
    result.should_refresh_online_catalog = request_result.success;
    if (!request_result.success) {
        return result;
    }

    store_song_mapping(store, session.server_url, song.song.meta.song_id, *remote_song_id);
    store_chart_mapping(store, session.server_url, chart.meta.chart_id, song.song.meta.song_id,
                        request_result.remote_chart_id, *remote_song_id);
    if (!save_upload_mappings(store)) {
        result.message += " Local upload mapping was not saved.";
    }

    return result;
}

}  // namespace title_create_upload
