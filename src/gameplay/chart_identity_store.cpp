#include "chart_identity_store.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "app_paths.h"

namespace chart_identity {
namespace {

constexpr char kHeader[] = "# raythm chart identity index v1";

struct entry {
    std::string chart_id;
    std::string song_id;
};

std::vector<std::string> split_tab_fields(const std::string& line) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t end = line.find('\t', start);
        if (end == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    return fields;
}

std::vector<entry> load_entries() {
    std::ifstream input(app_paths::chart_identity_index_path(), std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    std::vector<entry> entries;
    std::string line;
    while (std::getline(input, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const std::vector<std::string> fields = split_tab_fields(line);
        if (fields.size() >= 2 && !fields[0].empty() && !fields[1].empty()) {
            entries.push_back({
                .chart_id = fields[0],
                .song_id = fields[1],
            });
        }
    }
    return entries;
}

void save_entries(const std::vector<entry>& entries) {
    app_paths::ensure_directories();
    std::ofstream output(app_paths::chart_identity_index_path(), std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return;
    }

    output << kHeader << '\n';
    for (const entry& item : entries) {
        output << item.chart_id << '\t' << item.song_id << '\n';
    }
}

}  // namespace

std::optional<std::string> find_song_id(const std::string& chart_id) {
    if (chart_id.empty()) {
        return std::nullopt;
    }

    for (const entry& item : load_entries()) {
        if (item.chart_id == chart_id) {
            return item.song_id;
        }
    }
    return std::nullopt;
}

void put(const std::string& chart_id, const std::string& song_id) {
    if (chart_id.empty() || song_id.empty()) {
        return;
    }

    std::vector<entry> entries = load_entries();
    for (entry& item : entries) {
        if (item.chart_id == chart_id) {
            item.song_id = song_id;
            save_entries(entries);
            return;
        }
    }

    entries.push_back({
        .chart_id = chart_id,
        .song_id = song_id,
    });
    save_entries(entries);
}

void remove(const std::string& chart_id) {
    if (chart_id.empty()) {
        return;
    }

    std::vector<entry> entries = load_entries();
    std::erase_if(entries, [&](const entry& item) {
        return item.chart_id == chart_id;
    });
    save_entries(entries);
}

void remove_for_song(const std::string& song_id) {
    if (song_id.empty()) {
        return;
    }

    std::vector<entry> entries = load_entries();
    std::erase_if(entries, [&](const entry& item) {
        return item.song_id == song_id;
    });
    save_entries(entries);
}

}  // namespace chart_identity
