#pragma once

#include <functional>
#include <string>
#include <vector>

namespace title_online_view {

enum class catalog_mode;

struct remote_song_payload {
    std::string id;
    std::string title;
    std::string artist;
    float base_bpm = 0.0f;
    float duration_seconds = 0.0f;
    int preview_start_ms = 0;
    int song_version = 0;
    std::string content_source;
    std::string audio_url;
    std::string jacket_url;
};

struct remote_chart_payload {
    std::string id;
    std::string song_id;
    int key_count = 0;
    std::string difficulty_name;
    int chart_version = 0;
    float level = 0.0f;
    std::string chart_author;
    int format_version = 0;
    int resolution = 0;
    int offset = 0;
    int note_count = 0;
    float min_bpm = 0.0f;
    float max_bpm = 0.0f;
    std::string difficulty_ruleset_id;
    int difficulty_ruleset_version = 0;
    std::string chart_fingerprint;
    std::string chart_sha256;
    std::string content_source;
};

struct remote_catalog_fetch_result {
    std::vector<remote_song_payload> songs;
    std::vector<remote_chart_payload> charts;
    std::string server_url;
    bool success = false;
    std::string error_message;
};

struct remote_song_page_fetch_result {
    std::vector<remote_song_payload> songs;
    std::string server_url;
    bool success = false;
    std::string error_message;
    int total = 0;
    int page = 1;
    int page_size = 0;
};

struct remote_chart_page_fetch_result {
    std::vector<remote_chart_payload> charts;
    std::string server_url;
    bool success = false;
    std::string error_message;
    int total = 0;
    int page = 1;
    int page_size = 0;
    std::string song_id;
};

struct remote_song_lookup_result {
    remote_song_payload song;
    std::string server_url;
    bool success = false;
    bool not_found = false;
    std::string error_message;
};

struct remote_binary_fetch_result {
    std::vector<unsigned char> bytes;
    std::string content_type;
    bool success = false;
    std::string error_message;
};

using remote_binary_progress_callback = std::function<void(size_t bytes_received, size_t total_bytes)>;

remote_catalog_fetch_result fetch_remote_catalog();
remote_song_page_fetch_result fetch_remote_song_page(
    catalog_mode mode,
    int page,
    int page_size,
    const std::string& preferred_server_url = "");
remote_chart_page_fetch_result fetch_remote_chart_page(
    const std::string& server_url,
    const std::string& song_id,
    int page,
    int page_size);
remote_song_lookup_result fetch_remote_song_by_id(
    const std::string& song_id,
    const std::string& preferred_server_url = "");
remote_binary_fetch_result fetch_remote_binary(
    const std::string& url,
    const remote_binary_progress_callback& progress_callback = {});
std::string make_absolute_remote_url(const std::string& server_url, const std::string& value);

}  // namespace title_online_view
