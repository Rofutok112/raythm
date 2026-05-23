#pragma once

#include <optional>
#include <string>

#include "data_models.h"

namespace online_content {

enum class source {
    official,
    community,
};

struct song_identity {
    std::string server_url;
    std::string remote_song_id;
    source content_source = source::community;
};

struct chart_identity {
    std::string server_url;
    std::string remote_song_id;
    std::string remote_chart_id;
    source content_source = source::community;
    int remote_chart_version = 0;
};

inline std::optional<source> source_from_status(content_status status) {
    switch (status) {
    case content_status::official:
        return source::official;
    case content_status::community:
        return source::community;
    case content_status::local:
    case content_status::update:
    case content_status::modified:
    case content_status::checking:
        return std::nullopt;
    }
    return std::nullopt;
}

inline std::optional<source> source_from_string(const std::string& value) {
    if (value == "official") {
        return source::official;
    }
    if (value == "community") {
        return source::community;
    }
    return std::nullopt;
}

inline content_status status_from_source(source value) {
    return value == source::official ? content_status::official : content_status::community;
}

inline const char* source_label(source value) {
    return value == source::official ? "official" : "community";
}

inline bool is_queueable(const std::optional<chart_identity>& identity) {
    return identity.has_value() &&
           !identity->server_url.empty() &&
           !identity->remote_song_id.empty() &&
           !identity->remote_chart_id.empty();
}

}  // namespace online_content
