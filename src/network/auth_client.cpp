#include "network/auth_client.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "app_paths.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace {
namespace fs = std::filesystem;

struct http_response {
    int status_code = 0;
    std::string body;
    std::string error_message;
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
constexpr int kSendTimeoutMs = 5000;
constexpr int kReceiveTimeoutMs = 5000;
#endif

std::string trim(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

std::string escape_json_string(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += ch; break;
        }
    }
    return result;
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<size_t> find_json_key(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    return key_pos + token.size();
}

std::optional<size_t> find_value_start(const std::string& content, const std::string& key) {
    const auto key_end = find_json_key(content, key);
    if (!key_end.has_value()) {
        return std::nullopt;
    }

    const size_t colon_pos = content.find(':', *key_end);
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    size_t start = colon_pos + 1;
    while (start < content.size() && std::isspace(static_cast<unsigned char>(content[start]))) {
        ++start;
    }

    if (start >= content.size()) {
        return std::nullopt;
    }

    return start;
}

std::optional<std::string> extract_json_string(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value() || content[*start_opt] != '"') {
        return std::nullopt;
    }

    std::string result;
    bool escaping = false;
    for (size_t index = *start_opt + 1; index < content.size(); ++index) {
        const char ch = content[index];
        if (escaping) {
            switch (ch) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: result += ch; break;
            }
            escaping = false;
            continue;
        }

        if (ch == '\\') {
            escaping = true;
            continue;
        }

        if (ch == '"') {
            return result;
        }

        result += ch;
    }

    return std::nullopt;
}

std::optional<std::string> extract_json_object(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value() || content[*start_opt] != '{') {
        return std::nullopt;
    }

    size_t depth = 0;
    bool in_string = false;
    bool escaping = false;
    for (size_t index = *start_opt; index < content.size(); ++index) {
        const char ch = content[index];
        if (in_string) {
            if (escaping) {
                escaping = false;
                continue;
            }
            if (ch == '\\') {
                escaping = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }

        if (ch == '"') {
            in_string = true;
            continue;
        }

        if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                return content.substr(*start_opt, index - *start_opt + 1);
            }
        }
    }

    return std::nullopt;
}

std::optional<auth::public_user> parse_user_object(const std::string& content) {
    const std::optional<std::string> id = extract_json_string(content, "id");
    std::optional<std::string> email = extract_json_string(content, "email");
    if (!email.has_value()) {
        email = extract_json_string(content, "username");
    }
    const std::optional<std::string> display_name = extract_json_string(content, "displayName");
    if (!id.has_value() || !email.has_value() || !display_name.has_value()) {
        return std::nullopt;
    }

    return auth::public_user{
        .id = *id,
        .email = *email,
        .display_name = *display_name,
    };
}

std::optional<auth::session> parse_auth_session_response(const std::string& body, const std::string& server_url) {
    const std::optional<std::string> access_token = extract_json_string(body, "accessToken");
    const std::optional<std::string> refresh_token = extract_json_string(body, "refreshToken");
    const std::optional<std::string> user_object = extract_json_object(body, "user");
    if (!access_token.has_value() || !refresh_token.has_value() || !user_object.has_value()) {
        return std::nullopt;
    }

    const std::optional<auth::public_user> user = parse_user_object(*user_object);
    if (!user.has_value()) {
        return std::nullopt;
    }

    return auth::session{
        .server_url = server_url,
        .access_token = *access_token,
        .refresh_token = *refresh_token,
        .user = *user,
    };
}

std::optional<auth::public_user> parse_me_response(const std::string& body) {
    const std::optional<std::string> user_object = extract_json_object(body, "user");
    if (!user_object.has_value()) {
        return std::nullopt;
    }

    return parse_user_object(*user_object);
}

std::string parse_error_message(const std::string& body, std::string fallback) {
    const std::optional<std::string> message = extract_json_string(body, "message");
    return message.value_or(std::move(fallback));
}

