#pragma once

#include <string>
#include <unordered_map>

#include "song_select/song_select_state.h"

namespace song_select::content_verification_cache_database {

struct content_hashes {
    std::string song_json_sha256;
    std::string song_json_fingerprint;
    std::string audio_sha256;
    std::string jacket_sha256;
    std::string chart_sha256;
    std::string chart_fingerprint;
};

struct record {
    std::string server_url;
    std::string chart_id;
    std::string song_id;
    content_status status = content_status::local;
    std::string content_source;
    std::string file_signature;
    content_hashes local_hashes;
    content_hashes server_hashes;
};

using cache = std::unordered_map<std::string, record>;

cache load();
void save(const cache& records);

}  // namespace song_select::content_verification_cache_database
