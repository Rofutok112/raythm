#include "editor/service/editor_chart_identity_service.h"

#include <cctype>

#include "app_paths.h"
#include "path_utils.h"
#include "uuid_util.h"

namespace {
std::string slugify(std::string text) {
    std::string slug;
    slug.reserve(text.size());
    bool previous_dash = false;

    for (unsigned char ch : text) {
        if (std::isalnum(ch) != 0) {
            slug.push_back(static_cast<char>(std::tolower(ch)));
            previous_dash = false;
        } else if (!previous_dash && !slug.empty()) {
            slug.push_back('-');
            previous_dash = true;
        }
    }

    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }

    return slug;
}
}

std::string editor_chart_identity_service::generated_chart_id(const song_data& song, const std::string& difficulty) {
    const bool is_appdata_song = song.directory.find(path_utils::to_utf8(app_paths::app_data_root())) != std::string::npos;
    if (is_appdata_song) {
        return generate_uuid();
    }

    const std::string song_id = slugify(song.meta.song_id);
    const std::string difficulty_slug = slugify(difficulty);
    if (!song_id.empty() && !difficulty_slug.empty()) {
        return song_id + "-" + difficulty_slug;
    }
    if (!song_id.empty()) {
        return song_id + "-chart";
    }
    if (!difficulty_slug.empty()) {
        return "chart-" + difficulty_slug;
    }
    return "new-chart";
}