bool write_session_file(const auth::session& session_data) {
    app_paths::ensure_directories();
    std::ofstream output(app_paths::auth_session_path(), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "{\n";
    output << "  \"serverUrl\": \"" << escape_json_string(session_data.server_url) << "\",\n";
    output << "  \"accessToken\": \"" << escape_json_string(session_data.access_token) << "\",\n";
    output << "  \"refreshToken\": \"" << escape_json_string(session_data.refresh_token) << "\",\n";
    output << "  \"user\": {\n";
    output << "    \"id\": \"" << escape_json_string(session_data.user.id) << "\",\n";
    output << "    \"email\": \"" << escape_json_string(session_data.user.email) << "\",\n";
    output << "    \"displayName\": \"" << escape_json_string(session_data.user.display_name) << "\"\n";
    output << "  }\n";
    output << "}\n";
    return output.good();
}

#ifdef _WIN32
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

http_response send_request(const std::string& method,
                           const std::string& url,
                           const std::string& body,
                           const std::vector<std::pair<std::string, std::string>>& headers) {
    http_response response;

    const std::optional<http_url_parts> parts = parse_url_parts(url);
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

    const DWORD request_flags = parts->secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection,
                                           to_wstring(method).c_str(),
                                           parts->path_and_query.c_str(),
                                           nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           request_flags);
    if (request == nullptr) {
        response.error_message = describe_winhttp_error(GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }

    std::wstring header_block;
    for (const auto& [name, value] : headers) {
        header_block += to_wstring(name);
        header_block += L": ";
        header_block += to_wstring(value);
        header_block += L"\r\n";
    }

    LPVOID request_body = body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data());
    DWORD request_body_size = static_cast<DWORD>(body.size());
    const BOOL sent = WinHttpSendRequest(request,
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
        response.error_message = describe_winhttp_error(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return response;
    }

    response.status_code = static_cast<int>(status_code);

    DWORD available_size = 0;
    while (WinHttpQueryDataAvailable(request, &available_size) == TRUE && available_size > 0) {
        std::string chunk(available_size, '\0');
        DWORD bytes_read = 0;
        if (WinHttpReadData(request, chunk.data(), available_size, &bytes_read) == FALSE) {
            response.error_message = describe_winhttp_error(GetLastError());
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return response;
        }

        chunk.resize(bytes_read);
        response.body += chunk;
        available_size = 0;
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
        .error_message = "Networking is only supported on Windows in the current build.",
    };
}
#endif

std::string build_auth_url(const std::string& server_url, std::string_view path) {
    return auth::normalize_server_url(server_url) + std::string(path);
}

auth::operation_result finish_with_session(auth::operation_result result, const auth::session& session_data) {
    result.session_data = session_data;
    return result;
}

auth::operation_result parse_auth_response(const http_response& response,
                                           const std::string& server_url,
                                           std::string success_message) {
    if (!response.error_message.empty()) {
        return {
            .success = false,
            .message = response.error_message,
            .session_data = std::nullopt,
        };
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return {
            .success = false,
            .message = parse_error_message(response.body, "Authentication request failed."),
            .session_data = std::nullopt,
        };
    }

    const std::optional<auth::session> session_data =
        parse_auth_session_response(response.body, auth::normalize_server_url(server_url));
    if (!session_data.has_value()) {
        return {
            .success = false,
            .message = "Server returned an unexpected authentication response.",
            .session_data = std::nullopt,
        };
    }

    if (!write_session_file(*session_data)) {
        return {
            .success = false,
            .message = "Authentication succeeded, but the local session could not be saved.",
            .session_data = std::nullopt,
        };
    }

    return finish_with_session({
        .success = true,
        .message = std::move(success_message),
        .session_data = std::nullopt,
    }, *session_data);
}

}  // namespace

