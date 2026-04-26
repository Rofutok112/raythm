#pragma once

#include <optional>
#include <string>
#include <vector>

namespace auth {

inline constexpr const char* kDefaultServerUrl = "https://api.raythm.net";
inline constexpr const char* kLegacyLanServerUrl = "http://192.168.11.33";

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

struct community_song_upload {
    std::string id;
    std::string title;
    std::string artist;
    std::string visibility;
};

struct community_chart_upload {
    std::string id;
    std::string song_id;
    std::string song_title;
    std::string difficulty_name;
    std::string chart_author;
    std::string visibility;
};

struct my_uploads_result {
    bool success = false;
    std::string message;
    std::vector<community_song_upload> songs;
    std::vector<community_chart_upload> charts;
};

struct profile_ranking_record {
    std::string chart_id;
    std::string song_id;
    std::string song_title;
    std::string artist;
    std::string difficulty_name;
    std::string chart_author;
    std::string clear_rank;
    std::string recorded_at;
    std::string submitted_at;
    int score = 0;
    int placement = 0;
    int max_combo = 0;
    float accuracy = 0.0f;
    bool is_full_combo = false;
};

struct profile_rankings_result {
    bool success = false;
    std::string message;
    std::vector<profile_ranking_record> recent_records;
    std::vector<profile_ranking_record> first_place_records;
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
operation_result delete_saved_account(const std::string& password);
my_uploads_result fetch_my_community_uploads();
profile_rankings_result fetch_my_profile_rankings();
operation_result delete_community_song_upload(const std::string& song_id);
operation_result delete_community_chart_upload(const std::string& chart_id);

}  // namespace auth
