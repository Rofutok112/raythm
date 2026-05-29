#include "local_catalog_signature.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <vector>

#include "app_paths.h"

namespace local_catalog_signature {
namespace {

std::string path_key(const std::filesystem::path& root, const std::filesystem::path& path) {
    std::error_code ec;
    const std::filesystem::path relative = std::filesystem::relative(path, root, ec);
    return ec ? path.string() : relative.generic_string();
}

void append_tree_signature(std::ostringstream& output, const std::filesystem::path& root, const char* label) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        output << label << ":missing\n";
        return;
    }

    std::vector<std::filesystem::path> files;
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file(ec)) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    output << label << ":";
    for (const std::filesystem::path& file : files) {
        const auto size = std::filesystem::file_size(file, ec);
        if (ec) {
            continue;
        }
        const auto write_time = std::filesystem::last_write_time(file, ec);
        if (ec) {
            continue;
        }
        output << path_key(root, file) << "," << size << "," << write_time.time_since_epoch().count() << ";";
    }
    output << "\n";
}

}  // namespace

std::string current() {
    std::ostringstream output;
    append_tree_signature(output, app_paths::songs_root(), "songs");
    append_tree_signature(output, app_paths::content_cache_root(), "content-cache");
    return output.str();
}

}  // namespace local_catalog_signature
