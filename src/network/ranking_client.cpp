#include "network/ranking_client.h"

#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace {

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

std::optional<bool> extract_json_bool(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value()) {
        return std::nullopt;
    }

    if (content.compare(*start_opt, 4, "true") == 0) {
        return true;
    }

    if (content.compare(*start_opt, 5, "false") == 0) {
        return false;
    }

    return std::nullopt;
}

std::optional<int> extract_json_int(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value()) {
        return std::nullopt;
    }

    size_t end = *start_opt;
    if (content[end] == '-') {
        ++end;
    }
    while (end < content.size() && std::isdigit(static_cast<unsigned char>(content[end]))) {
        ++end;
    }
    if (end == *start_opt) {
        return std::nullopt;
    }

    try {
        return std::stoi(content.substr(*start_opt, end - *start_opt));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<float> extract_json_float(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value()) {
        return std::nullopt;
    }

    size_t end = *start_opt;
    if (content[end] == '-') {
        ++end;
    }
    while (end < content.size()) {
        const char ch = content[end];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '.')) {
            break;
        }
        ++end;
    }
    if (end == *start_opt) {
        return std::nullopt;
    }

    try {
        return std::stof(content.substr(*start_opt, end - *start_opt));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> extract_json_array(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value() || content[*start_opt] != '[') {
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

        if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return content.substr(*start_opt, index - *start_opt + 1);
            }
        }
    }

    return std::nullopt;
}

