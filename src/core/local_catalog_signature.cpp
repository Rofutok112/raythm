#include "local_catalog_signature.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#include "app_paths.h"

namespace local_catalog_signature {
namespace {
namespace fs = std::filesystem;

std::string path_key(const fs::path& root, const fs::path& path) {
    std::error_code ec;
    const fs::path relative = fs::relative(path, root, ec);
    return ec ? path.string() : relative.generic_string();
}

bool path_is_regular_file(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

bool path_is_directory(const fs::path& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

void add_file_if_present(std::vector<fs::path>& files, const fs::path& path) {
    if (path_is_regular_file(path)) {
        files.push_back(path);
    }
}

bool is_chart_file(const fs::path& path) {
    return path.extension() == ".rchart";
}

bool is_encrypted_chart_file(const fs::path& path) {
    return path.extension() == ".renc" && path.stem().extension() == ".rchart";
}

void add_chart_files(std::vector<fs::path>& files, const fs::path& charts_dir) {
    if (!path_is_directory(charts_dir)) {
        return;
    }

    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(charts_dir, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file(ec) && is_chart_file(entry.path())) {
            files.push_back(entry.path());
        }
    }
}

void add_encrypted_chart_files(std::vector<fs::path>& files, const fs::path& encrypted_charts_dir) {
    if (!path_is_directory(encrypted_charts_dir)) {
        return;
    }

    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(encrypted_charts_dir, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file(ec) && is_encrypted_chart_file(entry.path())) {
            files.push_back(entry.path());
        }
    }
}

void add_managed_catalog_assets(std::vector<fs::path>& files, const fs::path& song_dir) {
    add_file_if_present(files, song_dir / ".encrypted" / "song.json.renc");
    add_encrypted_chart_files(files, song_dir / ".encrypted" / "charts");
}

void add_song_directory_files(std::vector<fs::path>& files, const fs::path& song_dir) {
    add_file_if_present(files, song_dir / "song.json");
    add_file_if_present(files, song_dir / "managed-package.json");
    add_chart_files(files, song_dir / "charts");
    add_managed_catalog_assets(files, song_dir);
}

void append_files_signature(std::ostringstream& output,
                            const fs::path& root,
                            const char* label,
                            std::vector<fs::path> files) {
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        output << label << ":missing\n";
        return;
    }

    std::sort(files.begin(), files.end());

    output << label << ":";
    for (const fs::path& file : files) {
        const auto size = fs::file_size(file, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        const auto write_time = fs::last_write_time(file, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        output << path_key(root, file) << "," << size << "," << write_time.time_since_epoch().count() << ";";
    }
    output << "\n";
}

void append_workspace_signature(std::ostringstream& output, const fs::path& root, const char* label) {
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        output << label << ":missing\n";
        return;
    }

    std::vector<fs::path> files;
    for (const fs::directory_entry& entry : fs::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_directory(ec)) {
            add_song_directory_files(files, entry.path());
        }
    }
    append_files_signature(output, root, label, std::move(files));
}

void append_content_cache_signature(std::ostringstream& output, const fs::path& root, const char* label) {
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        output << label << ":missing\n";
        return;
    }

    std::vector<fs::path> files;
    for (const fs::path& source_root : {
             app_paths::community_content_cache_root(),
             app_paths::official_content_cache_root(),
        }) {
        const fs::path songs_root = source_root / "songs";
        if (!path_is_directory(songs_root)) {
            continue;
        }
        for (const fs::directory_entry& entry : fs::directory_iterator(songs_root, ec)) {
            if (ec) {
                break;
            }
            if (entry.is_directory(ec)) {
                add_song_directory_files(files, entry.path());
            }
        }
        ec.clear();
    }
    append_files_signature(output, root, label, std::move(files));
}

}  // namespace

std::string current() {
    std::ostringstream output;
    append_workspace_signature(output, app_paths::songs_root(), "songs");
    append_content_cache_signature(output, app_paths::content_cache_root(), "content-cache");
    return output.str();
}

}  // namespace local_catalog_signature
