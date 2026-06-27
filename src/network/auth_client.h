#pragma once

#include <optional>
#include <string>
#include <vector>

#include "network/server_environment.h"

namespace auth {

inline constexpr const char* kProductionServerUrl = server_environment::kProductionServerUrl;
inline constexpr const char* kDevelopmentServerUrl = server_environment::kDevelopmentServerUrl;
inline constexpr const char* kDefaultServerUrl = kProductionServerUrl;

struct external_link {
    std::string label;
    std::string url;
};

struct rating_summary {
    float rating = 0.0f;
    int rank = 0;
    int eligible_play_count = 0;
    int best_play_count = 0;
    std::string ruleset_version;
};

struct public_user {
    std::string id;
    std::string email;
    std::string display_name;
    std::string avatar_url;
    bool email_verified = false;
    rating_summary rating;
    std::vector<external_link> external_links;
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
    std::string avatar_url;
    bool email_verified = false;
    rating_summary rating;
    std::vector<external_link> external_links;
};

struct profile_ranking_record {
    std::string chart_id;
    std::string song_id;
    std::string song_title;
    std::string artist;
    std::string genre;
    std::string difficulty_name;
    std::string chart_author;
    std::string clear_rank;
    std::string recorded_at;
    std::string submitted_at;
    int score = 0;
    int placement = 0;
    int max_combo = 0;
    float accuracy = 0.0f;
    float play_rating = 0.0f;
    float rating_contribution = 0.0f;
    float rating_contribution_percent = 0.0f;
    bool is_full_combo = false;
};

struct public_profile {
    std::string id;
    std::string display_name;
    std::string avatar_url;
    rating_summary rating;
    std::string relationship_status;
    std::string relationship_request_id;
    std::vector<external_link> external_links;
    std::vector<profile_ranking_record> best_rating_records;
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
    bool maintenance = false;
    std::string retry_after;
    bool verification_required = false;
    verification_purpose verification = verification_purpose::none;
    std::string verification_email;
};

struct community_song_upload {
    std::string id;
    std::string client_song_id;
    std::string title;
    std::string artist;
    std::string genre;
    std::string content_source;
    std::string visibility;
    std::string lifecycle_status;
    std::string review_status;
    bool can_edit = false;
    bool has_can_edit = false;
    float base_bpm = 0.0f;
    float duration_seconds = 0.0f;
    int preview_start_ms = 0;
    int song_version = 0;
};

struct community_chart_upload {
    std::string id;
    std::string client_chart_id;
    std::string song_id;
    std::string client_song_id;
    std::string song_title;
    std::string difficulty_name;
    std::string chart_author;
    std::string content_source;
    std::string visibility;
    std::string lifecycle_status;
    std::string review_status;
    bool can_edit = false;
    bool has_can_edit = false;
    int key_count = 0;
    float level = 0.0f;
    int note_count = 0;
    float min_bpm = 0.0f;
    float max_bpm = 0.0f;
    std::string difficulty_ruleset_id;
    int difficulty_ruleset_version = 0;
};

struct my_uploads_result {
    bool success = false;
    bool maintenance = false;
    std::string message;
    std::string retry_after;
    std::vector<community_song_upload> songs;
    std::vector<community_chart_upload> charts;
};

struct profile_rankings_result {
    bool success = false;
    bool maintenance = false;
    std::string message;
    std::string retry_after;
    std::vector<profile_ranking_record> best_rating_records;
    std::vector<profile_ranking_record> recent_records;
    std::vector<profile_ranking_record> first_place_records;
};

struct public_profile_result {
    bool success = false;
    bool unauthorized = false;
    bool maintenance = false;
    std::string message;
    std::string retry_after;
    std::optional<public_profile> profile;
};

struct global_rating_ranking_entry {
    std::string user_id;
    std::string display_name;
    std::string avatar_url;
    rating_summary rating;
};

struct global_rating_rankings_result {
    bool success = false;
    bool unauthorized = false;
    bool maintenance = false;
    std::string message;
    std::string retry_after;
    int page = 1;
    int page_size = 50;
    int total = 0;
    bool has_next_page = false;
    std::vector<global_rating_ranking_entry> items;
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
operation_result update_profile_external_links(const std::vector<external_link>& links);
operation_result update_profile_avatar(const std::string& image_path);
operation_result delete_profile_avatar();
my_uploads_result fetch_my_community_uploads();
profile_rankings_result fetch_my_profile_rankings();
public_profile_result fetch_public_profile(const std::string& user_id);
global_rating_rankings_result fetch_global_rating_rankings(int page = 1, int page_size = 50);
operation_result delete_community_song_upload(const std::string& song_id);
operation_result delete_community_chart_upload(const std::string& chart_id);

}  // namespace auth