std::vector<std::string> extract_json_objects_from_array(const std::string& content) {
    std::vector<std::string> objects;
    bool in_string = false;
    bool escaping = false;
    size_t object_start = std::string::npos;
    size_t depth = 0;

    for (size_t index = 0; index < content.size(); ++index) {
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
            if (depth == 0) {
                object_start = index;
            }
            ++depth;
        } else if (ch == '}') {
            if (depth == 0) {
                continue;
            }
            --depth;
            if (depth == 0 && object_start != std::string::npos) {
                objects.push_back(content.substr(object_start, index - object_start + 1));
                object_start = std::string::npos;
            }
        }
    }

    return objects;
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
                           const std::vector<std::pair<std::string, std::string>>& headers,
                           const std::string& body = {}) {
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

    const BOOL sent = WinHttpSendRequest(request,
                                         header_block.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : header_block.c_str(),
                                         header_block.empty() ? 0 : static_cast<DWORD>(-1L),
                                         body.empty() ? WINHTTP_NO_REQUEST_DATA : reinterpret_cast<LPVOID>(const_cast<char*>(body.data())),
                                         static_cast<DWORD>(body.size()),
                                         static_cast<DWORD>(body.size()),
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
                           const std::vector<std::pair<std::string, std::string>>&,
                           const std::string&) {
    return {
        .status_code = 0,
        .body = {},
        .error_message = "Networking is only supported on Windows in the current build.",
    };
}
#endif

std::optional<ranking_service::entry> parse_ranking_entry(const std::string& content) {
    std::string player_display_name;
    if (const auto player_object = extract_json_object(content, "player"); player_object.has_value()) {
        player_display_name = extract_json_string(*player_object, "display_name").value_or("");
    }

    const auto placement = extract_json_int(content, "placement");
    const auto accuracy = extract_json_float(content, "accuracy");
    const auto is_full_combo = extract_json_bool(content, "is_full_combo");
    const auto max_combo = extract_json_int(content, "max_combo");
    const auto score = extract_json_int(content, "score");
    const auto recorded_at = extract_json_string(content, "recorded_at");
    const auto verified = extract_json_bool(content, "verified");
    const std::string clear_rank_label = extract_json_string(content, "clear_rank").value_or("");
    if (!placement.has_value() ||
        !accuracy.has_value() ||
        !is_full_combo.has_value() ||
        !max_combo.has_value() ||
        !score.has_value() ||
        !recorded_at.has_value() ||
        !verified.has_value()) {
        return std::nullopt;
    }

    return ranking_service::entry{
        .placement = *placement,
        .player_display_name = player_display_name,
        .accuracy = *accuracy,
        .is_full_combo = *is_full_combo,
        .max_combo = *max_combo,
        .score = *score,
        .recorded_at = *recorded_at,
        .verified = *verified,
        .resolved_clear_rank = clear_rank_label.empty()
            ? scoring_ruleset_runtime::compute_rank_for(
                scoring_ruleset_runtime::current_ruleset(),
                *accuracy,
                *is_full_combo)
            : [&clear_rank_label]() {
                if (clear_rank_label == "ss") return rank::ss;
                if (clear_rank_label == "s") return rank::s;
                if (clear_rank_label == "aa") return rank::aa;
                if (clear_rank_label == "a") return rank::a;
                if (clear_rank_label == "b") return rank::b;
                if (clear_rank_label == "c") return rank::c;
                return rank::f;
            }(),
    };
}

std::optional<ranking_client::listing_response> parse_listing_response(const std::string& body) {
    const bool available = extract_json_bool(body, "available").value_or(false);
    const std::string message = extract_json_string(body, "message").value_or("");

    ranking_client::listing_response listing;
    listing.available = available;
    listing.message = message;

    const std::optional<std::string> entries_array = extract_json_array(body, "entries");
    if (!entries_array.has_value()) {
        return listing;
    }

    for (const std::string& object : extract_json_objects_from_array(*entries_array)) {
        const auto entry = parse_ranking_entry(object);
        if (entry.has_value()) {
            listing.entries.push_back(*entry);
        }
    }

    return listing;
}

std::string build_ranking_url(const std::string& server_url, const std::string& chart_id, int limit) {
    const int clamped_limit = std::max(1, limit);
    return server_url + "/charts/" + chart_id + "/rankings?page=1&pageSize=" + std::to_string(clamped_limit);
}

std::string build_submit_ranking_url(const std::string& server_url, const std::string& chart_id) {
    return server_url + "/charts/" + chart_id + "/rankings";
}

std::string build_manifest_url(const std::string& server_url, const std::string& chart_id) {
    return server_url + "/charts/" + chart_id + "/official-manifest";
}

std::string judge_result_label(judge_result result) {
    switch (result) {
        case judge_result::perfect: return "perfect";
        case judge_result::great: return "great";
        case judge_result::good: return "good";
        case judge_result::bad: return "bad";
        case judge_result::miss: return "miss";
    }

    return "miss";
}

std::string build_note_results_json(const std::vector<note_result_entry>& note_results) {
    std::string json = "[";
    for (size_t i = 0; i < note_results.size(); ++i) {
        const note_result_entry& entry = note_results[i];
        if (i > 0) {
            json += ",";
        }
        json += "{";
        json += "\"event_index\":" + std::to_string(entry.event_index) + ",";
        json += "\"result\":\"" + judge_result_label(entry.result) + "\",";
        json += "\"offset_ms\":" + std::to_string(entry.offset_ms);
        json += "}";
    }
    json += "]";
    return json;
}

std::string build_submit_payload(const result_data& result,
                                 const std::string& recorded_at,
                                 const std::string& ruleset_version) {
    return "{"
        "\"recorded_at\":\"" + escape_json_string(recorded_at) + "\","
        "\"ruleset_version\":\"" + escape_json_string(ruleset_version) + "\","
        "\"note_results\":" + build_note_results_json(result.note_results) +
        "}";
}

std::optional<ranking_client::submit_response> parse_submit_response(const std::string& body) {
    ranking_client::submit_response response;
    response.available = extract_json_bool(body, "available").value_or(true);
    response.updated = extract_json_bool(body, "updated").value_or(false);
    response.message = extract_json_string(body, "message").value_or("");

    if (const auto entry_object = extract_json_object(body, "entry"); entry_object.has_value()) {
        response.entry = parse_ranking_entry(*entry_object);
    }

    return response;
}

std::optional<ranking_client::official_manifest> parse_official_manifest_response(const std::string& body) {
    const auto available = extract_json_bool(body, "available");
    const auto chart_id = extract_json_string(body, "chart_id");
    const auto song_id = extract_json_string(body, "song_id");
    if (!available.has_value() || !chart_id.has_value() || !song_id.has_value()) {
        return std::nullopt;
    }

    return ranking_client::official_manifest{
        .available = *available,
        .message = extract_json_string(body, "message").value_or(""),
        .chart_id = *chart_id,
        .song_id = *song_id,
        .song_json_sha256 = extract_json_string(body, "song_json_sha256").value_or(""),
        .audio_sha256 = extract_json_string(body, "audio_sha256").value_or(""),
        .jacket_sha256 = extract_json_string(body, "jacket_sha256").value_or(""),
        .chart_sha256 = extract_json_string(body, "chart_sha256").value_or(""),
    };
}

std::optional<ranking_client::scoring_ruleset> parse_scoring_ruleset_response(const std::string& body) {
    const auto active = extract_json_bool(body, "active");
    const auto accepted_input = extract_json_string(body, "accepted_input");
    const auto ruleset_version = extract_json_string(body, "ruleset_version");
    const auto score_model = extract_json_string(body, "score_model");
    const auto max_score = extract_json_int(body, "max_score");
    const auto judges_object = extract_json_object(body, "judges");
    const auto thresholds_array = extract_json_array(body, "rank_thresholds");
    if (!active.has_value() || !accepted_input.has_value() || !ruleset_version.has_value() ||
        !score_model.has_value() || !max_score.has_value() ||
        !judges_object.has_value() || !thresholds_array.has_value()) {
        return std::nullopt;
    }

    const auto perfect = extract_json_int(*judges_object, "perfect");
    const auto great = extract_json_int(*judges_object, "great");
    const auto good = extract_json_int(*judges_object, "good");
    const auto bad = extract_json_int(*judges_object, "bad");
    const auto miss = extract_json_int(*judges_object, "miss");
    if (!perfect.has_value() || !great.has_value() || !good.has_value() ||
        !bad.has_value() || !miss.has_value()) {
        return std::nullopt;
    }

    std::vector<scoring_ruleset_runtime::rank_threshold> rank_thresholds;
    for (const std::string& object : extract_json_objects_from_array(*thresholds_array)) {
        const auto rank_label = extract_json_string(object, "rank");
        const auto min_accuracy = extract_json_float(object, "min_accuracy");
        const auto requires_full_combo = extract_json_bool(object, "requires_full_combo");
        if (!rank_label.has_value() || !min_accuracy.has_value() || !requires_full_combo.has_value()) {
            return std::nullopt;
        }

        rank parsed_rank = rank::f;
        if (*rank_label == "ss") {
            parsed_rank = rank::ss;
        } else if (*rank_label == "s") {
            parsed_rank = rank::s;
        } else if (*rank_label == "aa") {
            parsed_rank = rank::aa;
        } else if (*rank_label == "a") {
            parsed_rank = rank::a;
        } else if (*rank_label == "b") {
            parsed_rank = rank::b;
        } else if (*rank_label == "c") {
            parsed_rank = rank::c;
        }

        rank_thresholds.push_back(scoring_ruleset_runtime::rank_threshold{
            .clear_rank = parsed_rank,
            .min_accuracy = *min_accuracy,
            .requires_full_combo = *requires_full_combo,
        });
    }

    return ranking_client::scoring_ruleset{
        .active = *active,
        .accepted_input = *accepted_input,
        .ruleset_version = *ruleset_version,
        .score_model = *score_model,
        .max_score = *max_score,
        .judge_values = {*perfect, *great, *good, *bad, *miss},
        .rank_thresholds = std::move(rank_thresholds),
    };
}

}  // namespace

