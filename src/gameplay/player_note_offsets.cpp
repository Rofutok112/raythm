#include "player_note_offsets.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app_paths.h"

namespace {

constexpr int kMinOffsetMs = -1000;
constexpr int kMaxOffsetMs = 1000;

std::string trim(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

}  // namespace

player_song_offset_map load_player_song_offsets() {
    player_song_offset_map offsets;

    std::ifstream input(app_paths::song_offsets_path());
    if (!input.is_open()) {
        return offsets;
    }

    std::string line;
    while (std::getline(input, line)) {
        const size_t delimiter = line.find('\t');
        if (delimiter == std::string::npos) {
            continue;
        }

        const std::string song_id = trim(std::string_view(line).substr(0, delimiter));
        const std::string value = trim(std::string_view(line).substr(delimiter + 1));
        if (song_id.empty() || value.empty()) {
            continue;
        }

        try {
            const int offset_ms = std::clamp(std::stoi(value), kMinOffsetMs, kMaxOffsetMs);
            if (offset_ms != 0) {
                offsets[song_id] = offset_ms;
            }
        } catch (...) {
            // Ignore malformed rows and keep the rest.
        }
    }

    return offsets;
}

int load_player_song_offset(const std::string& song_id) {
    if (song_id.empty()) {
        return 0;
    }

    const player_song_offset_map offsets = load_player_song_offsets();
    const auto it = offsets.find(song_id);
    return it == offsets.end() ? 0 : it->second;
}

bool save_player_song_offset(const std::string& song_id, int offset_ms) {
    if (song_id.empty()) {
        return false;
    }

    player_song_offset_map offsets = load_player_song_offsets();
    const int clamped_offset = std::clamp(offset_ms, kMinOffsetMs, kMaxOffsetMs);
    if (clamped_offset == 0) {
        offsets.erase(song_id);
    } else {
        offsets[song_id] = clamped_offset;
    }

    std::vector<std::pair<std::string, int>> ordered(offsets.begin(), offsets.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });

    app_paths::ensure_directories();
    std::ofstream output(app_paths::song_offsets_path(), std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    for (const auto& [id, value] : ordered) {
        output << id << '\t' << value << '\n';
    }
    return true;
}
