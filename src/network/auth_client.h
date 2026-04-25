#pragma once

#include <optional>
#include <string>

namespace auth {

inline constexpr const char* kDefaultServerUrl = "http://192.168.11.33";

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

enum class verification_purpose {
    none,
    email_verification,
    login_verification,
};

struct operation_result {
    bool success = false;
    std::string message;
    std::optional<session> session_data;
    bool verification_required = false;
    verification_purpose verification = verification_purpose::none;
    std::string verification_email;
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
operation_result verify_email_code(const std::string& server_url,
                                   const std::string& email,
                                   const std::string& code);
operation_result verify_login_code(const std::string& server_url,
                                   const std::string& email,
                                   const std::string& code);
operation_result resend_verification_code(const std::string& server_url,
                                          const std::string& email,
                                          verification_purpose purpose);
operation_result restore_saved_session();
operation_result logout_saved_session();

}  // namespace auth