namespace ranking_client {

operation_result fetch_chart_ranking(const std::string& server_url,
                                     const std::string& access_token,
                                     const std::string& chart_id,
                                     int limit) {
    if (server_url.empty()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = "No server URL is configured.",
            .listing = std::nullopt,
        };
    }

    if (access_token.empty()) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in to view online rankings.",
            .listing = std::nullopt,
        };
    }

    const http_response response = send_request(
        "GET",
        build_ranking_url(server_url, chart_id, limit),
        {
            {"Accept", "application/json"},
            {"Authorization", "Bearer " + access_token},
            {"User-Agent", "raythm/0.1"},
        });

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = response.error_message,
            .listing = std::nullopt,
        };
    }

    if (response.status_code == 401) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in to view online rankings.",
            .listing = std::nullopt,
        };
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return {
            .success = false,
            .unauthorized = false,
            .message = extract_json_string(response.body, "message").value_or("Failed to load online rankings."),
            .listing = std::nullopt,
        };
    }

    const std::optional<listing_response> listing = parse_listing_response(response.body);
    if (!listing.has_value()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = "Server returned an unexpected ranking response.",
            .listing = std::nullopt,
        };
    }

    return {
        .success = true,
        .unauthorized = false,
        .message = listing->message,
        .listing = listing,
    };
}

