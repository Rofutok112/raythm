#include "official_content_sync.h"

#include <filesystem>
#include <system_error>

#include "app_paths.h"

namespace {
namespace fs = std::filesystem;

void mirror_directory(const fs::path& source_root, const fs::path& dest_root) {
    std::error_code ec;
    fs::create_directories(dest_root, ec);
    if (!fs::exists(source_root) || !fs::is_directory(source_root)) {
        return;
    }

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(source_root)) {
        const fs::path relative = fs::relative(entry.path(), source_root, ec);
        if (ec) {
            ec.clear();
            continue;
        }

        const fs::path dest = dest_root / relative;
        if (entry.is_directory()) {
            fs::create_directories(dest, ec);
            ec.clear();
            continue;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        fs::create_directories(dest.parent_path(), ec);
        ec.clear();
        fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing, ec);
        ec.clear();
    }
}

}  // namespace

namespace official_content_sync {

void synchronize() {
    app_paths::ensure_directories();
    std::error_code ec;
    fs::remove_all(app_paths::official_root(), ec);
    ec.clear();
    fs::create_directories(app_paths::official_songs_root(), ec);
    ec.clear();
    fs::create_directories(app_paths::official_charts_root(), ec);
    ec.clear();
    mirror_directory(app_paths::legacy_songs_root(), app_paths::official_songs_root());
    mirror_directory(app_paths::assets_root() / "charts", app_paths::official_charts_root());
}

}  // namespace official_content_sync
