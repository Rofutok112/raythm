#pragma once

#include <optional>
#include <string>
#include <vector>

namespace title_upload_mapping {

enum class mapping_origin {
    owned_upload,
    downloaded,
    linked,
};

struct song_mapping_entry {
    std::string server_url;
    std::string local_song_id;
    std::string remote_song_id;
    mapping_origin origin = mapping_origin::owned_upload;
};

struct chart_mapping_entry {
    std::string server_url;
    std::string local_chart_id;
    std::string local_song_id;
    std::string remote_chart_id;
    std::string remote_song_id;
    mapping_origin origin = mapping_origin::owned_upload;
};

struct store {
    std::vector<song_mapping_entry> songs;
    std::vector<chart_mapping_entry> charts;
};

store load();
bool save(const store& mappings);

std::optional<std::string> find_remote_song_id(const store& mappings,
                                               const std::string& server_url,
                                               const std::string& local_song_id);
std::optional<mapping_origin> find_song_origin(const store& mappings,
                                               const std::string& server_url,
                                               const std::string& local_song_id);
std::optional<std::string> find_local_song_id(const store& mappings,
                                              const std::string& server_url,
                                              const std::string& remote_song_id);
std::optional<std::string> find_remote_chart_id(const store& mappings,
                                                const std::string& server_url,
                                                const std::string& local_chart_id);
std::optional<mapping_origin> find_chart_origin(const store& mappings,
                                                const std::string& server_url,
                                                const std::string& local_chart_id);
std::optional<std::string> find_local_chart_id(const store& mappings,
                                               const std::string& server_url,
                                               const std::string& remote_chart_id);

void remove_chart(store& mappings,
                  const std::string& server_url,
                  const std::string& local_chart_id);
void remove_song(store& mappings,
                 const std::string& server_url,
                 const std::string& local_song_id);
void put_song(store& mappings,
              const std::string& server_url,
              const std::string& local_song_id,
              const std::string& remote_song_id,
              mapping_origin origin = mapping_origin::owned_upload);
void put_chart(store& mappings,
               const std::string& server_url,
               const std::string& local_chart_id,
               const std::string& local_song_id,
               const std::string& remote_chart_id,
               const std::string& remote_song_id,
               mapping_origin origin = mapping_origin::owned_upload);

}  // namespace title_upload_mapping
