#include "title/upload_mapping_store.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "app_paths.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dpapi.h>
#include <wincrypt.h>
#endif

namespace title_upload_mapping {
namespace {

constexpr char kHeader[] = "# raythm upload mappings v2";

std::filesystem::path secure_mapping_path() {
    std::filesystem::path path = app_paths::upload_mapping_path();
    path.replace_extension(".bin");
    return path;
}

std::string strip_trailing_cr(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) {
        value.pop_back();
    }
    return value;
}

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

std::string origin_to_string(mapping_origin origin) {
    switch (origin) {
    case mapping_origin::owned_upload:
        return "owned_upload";
    case mapping_origin::downloaded:
        return "downloaded";
    case mapping_origin::linked:
        return "linked";
    }
    return "owned_upload";
}

mapping_origin parse_origin(const std::string& value) {
    if (value == "downloaded") {
        return mapping_origin::downloaded;
    }
    if (value == "linked") {
        return mapping_origin::linked;
    }
    return mapping_origin::owned_upload;
}

std::string serialize_plaintext(const store& mappings) {
    std::ostringstream output;
    output << kHeader << '\n';
    output << "[songs]\n";
    for (const song_mapping_entry& entry : mappings.songs) {
        output << entry.server_url << '\t'
               << entry.local_song_id << '\t'
               << entry.remote_song_id << '\t'
               << origin_to_string(entry.origin) << '\n';
    }
    output << "[charts]\n";
    for (const chart_mapping_entry& entry : mappings.charts) {
        output << entry.server_url << '\t'
               << entry.local_chart_id << '\t'
               << entry.local_song_id << '\t'
               << entry.remote_chart_id << '\t'
               << entry.remote_song_id << '\t'
               << origin_to_string(entry.origin) << '\n';
    }
    return output.str();
}

store parse_plaintext(const std::string& content) {
    store mappings;

    std::istringstream input(content);

    enum class section {
        none,
        songs,
        charts,
    };

    section current_section = section::none;
    std::string line;
    while (std::getline(input, line)) {
        line = strip_trailing_cr(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line == "[songs]") {
            current_section = section::songs;
            continue;
        }
        if (line == "[charts]") {
            current_section = section::charts;
            continue;
        }

        const std::vector<std::string> fields = split_tab_fields(line);
        if (current_section == section::songs && fields.size() >= 3) {
            mappings.songs.push_back(song_mapping_entry{
                .server_url = fields[0],
                .local_song_id = fields[1],
                .remote_song_id = fields[2],
                .origin = fields.size() >= 4 ? parse_origin(fields[3]) : mapping_origin::owned_upload,
            });
        } else if (current_section == section::charts && fields.size() >= 5) {
            mappings.charts.push_back(chart_mapping_entry{
                .server_url = fields[0],
                .local_chart_id = fields[1],
                .local_song_id = fields[2],
                .remote_chart_id = fields[3],
                .remote_song_id = fields[4],
                .origin = fields.size() >= 6 ? parse_origin(fields[5]) : mapping_origin::owned_upload,
            });
        }
    }

    return mappings;
}

std::optional<std::string> read_file_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        return std::nullopt;
    }
    return buffer.str();
}

bool write_file_bytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    return output.good();
}

#ifdef _WIN32
std::optional<std::vector<unsigned char>> protect_bytes(const std::string& plaintext) {
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));
    input.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB output{};
    if (CryptProtectData(&input, L"raythm upload mappings", nullptr, nullptr, nullptr, 0, &output) == 0) {
        return std::nullopt;
    }

    std::vector<unsigned char> encrypted(output.pbData, output.pbData + output.cbData);
    LocalFree(output.pbData);
    return encrypted;
}

std::optional<std::string> unprotect_bytes(const std::string& encrypted) {
    if (encrypted.empty()) {
        return std::nullopt;
    }

    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data()));
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output{};
    if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output) == 0) {
        return std::nullopt;
    }

    std::string plaintext(reinterpret_cast<char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return plaintext;
}
#else
std::optional<std::vector<unsigned char>> protect_bytes(const std::string& plaintext) {
    return std::vector<unsigned char>(plaintext.begin(), plaintext.end());
}

std::optional<std::string> unprotect_bytes(const std::string& encrypted) {
    return encrypted;
}
#endif

}  // namespace

store load() {
    if (const std::optional<std::string> encrypted = read_file_text(secure_mapping_path());
        encrypted.has_value()) {
        if (const std::optional<std::string> plaintext = unprotect_bytes(*encrypted);
            plaintext.has_value()) {
            return parse_plaintext(*plaintext);
        }
    }

    if (const std::optional<std::string> legacy = read_file_text(app_paths::upload_mapping_path());
        legacy.has_value()) {
        return parse_plaintext(*legacy);
    }

    return {};
}

