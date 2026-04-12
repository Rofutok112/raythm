#pragma once

#include <optional>
#include <string>

namespace auth {

inline constexpr const char* kDefaultServerUrl = "http://192.168.11.22";

struct public_user {
    std::string id;
    std::string email;
    std::string display_name;
    bool email_verified = false;
};

struct session {
    std::string server_url;
    std::string access_token;
    std::string refresh_token;
    public_user user;
};

struct session_summary {
    bool logged_in = false;
    std::string server_url;
    std::string email;
    std::string display_name;
    bool email_verified = false;
};

struct operation_result {
    bool success = false;
    std::string message;
    std::optional<session> session_data;
};

std::string normalize_server_url(const std::string& server_url);

std::optional<session> load_saved_session();
session_summary load_session_summary();
bool save_session(const session& session_data);
void clear_saved_session();

operation_result register_user(const std::string& server_url,
                               const std::string& email,
                               const std::string& display_name,
                               const std::string& password);
operation_result login_user(const std::string& server_url,
                            const std::string& email,
                            const std::string& password);
operation_result restore_saved_session();
operation_result logout_saved_session();

}  // namespace auth