submit_operation_result submit_chart_ranking(const std::string& server_url,
                                             const std::string& access_token,
                                             const std::string& chart_id,
                                             const result_data& result,
                                             const std::string& recorded_at,
                                             const std::string& ruleset_version) {
    if (server_url.empty()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = "No server URL is configured.",
            .submission = std::nullopt,
        };
    }

    if (access_token.empty()) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in to submit online rankings.",
            .submission = std::nullopt,
        };
    }

    const http_response response = send_request(
        "POST",
        build_submit_ranking_url(server_url, chart_id),
        {
            {"Accept", "application/json"},
            {"Authorization", "Bearer " + access_token},
            {"Content-Type", "application/json"},
            {"User-Agent", "raythm/0.1"},
        },
        build_submit_payload(result, recorded_at, ruleset_version));

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = response.error_message,
            .submission = std::nullopt,
        };
    }

    if (response.status_code == 401) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in to submit online rankings.",
            .submission = std::nullopt,
        };
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return {
            .success = false,
            .unauthorized = false,
            .message = extract_json_string(response.body, "message").value_or("Failed to submit online ranking."),
            .submission = std::nullopt,
        };
    }

    const std::optional<submit_response> submission = parse_submit_response(response.body);
    if (!submission.has_value()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = "Server returned an unexpected ranking response.",
            .submission = std::nullopt,
        };
    }

    return {
        .success = true,
        .unauthorized = false,
        .message = submission->message,
        .submission = submission,
    };
}

scoring_ruleset_operation_result fetch_scoring_ruleset(const std::string& server_url) {
    if (server_url.empty()) {
        return {
            .success = false,
            .message = "No server URL is configured.",
            .ruleset = std::nullopt,
        };
    }

    const http_response response = send_request(
        "GET",
        server_url + "/scoring/ruleset",
        {
            {"Accept", "application/json"},
            {"User-Agent", "raythm/0.1"},
        });

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .message = response.error_message,
            .ruleset = std::nullopt,
        };
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return {
            .success = false,
            .message = extract_json_string(response.body, "message").value_or("Failed to fetch scoring ruleset."),
            .ruleset = std::nullopt,
        };
    }

    const std::optional<scoring_ruleset> ruleset = parse_scoring_ruleset_response(response.body);
    if (!ruleset.has_value()) {
        return {
            .success = false,
            .message = "Server returned an unexpected scoring ruleset response.",
            .ruleset = std::nullopt,
        };
    }

    return {
        .success = true,
        .message = {},
        .ruleset = ruleset,
    };
}

manifest_operation_result fetch_official_chart_manifest(const std::string& server_url,
                                                        const std::string& chart_id) {
    if (server_url.empty()) {
        return {
            .success = false,
            .message = "No server URL is configured.",
            .manifest = std::nullopt,
        };
    }

    const http_response response = send_request(
        "GET",
        build_manifest_url(server_url, chart_id),
        {
            {"Accept", "application/json"},
            {"User-Agent", "raythm/0.1"},
        });

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .message = response.error_message,
            .manifest = std::nullopt,
        };
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return {
            .success = false,
            .message = extract_json_string(response.body, "message").value_or("Failed to fetch official manifest."),
            .manifest = std::nullopt,
        };
    }

    const std::optional<official_manifest> manifest = parse_official_manifest_response(response.body);
    if (!manifest.has_value()) {
        return {
            .success = false,
            .message = "Server returned an unexpected official manifest response.",
            .manifest = std::nullopt,
        };
    }

    return {
        .success = true,
        .message = manifest->message,
        .manifest = manifest,
    };
}

}  // namespace ranking_client