bool save(const store& mappings) {
    app_paths::ensure_directories();

    const std::string plaintext = serialize_plaintext(mappings);
    const std::optional<std::vector<unsigned char>> encrypted = protect_bytes(plaintext);
    if (!encrypted.has_value()) {
        return false;
    }

    if (!write_file_bytes(secure_mapping_path(), *encrypted)) {
        return false;
    }

    std::error_code ec;
    std::filesystem::remove(app_paths::upload_mapping_path(), ec);
    return true;
}

std::optional<std::string> find_remote_song_id(const store& mappings,
                                               const std::string& server_url,
                                               const std::string& local_song_id) {
    for (const song_mapping_entry& entry : mappings.songs) {
        if (entry.server_url == server_url && entry.local_song_id == local_song_id) {
            return entry.remote_song_id;
        }
    }
    return std::nullopt;
}

std::optional<mapping_origin> find_song_origin(const store& mappings,
                                               const std::string& server_url,
                                               const std::string& local_song_id) {
    for (const song_mapping_entry& entry : mappings.songs) {
        if (entry.server_url == server_url && entry.local_song_id == local_song_id) {
            return entry.origin;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_local_song_id(const store& mappings,
                                              const std::string& server_url,
                                              const std::string& remote_song_id) {
    for (const song_mapping_entry& entry : mappings.songs) {
        if (entry.server_url == server_url && entry.remote_song_id == remote_song_id) {
            return entry.local_song_id;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_remote_chart_id(const store& mappings,
                                                const std::string& server_url,
                                                const std::string& local_chart_id) {
    for (const chart_mapping_entry& entry : mappings.charts) {
        if (entry.server_url == server_url && entry.local_chart_id == local_chart_id) {
            return entry.remote_chart_id;
        }
    }
    return std::nullopt;
}

std::optional<mapping_origin> find_chart_origin(const store& mappings,
                                                const std::string& server_url,
                                                const std::string& local_chart_id) {
    for (const chart_mapping_entry& entry : mappings.charts) {
        if (entry.server_url == server_url && entry.local_chart_id == local_chart_id) {
            return entry.origin;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_local_chart_id(const store& mappings,
                                               const std::string& server_url,
                                               const std::string& remote_chart_id) {
    for (const chart_mapping_entry& entry : mappings.charts) {
        if (entry.server_url == server_url && entry.remote_chart_id == remote_chart_id) {
            return entry.local_chart_id;
        }
    }
    return std::nullopt;
}

void remove_chart(store& mappings,
                  const std::string& server_url,
                  const std::string& local_chart_id) {
    std::erase_if(mappings.charts, [&](const chart_mapping_entry& entry) {
        return entry.server_url == server_url && entry.local_chart_id == local_chart_id;
    });
}

void remove_song(store& mappings,
                 const std::string& server_url,
                 const std::string& local_song_id) {
    std::erase_if(mappings.songs, [&](const song_mapping_entry& entry) {
        return entry.server_url == server_url && entry.local_song_id == local_song_id;
    });
    std::erase_if(mappings.charts, [&](const chart_mapping_entry& entry) {
        return entry.server_url == server_url && entry.local_song_id == local_song_id;
    });
}

void put_song(store& mappings,
              const std::string& server_url,
              const std::string& local_song_id,
              const std::string& remote_song_id,
              mapping_origin origin) {
    if (local_song_id.empty() || remote_song_id.empty()) {
        return;
    }

    bool found = false;
    for (song_mapping_entry& entry : mappings.songs) {
        if (entry.server_url == server_url && entry.local_song_id == local_song_id) {
            found = true;
            if (entry.remote_song_id != remote_song_id) {
                entry.remote_song_id = remote_song_id;
                std::erase_if(mappings.charts, [&](const chart_mapping_entry& chart_entry) {
                    return chart_entry.server_url == server_url &&
                           chart_entry.local_song_id == local_song_id;
                });
            }
            entry.origin = origin;
            break;
        }
    }

    if (!found) {
        mappings.songs.push_back(song_mapping_entry{
            .server_url = server_url,
            .local_song_id = local_song_id,
            .remote_song_id = remote_song_id,
            .origin = origin,
        });
    }
}

void put_chart(store& mappings,
               const std::string& server_url,
               const std::string& local_chart_id,
               const std::string& local_song_id,
               const std::string& remote_chart_id,
               const std::string& remote_song_id,
               mapping_origin origin) {
    if (local_chart_id.empty() || remote_chart_id.empty()) {
        return;
    }

    for (chart_mapping_entry& entry : mappings.charts) {
        if (entry.server_url == server_url && entry.local_chart_id == local_chart_id) {
            entry.local_song_id = local_song_id;
            entry.remote_chart_id = remote_chart_id;
            entry.remote_song_id = remote_song_id;
            entry.origin = origin;
            return;
        }
    }

    mappings.charts.push_back(chart_mapping_entry{
        .server_url = server_url,
        .local_chart_id = local_chart_id,
        .local_song_id = local_song_id,
        .remote_chart_id = remote_chart_id,
        .remote_song_id = remote_song_id,
        .origin = origin,
    });
}

}  // namespace title_upload_mapping