namespace auth {

std::string normalize_server_url(const std::string& server_url) {
    std::string normalized = trim(server_url);
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

std::optional<session> load_saved_session() {
    const std::string content = read_file(app_paths::auth_session_path());
    if (content.empty()) {
        return std::nullopt;
    }

    const std::optional<std::string> server_url = extract_json_string(content, "serverUrl");
    const std::optional<std::string> access_token = extract_json_string(content, "accessToken");
    const std::optional<std::string> refresh_token = extract_json_string(content, "refreshToken");
    const std::optional<std::string> user_object = extract_json_object(content, "user");
    if (!server_url.has_value() || !access_token.has_value() || !refresh_token.has_value() || !user_object.has_value()) {
        return std::nullopt;
    }

    const std::optional<public_user> user = parse_user_object(*user_object);
    if (!user.has_value()) {
        return std::nullopt;
    }

    return session{
        .server_url = normalize_server_url(*server_url),
        .access_token = *access_token,
        .refresh_token = *refresh_token,
        .user = *user,
    };
}

session_summary load_session_summary() {
    const std::optional<session> stored = load_saved_session();
    if (!stored.has_value()) {
        return {
            .logged_in = false,
            .server_url = kDefaultServerUrl,
            .email = {},
            .display_name = {},
        };
    }

    return {
        .logged_in = true,
        .server_url = stored->server_url,
        .email = stored->user.email,
        .display_name = stored->user.display_name,
    };
}

bool save_session(const session& session_data) {
    return write_session_file(session_data);
}

void clear_saved_session() {
    std::error_code ec;
    fs::remove(app_paths::auth_session_path(), ec);
}

operation_result register_user(const std::string& server_url,
                               const std::string& email,
                               const std::string& display_name,
                               const std::string& password) {
    const std::string normalized_server_url = normalize_server_url(server_url);
    if (normalized_server_url.empty()) {
        return {
            .success = false,
            .message = "Server URL is required.",
            .session_data = std::nullopt,
        };
    }

    const std::string trimmed_display_name = trim(display_name);
    const std::string body =
        "{"
        "\"email\":\"" + escape_json_string(trim(email)) + "\","
        "\"displayName\":\"" + escape_json_string(trimmed_display_name) + "\","
        "\"password\":\"" + escape_json_string(password) + "\""
        "}";

    const http_response response = send_request(
        "POST",
        build_auth_url(normalized_server_url, "/auth/register"),
        body,
        {
            {"Accept", "application/json"},
            {"Content-Type", "application/json"},
            {"User-Agent", "raythm/0.1"},
        });

    return parse_auth_response(response, normalized_server_url, "Account created successfully.");
}

operation_result login_user(const std::string& server_url,
                            const std::string& email,
                            const std::string& password) {
    const std::string normalized_server_url = normalize_server_url(server_url);
    if (normalized_server_url.empty()) {
        return {
            .success = false,
            .message = "Server URL is required.",
            .session_data = std::nullopt,
        };
    }

    const std::string body =
        "{"
        "\"email\":\"" + escape_json_string(trim(email)) + "\","
        "\"password\":\"" + escape_json_string(password) + "\""
        "}";

    const http_response response = send_request(
        "POST",
        build_auth_url(normalized_server_url, "/auth/login"),
        body,
        {
            {"Accept", "application/json"},
            {"Content-Type", "application/json"},
            {"User-Agent", "raythm/0.1"},
        });

    return parse_auth_response(response, normalized_server_url, "Logged in successfully.");
}

operation_result restore_saved_session() {
    const std::optional<session> stored = load_saved_session();
    if (!stored.has_value()) {
        return {
            .success = false,
            .message = "No saved session was found.",
            .session_data = std::nullopt,
        };
    }

    const http_response me_response = send_request(
        "GET",
        build_auth_url(stored->server_url, "/me"),
        {},
        {
            {"Accept", "application/json"},
            {"Authorization", "Bearer " + stored->access_token},
            {"User-Agent", "raythm/0.1"},
        });

    if (me_response.error_message.empty() && me_response.status_code >= 200 && me_response.status_code < 300) {
        const std::optional<public_user> user = parse_me_response(me_response.body);
        if (user.has_value()) {
            session restored = *stored;
            restored.user = *user;
            if (!write_session_file(restored)) {
                return {
                    .success = false,
                    .message = "Session restored, but the local session file could not be updated.",
                    .session_data = std::nullopt,
                };
            }

            return finish_with_session({
                .success = true,
                .message = "Session restored.",
                .session_data = std::nullopt,
            }, restored);
        }
    }

    if (!me_response.error_message.empty()) {
        return {
            .success = false,
            .message = me_response.error_message,
            .session_data = std::nullopt,
        };
    }

    if (me_response.status_code != 401) {
        return {
            .success = false,
            .message = parse_error_message(me_response.body, "Failed to restore session."),
            .session_data = std::nullopt,
        };
    }

    const std::string refresh_body =
        "{"
        "\"refreshToken\":\"" + escape_json_string(stored->refresh_token) + "\""
        "}";

    const http_response refresh_response = send_request(
        "POST",
        build_auth_url(stored->server_url, "/auth/refresh"),
        refresh_body,
        {
            {"Accept", "application/json"},
            {"Content-Type", "application/json"},
            {"User-Agent", "raythm/0.1"},
        });

    if (!refresh_response.error_message.empty()) {
        return {
            .success = false,
            .message = refresh_response.error_message,
            .session_data = std::nullopt,
        };
    }

    if (refresh_response.status_code < 200 || refresh_response.status_code >= 300) {
        clear_saved_session();
        return {
            .success = false,
            .message = parse_error_message(refresh_response.body, "Saved session expired."),
            .session_data = std::nullopt,
        };
    }

    const std::optional<session> refreshed = parse_auth_session_response(refresh_response.body, stored->server_url);
    if (!refreshed.has_value()) {
        clear_saved_session();
        return {
            .success = false,
            .message = "Server returned an unexpected refresh response.",
            .session_data = std::nullopt,
        };
    }

    if (!write_session_file(*refreshed)) {
        return {
            .success = false,
            .message = "Session refreshed, but the local session could not be saved.",
            .session_data = std::nullopt,
        };
    }

    return finish_with_session({
        .success = true,
        .message = "Session refreshed.",
        .session_data = std::nullopt,
    }, *refreshed);
}

operation_result logout_saved_session() {
    const std::optional<session> stored = load_saved_session();
    if (!stored.has_value()) {
        clear_saved_session();
        return {
            .success = true,
            .message = "Already logged out.",
            .session_data = std::nullopt,
        };
    }

    const std::string body =
        "{"
        "\"refreshToken\":\"" + escape_json_string(stored->refresh_token) + "\""
        "}";

    const http_response response = send_request(
        "POST",
        build_auth_url(stored->server_url, "/auth/logout"),
        body,
        {
            {"Accept", "application/json"},
            {"Content-Type", "application/json"},
            {"User-Agent", "raythm/0.1"},
        });

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .message = response.error_message,
            .session_data = std::nullopt,
        };
    }

    if (response.status_code != 204 &&
        (response.status_code < 200 || response.status_code >= 300) &&
        response.status_code != 401) {
        return {
            .success = false,
            .message = parse_error_message(response.body, "Failed to log out."),
            .session_data = std::nullopt,
        };
    }

    clear_saved_session();
    return {
        .success = true,
        .message = "Logged out successfully.",
        .session_data = std::nullopt,
    };
}

}  // namespace auth
