#include "network/auth_client.h"

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
#include "network/http_client.h"
#include "network/json_helpers.h"

namespace {
namespace fs = std::filesystem;
namespace json = network::json;
using http_response = network::http::response;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<auth::public_user> parse_user_object(const std::string& content) {
    const std::optional<std::string> id = json::extract_string(content, "id");
    std::optional<std::string> email = json::extract_string(content, "email");
    if (!email.has_value()) {
        email = json::extract_string(content, "username");
    }
    const std::optional<std::string> display_name = json::extract_string(content, "displayName");
    if (!id.has_value() || !email.has_value() || !display_name.has_value()) {
        return std::nullopt;
    }

    const bool email_verified = json::extract_bool(content, "emailVerified").value_or(false);

    return auth::public_user{
        .id = *id,
        .email = *email,
        .display_name = *display_name,
        .email_verified = email_verified,
    };
}

std::optional<auth::session> parse_auth_session_response(const std::string& body, const std::string& server_url) {
    const std::optional<std::string> access_token = json::extract_string(body, "accessToken");
    const std::optional<std::string> refresh_token = json::extract_string(body, "refreshToken");
    const std::optional<std::string> user_object = json::extract_object(body, "user");
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

auth::verification_purpose parse_verification_purpose(const std::string& value) {
    if (value == "email_verification") {
        return auth::verification_purpose::email_verification;
    }
    if (value == "login_verification") {
        return auth::verification_purpose::login_verification;
    }
    return auth::verification_purpose::none;
}

std::optional<auth::operation_result> parse_verification_required_response(const std::string& body) {
    if (!json::extract_bool(body, "verificationRequired").value_or(false)) {
        return std::nullopt;
    }

    const std::string purpose_text =
        json::extract_string(body, "verificationPurpose").value_or("email_verification");
    const auth::verification_purpose purpose = parse_verification_purpose(purpose_text);
    if (purpose == auth::verification_purpose::none) {
        return std::nullopt;
    }

    return auth::operation_result{
        .success = false,
        .message = json::extract_string(body, "message").value_or("Verification code required."),
        .session_data = std::nullopt,
        .verification_required = true,
        .verification = purpose,
        .verification_email = json::extract_string(body, "email").value_or(""),
    };
}

std::optional<auth::public_user> parse_me_response(const std::string& body) {
    const std::optional<std::string> user_object = json::extract_object(body, "user");
    if (!user_object.has_value()) {
        return std::nullopt;
    }

    return parse_user_object(*user_object);
}

std::optional<auth::community_song_upload> parse_community_song_upload(const std::string& object,
                                                                        const std::string& user_id) {
    const std::optional<std::string> uploader_object = json::extract_object(object, "uploader");
    if (!uploader_object.has_value() || json::extract_string(*uploader_object, "id").value_or("") != user_id) {
        return std::nullopt;
    }

    const std::optional<std::string> id = json::extract_string(object, "id");
    const std::optional<std::string> title = json::extract_string(object, "title");
    if (!id.has_value() || !title.has_value()) {
        return std::nullopt;
    }

    return auth::community_song_upload{
        .id = *id,
        .client_song_id = json::extract_string(object, "clientSongId").value_or(""),
        .title = *title,
        .artist = json::extract_string(object, "artist").value_or(""),
        .content_source = json::extract_string(object, "contentSource").value_or("community"),
        .visibility = json::extract_string(object, "visibility").value_or("public"),
        .base_bpm = json::extract_float(object, "baseBpm").value_or(0.0f),
        .duration_seconds = json::extract_float(object, "durationSec").value_or(0.0f),
        .preview_start_ms = json::extract_int(object, "previewStartMs").value_or(0),
        .song_version = json::extract_int(object, "songVersion").value_or(0),
    };
}

std::optional<auth::community_chart_upload> parse_community_chart_upload(const std::string& object,
                                                                          const std::string& user_id) {
    const std::optional<std::string> uploader_object = json::extract_object(object, "uploader");
    if (!uploader_object.has_value() || json::extract_string(*uploader_object, "id").value_or("") != user_id) {
        return std::nullopt;
    }

    const std::optional<std::string> id = json::extract_string(object, "id");
    if (!id.has_value()) {
        return std::nullopt;
    }

    const std::optional<std::string> song_object = json::extract_object(object, "song");
    return auth::community_chart_upload{
        .id = *id,
        .client_chart_id = json::extract_string(object, "clientChartId").value_or(""),
        .song_id = json::extract_string(object, "songId")
            .value_or(song_object.has_value() ? json::extract_string(*song_object, "id").value_or("") : ""),
        .client_song_id = json::extract_string(object, "clientSongId").value_or(""),
        .song_title = song_object.has_value() ? json::extract_string(*song_object, "title").value_or("") : "",
        .difficulty_name = json::extract_string(object, "difficultyName").value_or(""),
        .chart_author = json::extract_string(object, "chartAuthor").value_or(""),
        .content_source = json::extract_string(object, "contentSource").value_or("community"),
        .visibility = json::extract_string(object, "visibility").value_or("public"),
        .key_count = json::extract_int(object, "keyCount").value_or(0),
        .level = json::extract_float(object, "calculatedLevel").value_or(0.0f),
        .note_count = json::extract_int(object, "noteCount").value_or(0),
        .min_bpm = json::extract_float(object, "minBpm").value_or(0.0f),
        .max_bpm = json::extract_float(object, "maxBpm").value_or(0.0f),
        .difficulty_ruleset_id = json::extract_string(object, "difficultyRulesetId").value_or(""),
        .difficulty_ruleset_version = json::extract_int(object, "difficultyRulesetVersion").value_or(0),
    };
}

std::optional<auth::profile_ranking_record> parse_profile_ranking_record(const std::string& object) {
    const std::optional<std::string> chart_id = json::extract_string(object, "chartId");
    const std::optional<std::string> song_id = json::extract_string(object, "songId");
    const std::optional<std::string> song_title = json::extract_string(object, "songTitle");
    const std::optional<std::string> difficulty_name = json::extract_string(object, "difficultyName");
    const std::optional<int> score = json::extract_int(object, "score");
    const std::optional<int> placement = json::extract_int(object, "placement");
    if (!chart_id.has_value() || !song_id.has_value() || !song_title.has_value() ||
        !difficulty_name.has_value() || !score.has_value() || !placement.has_value()) {
        return std::nullopt;
    }

    return auth::profile_ranking_record{
        .chart_id = *chart_id,
        .song_id = *song_id,
        .song_title = *song_title,
        .artist = json::extract_string(object, "artist").value_or(""),
        .difficulty_name = *difficulty_name,
        .chart_author = json::extract_string(object, "chartAuthor").value_or(""),
        .clear_rank = json::extract_string(object, "clearRank").value_or(""),
        .recorded_at = json::extract_string(object, "recordedAt").value_or(""),
        .submitted_at = json::extract_string(object, "submittedAt").value_or(""),
        .score = *score,
        .placement = *placement,
        .max_combo = json::extract_int(object, "maxCombo").value_or(0),
        .accuracy = json::extract_float(object, "accuracy").value_or(0.0f),
        .is_full_combo = json::extract_bool(object, "isFullCombo").value_or(false),
    };
}

void parse_profile_ranking_array(const std::string& body,
                                 const char* key,
                                 std::vector<auth::profile_ranking_record>& output) {
    const std::optional<std::string> array = json::extract_array(body, key);
    if (!array.has_value()) {
        return;
    }

    for (const std::string& object : json::extract_objects_from_array(*array)) {
        if (const auto record = parse_profile_ranking_record(object); record.has_value()) {
            output.push_back(*record);
        }
    }
}

std::string parse_error_message(const std::string& body, std::string fallback) {
    const std::optional<std::string> message = json::extract_string(body, "message");
    return message.value_or(std::move(fallback));
}

std::string migrate_legacy_server_url(const std::string& server_url) {
    const std::string normalized = auth::normalize_server_url(server_url);
    return normalized;
}

void push_candidate_server_url(std::vector<std::string>& urls, const std::string& server_url) {
    const std::string normalized = auth::normalize_server_url(server_url);
    if (normalized.empty()) {
        return;
    }

    for (const std::string& existing : urls) {
        if (existing == normalized) {
            return;
        }
    }
    urls.push_back(normalized);
}

std::vector<std::string> candidate_server_urls(const std::string& server_url) {
    std::vector<std::string> urls;
    push_candidate_server_url(urls, server_url);

    const std::string normalized = auth::normalize_server_url(server_url);
    if (normalized == auth::normalize_server_url(auth::kDefaultServerUrl)) {
        push_candidate_server_url(urls, auth::kLanServerUrl);
    } else if (normalized == auth::normalize_server_url(auth::kLanServerUrl) ||
               normalized == auth::normalize_server_url(auth::kLegacyLanServerUrl)) {
        push_candidate_server_url(urls, auth::kDefaultServerUrl);
    }

    return urls;
}

bool server_urls_match(const std::string& lhs, const std::string& rhs) {
    const std::vector<std::string> lhs_candidates = candidate_server_urls(lhs);
    const std::vector<std::string> rhs_candidates = candidate_server_urls(rhs);
    for (const std::string& lhs_candidate : lhs_candidates) {
        for (const std::string& rhs_candidate : rhs_candidates) {
            if (lhs_candidate == rhs_candidate) {
                return true;
            }
        }
    }
    return false;
}

bool write_session_file(const auth::session& session_data) {
    app_paths::ensure_directories();
    std::ofstream output(app_paths::auth_session_path(), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "{\n";
    output << "  \"serverUrl\": \"" << json::escape_string(migrate_legacy_server_url(session_data.server_url)) << "\",\n";
    output << "  \"accessToken\": \"" << json::escape_string(session_data.access_token) << "\",\n";
    output << "  \"refreshToken\": \"" << json::escape_string(session_data.refresh_token) << "\",\n";
    output << "  \"user\": {\n";
    output << "    \"id\": \"" << json::escape_string(session_data.user.id) << "\",\n";
    output << "    \"email\": \"" << json::escape_string(session_data.user.email) << "\",\n";
    output << "    \"displayName\": \"" << json::escape_string(session_data.user.display_name) << "\",\n";
    output << "    \"emailVerified\": " << (session_data.user.email_verified ? "true" : "false") << "\n";
    output << "  }\n";
    output << "}\n";
    return output.good();
}

std::optional<std::string> read_trusted_device_token(const std::string& server_url, const std::string& email) {
    const std::string content = read_file(app_paths::auth_device_path());
    if (content.empty()) {
        return std::nullopt;
    }

    const std::optional<std::string> stored_server_url = json::extract_string(content, "serverUrl");
    const std::optional<std::string> stored_email = json::extract_string(content, "email");
    const std::optional<std::string> device_token = json::extract_string(content, "trustedDeviceToken");
    if (!stored_server_url.has_value() || !stored_email.has_value() || !device_token.has_value()) {
        return std::nullopt;
    }

    if (!server_urls_match(*stored_server_url, server_url) || json::trim(*stored_email) != json::trim(email)) {
        return std::nullopt;
    }

    return *device_token;
}

bool write_trusted_device_token(const std::string& server_url, const std::string& email, const std::string& token) {
    if (token.empty()) {
        return true;
    }

    app_paths::ensure_directories();
    std::ofstream output(app_paths::auth_device_path(), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output << "{\n";
    output << "  \"serverUrl\": \"" << json::escape_string(migrate_legacy_server_url(server_url)) << "\",\n";
    output << "  \"email\": \"" << json::escape_string(json::trim(email)) << "\",\n";
    output << "  \"trustedDeviceToken\": \"" << json::escape_string(token) << "\"\n";
    output << "}\n";
    return output.good();
}

http_response send_request(const std::string& method,
                           const std::string& url,
                           const std::string& body,
                           const std::vector<std::pair<std::string, std::string>>& headers) {
    return network::http::send_request(method, url, headers, body);
}

std::string build_auth_url(const std::string& server_url, std::string_view path) {
    return auth::normalize_server_url(server_url) + std::string(path);
}

auth::operation_result finish_with_session(auth::operation_result result, const auth::session& session_data) {
    result.session_data = session_data;
    return result;
}

http_response send_authenticated_request(const auth::session& session_data,
                                         const std::string& method,
                                         std::string_view path,
                                         const std::string& body = {}) {
    std::vector<std::pair<std::string, std::string>> headers = {
        {"Accept", "application/json"},
        {"Authorization", "Bearer " + session_data.access_token},
        {"User-Agent", "raythm/0.1"},
    };
    if (!body.empty()) {
        headers.emplace_back("Content-Type", "application/json");
    }

    http_response last_response;
    for (const std::string& server_url : candidate_server_urls(session_data.server_url)) {
        last_response = send_request(method, build_auth_url(server_url, path), body, headers);
        if (last_response.error_message.empty()) {
            return last_response;
        }
    }
    return last_response;
}

auth::operation_result parse_auth_response(const http_response& response,
                                           const std::string& server_url,
                                           std::string success_message,
                                           const std::string& email_for_device = {}) {
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

    if (const std::optional<auth::operation_result> verification = parse_verification_required_response(response.body);
        verification.has_value()) {
        return *verification;
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

    if (const std::optional<std::string> trusted_device_token =
            json::extract_string(response.body, "trustedDeviceToken");
        trusted_device_token.has_value()) {
        const std::string device_email = email_for_device.empty() ? session_data->user.email : email_for_device;
        write_trusted_device_token(server_url, device_email, *trusted_device_token);
    }

    return finish_with_session({
        .success = true,
        .message = std::move(success_message),
        .session_data = std::nullopt,
    }, *session_data);
}

auth::operation_result send_auth_json_with_fallback(const std::string& server_url,
                                                    std::string_view path,
                                                    const std::string& body,
                                                    std::string success_message,
                                                    const std::string& email_for_device = {}) {
    auth::operation_result last_result{
        .success = false,
        .message = "Server URL is required.",
        .session_data = std::nullopt,
    };
    for (const std::string& candidate_url : candidate_server_urls(server_url)) {
        const http_response response = send_request(
            "POST",
            build_auth_url(candidate_url, path),
            body,
            {
                {"Accept", "application/json"},
                {"Content-Type", "application/json"},
                {"User-Agent", "raythm/0.1"},
            });
        last_result = parse_auth_response(response, candidate_url, success_message, email_for_device);
        if (response.error_message.empty()) {
            return last_result;
        }
    }
    return last_result;
}

}  // namespace

namespace auth {

std::string normalize_server_url(const std::string& server_url) {
    std::string normalized = json::trim(server_url);
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

    const std::optional<std::string> server_url = json::extract_string(content, "serverUrl");
    const std::optional<std::string> access_token = json::extract_string(content, "accessToken");
    const std::optional<std::string> refresh_token = json::extract_string(content, "refreshToken");
    const std::optional<std::string> user_object = json::extract_object(content, "user");
    if (!server_url.has_value() || !access_token.has_value() || !refresh_token.has_value() || !user_object.has_value()) {
        return std::nullopt;
    }

    const std::optional<public_user> user = parse_user_object(*user_object);
    if (!user.has_value()) {
        return std::nullopt;
    }

    session loaded{
        .server_url = migrate_legacy_server_url(*server_url),
        .access_token = *access_token,
        .refresh_token = *refresh_token,
        .user = *user,
    };
    if (loaded.server_url != normalize_server_url(*server_url)) {
        write_session_file(loaded);
    }
    return loaded;
}

session_summary load_session_summary() {
    const std::optional<session> stored = load_saved_session();
    if (!stored.has_value()) {
        return {
            .logged_in = false,
            .server_url = kDefaultServerUrl,
            .email = {},
            .display_name = {},
            .email_verified = false,
        };
    }

    return {
        .logged_in = true,
        .server_url = stored->server_url,
        .email = stored->user.email,
        .display_name = stored->user.display_name,
        .email_verified = stored->user.email_verified,
    };
}

bool save_session(const session& session_data) {
    return write_session_file(session_data);
}

void clear_saved_session() {
    std::error_code ec;
    fs::remove(app_paths::auth_session_path(), ec);
}

void clear_trusted_device_file() {
    std::error_code ec;
    fs::remove(app_paths::auth_device_path(), ec);
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

    const std::string trimmed_display_name = json::trim(display_name);
    const std::string body =
        "{"
        "\"email\":\"" + json::escape_string(json::trim(email)) + "\","
        "\"displayName\":\"" + json::escape_string(trimmed_display_name) + "\","
        "\"password\":\"" + json::escape_string(password) + "\""
        "}";

    return send_auth_json_with_fallback(
        normalized_server_url,
        "/auth/register",
        body,
        "Account created successfully.",
        json::trim(email));
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

    const std::optional<std::string> trusted_device_token =
        read_trusted_device_token(normalized_server_url, json::trim(email));
    const std::string body =
        "{"
        "\"email\":\"" + json::escape_string(json::trim(email)) + "\","
        "\"password\":\"" + json::escape_string(password) + "\""
        + (trusted_device_token.has_value()
               ? ",\"deviceToken\":\"" + json::escape_string(*trusted_device_token) + "\""
               : "") +
        "}";

    return send_auth_json_with_fallback(
        normalized_server_url,
        "/auth/login",
        body,
        "Logged in successfully.",
        json::trim(email));
}

operation_result verify_email_code(const std::string& server_url,
                                   const std::string& email,
                                   const std::string& code) {
    const std::string normalized_server_url = normalize_server_url(server_url);
    const std::string body =
        "{"
        "\"email\":\"" + json::escape_string(json::trim(email)) + "\","
        "\"code\":\"" + json::escape_string(json::trim(code)) + "\""
        "}";

    return send_auth_json_with_fallback(
        normalized_server_url,
        "/auth/verify-email",
        body,
        "Email verified.",
        json::trim(email));
}

operation_result verify_login_code(const std::string& server_url,
                                   const std::string& email,
                                   const std::string& code) {
    const std::string normalized_server_url = normalize_server_url(server_url);
    const std::string body =
        "{"
        "\"email\":\"" + json::escape_string(json::trim(email)) + "\","
        "\"code\":\"" + json::escape_string(json::trim(code)) + "\""
        "}";

    return send_auth_json_with_fallback(
        normalized_server_url,
        "/auth/verify-login",
        body,
        "Login verified.",
        json::trim(email));
}

operation_result resend_verification_code(const std::string& server_url,
                                          const std::string& email,
                                          verification_purpose purpose) {
    const std::string normalized_server_url = normalize_server_url(server_url);
    const char* purpose_text = purpose == verification_purpose::login_verification
        ? "login_verification"
        : "email_verification";
    const std::string body =
        "{"
        "\"email\":\"" + json::escape_string(json::trim(email)) + "\","
        "\"purpose\":\"" + purpose_text + "\""
        "}";

    http_response response;
    for (const std::string& candidate_url : candidate_server_urls(normalized_server_url)) {
        response = send_request(
            "POST",
            build_auth_url(candidate_url, "/auth/resend-code"),
            body,
            {
                {"Accept", "application/json"},
                {"Content-Type", "application/json"},
                {"User-Agent", "raythm/0.1"},
            });
        if (response.error_message.empty()) {
            break;
        }
    }

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
            .message = parse_error_message(response.body, "Failed to resend code."),
            .session_data = std::nullopt,
        };
    }
    return {
        .success = true,
        .message = parse_error_message(response.body, "Verification code sent."),
        .session_data = std::nullopt,
    };
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

    http_response me_response;
    std::string active_server_url = stored->server_url;
    for (const std::string& candidate_url : candidate_server_urls(stored->server_url)) {
        me_response = send_request(
            "GET",
            build_auth_url(candidate_url, "/me"),
            {},
            {
                {"Accept", "application/json"},
                {"Authorization", "Bearer " + stored->access_token},
                {"User-Agent", "raythm/0.1"},
            });
        active_server_url = candidate_url;
        if (me_response.error_message.empty()) {
            break;
        }
    }

    if (me_response.error_message.empty() && me_response.status_code >= 200 && me_response.status_code < 300) {
        const std::optional<public_user> user = parse_me_response(me_response.body);
        if (user.has_value()) {
            session restored = *stored;
            restored.server_url = active_server_url;
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
        "\"refreshToken\":\"" + json::escape_string(stored->refresh_token) + "\""
        "}";

    const http_response refresh_response = send_request(
        "POST",
        build_auth_url(active_server_url, "/auth/refresh"),
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

    const std::optional<session> refreshed = parse_auth_session_response(refresh_response.body, active_server_url);
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
        "\"refreshToken\":\"" + json::escape_string(stored->refresh_token) + "\""
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

operation_result delete_saved_account(const std::string& password) {
    if (json::trim(password).empty()) {
        return {
            .success = false,
            .message = "Password is required to delete the account.",
            .session_data = std::nullopt,
        };
    }

    std::optional<session> stored = load_saved_session();
    if (!stored.has_value()) {
        clear_saved_session();
        clear_trusted_device_file();
        return {
            .success = true,
            .message = "Account session was already cleared.",
            .session_data = std::nullopt,
        };
    }

    const std::string body =
        "{"
        "\"password\":\"" + json::escape_string(password) + "\""
        "}";

    auto send_delete = [&](const session& session_data) {
        return send_request(
            "DELETE",
            build_auth_url(session_data.server_url, "/me"),
            body,
            {
                {"Accept", "application/json"},
                {"Authorization", "Bearer " + session_data.access_token},
                {"Content-Type", "application/json"},
                {"User-Agent", "raythm/0.1"},
            });
    };

    http_response response = send_delete(*stored);
    if (response.error_message.empty() && response.status_code == 401) {
        const operation_result restored = restore_saved_session();
        if (restored.success && restored.session_data.has_value()) {
            stored = restored.session_data;
            response = send_delete(*stored);
        }
    }

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .message = response.error_message,
            .session_data = std::nullopt,
        };
    }

    if (response.status_code != 204 &&
        (response.status_code < 200 || response.status_code >= 300)) {
        return {
            .success = false,
            .message = parse_error_message(response.body, "Failed to delete account."),
            .session_data = std::nullopt,
        };
    }

    clear_saved_session();
    clear_trusted_device_file();
    return {
        .success = true,
        .message = "Account deleted.",
        .session_data = std::nullopt,
    };
}

my_uploads_result fetch_my_community_uploads() {
    my_uploads_result result;
    std::optional<session> stored = load_saved_session();
    if (!stored.has_value()) {
        result.message = "Login required.";
        return result;
    }

    auto fetch_page = [&](const session& session_data, std::string_view path) {
        return send_authenticated_request(session_data, "GET", path);
    };

    auto refresh_session = [&]() -> bool {
        const operation_result restored = restore_saved_session();
        if (!restored.success || !restored.session_data.has_value()) {
            result.message = restored.message.empty() ? "Saved session expired." : restored.message;
            return false;
        }
        stored = restored.session_data;
        return true;
    };

    constexpr int kPageSize = 50;
    for (int page = 1; page <= 20; ++page) {
        const std::string path =
            "/songs?includeMine=true&contentSource=community&page=" + std::to_string(page) +
            "&pageSize=" + std::to_string(kPageSize);
        http_response response = fetch_page(*stored, path);
        if (response.error_message.empty() && response.status_code == 401 && refresh_session()) {
            response = fetch_page(*stored, path);
        }
        if (!response.error_message.empty()) {
            result.message = response.error_message;
            return result;
        }
        if (response.status_code < 200 || response.status_code >= 300) {
            result.message = parse_error_message(response.body, "Failed to load uploaded songs.");
            return result;
        }

        const std::optional<std::string> items = json::extract_array(response.body, "items");
        if (!items.has_value()) {
            result.message = "Server returned an unexpected songs response.";
            return result;
        }
        const std::vector<std::string> objects = json::extract_objects_from_array(*items);
        for (const std::string& object : objects) {
            if (const auto song = parse_community_song_upload(object, stored->user.id); song.has_value()) {
                result.songs.push_back(*song);
            }
        }
        if (static_cast<int>(objects.size()) < kPageSize) {
            break;
        }
    }

    for (int page = 1; page <= 20; ++page) {
        const std::string path =
            "/charts?includeMine=true&contentSource=community&page=" + std::to_string(page) +
            "&pageSize=" + std::to_string(kPageSize);
        http_response response = fetch_page(*stored, path);
        if (response.error_message.empty() && response.status_code == 401 && refresh_session()) {
            response = fetch_page(*stored, path);
        }
        if (!response.error_message.empty()) {
            result.message = response.error_message;
            return result;
        }
        if (response.status_code < 200 || response.status_code >= 300) {
            result.message = parse_error_message(response.body, "Failed to load uploaded charts.");
            return result;
        }

        const std::optional<std::string> items = json::extract_array(response.body, "items");
        if (!items.has_value()) {
            result.message = "Server returned an unexpected charts response.";
            return result;
        }
        const std::vector<std::string> objects = json::extract_objects_from_array(*items);
        for (const std::string& object : objects) {
            if (const auto chart = parse_community_chart_upload(object, stored->user.id); chart.has_value()) {
                result.charts.push_back(*chart);
            }
        }
        if (static_cast<int>(objects.size()) < kPageSize) {
            break;
        }
    }

    result.success = true;
    result.message = "Uploaded content loaded.";
    return result;
}

profile_rankings_result fetch_my_profile_rankings() {
    profile_rankings_result result;
    std::optional<session> stored = load_saved_session();
    if (!stored.has_value()) {
        result.message = "Login required.";
        return result;
    }

    auto send_profile_request = [&](const session& session_data) {
        return send_authenticated_request(session_data, "GET", "/me/profile/rankings?limit=20");
    };

    http_response response = send_profile_request(*stored);
    if (response.error_message.empty() && response.status_code == 401) {
        const operation_result restored = restore_saved_session();
        if (restored.success && restored.session_data.has_value()) {
            stored = restored.session_data;
            response = send_profile_request(*stored);
        }
    }

    if (!response.error_message.empty()) {
        result.message = response.error_message;
        return result;
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        result.message = parse_error_message(response.body, "Failed to load profile rankings.");
        return result;
    }

    parse_profile_ranking_array(response.body, "recentRecords", result.recent_records);
    parse_profile_ranking_array(response.body, "firstPlaceRecords", result.first_place_records);
    result.success = true;
    result.message = "Profile rankings loaded.";
    return result;
}

operation_result delete_community_song_upload(const std::string& song_id) {
    std::optional<session> stored = load_saved_session();
    if (!stored.has_value()) {
        return {
            .success = false,
            .message = "Login required.",
            .session_data = std::nullopt,
        };
    }

    auto send_delete = [&](const session& session_data) {
        return send_authenticated_request(session_data, "DELETE", "/songs/" + song_id);
    };

    http_response response = send_delete(*stored);
    if (response.error_message.empty() && response.status_code == 401) {
        const operation_result restored = restore_saved_session();
        if (restored.success && restored.session_data.has_value()) {
            stored = restored.session_data;
            response = send_delete(*stored);
        }
    }
    if (!response.error_message.empty()) {
        return {
            .success = false,
            .message = response.error_message,
            .session_data = std::nullopt,
        };
    }
    if (response.status_code != 204 && (response.status_code < 200 || response.status_code >= 300)) {
        return {
            .success = false,
            .message = parse_error_message(response.body, "Failed to delete uploaded song."),
            .session_data = std::nullopt,
        };
    }
    return {
        .success = true,
        .message = "Uploaded song deleted.",
        .session_data = std::nullopt,
    };
}

operation_result delete_community_chart_upload(const std::string& chart_id) {
    std::optional<session> stored = load_saved_session();
    if (!stored.has_value()) {
        return {
            .success = false,
            .message = "Login required.",
            .session_data = std::nullopt,
        };
    }

    auto send_delete = [&](const session& session_data) {
        return send_authenticated_request(session_data, "DELETE", "/charts/" + chart_id);
    };

    http_response response = send_delete(*stored);
    if (response.error_message.empty() && response.status_code == 401) {
        const operation_result restored = restore_saved_session();
        if (restored.success && restored.session_data.has_value()) {
            stored = restored.session_data;
            response = send_delete(*stored);
        }
    }
    if (!response.error_message.empty()) {
        return {
            .success = false,
            .message = response.error_message,
            .session_data = std::nullopt,
        };
    }
    if (response.status_code != 204 && (response.status_code < 200 || response.status_code >= 300)) {
        return {
            .success = false,
            .message = parse_error_message(response.body, "Failed to delete uploaded chart."),
            .session_data = std::nullopt,
        };
    }
    return {
        .success = true,
        .message = "Uploaded chart deleted.",
        .session_data = std::nullopt,
    };
}

}  // namespace auth
