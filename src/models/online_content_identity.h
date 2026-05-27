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
    std::optional<bool> can_edit;
    std::string lifecycle_status;
};

struct chart_identity {
    std::string server_url;
    std::string remote_song_id;
    std::string remote_chart_id;
    source content_source = source::community;
    int remote_chart_version = 0;
    std::optional<bool> can_edit;
    std::string lifecycle_status;
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

inline bool is_queueable(const chart_identity& identity) {
    return !identity.server_url.empty() &&
           !identity.remote_song_id.empty() &&
           !identity.remote_chart_id.empty();
}

inline std::optional<bool> explicit_edit_permission(const song_identity& identity) {
    return identity.can_edit;
}

inline std::optional<bool> explicit_edit_permission(const chart_identity& identity) {
    return identity.can_edit;
}

inline bool can_edit_with_owned_fallback(const std::optional<bool>& can_edit, bool owned_upload) {
    return can_edit.value_or(owned_upload);
}

}  // namespace online_content
