#include "updater/update_apply.h"

#include <system_error>

namespace {

bool should_skip_copy(const std::filesystem::path& source_path, bool skip_updater_executable) {
    if (!skip_updater_executable) {
        return false;
    }
    return source_path.filename() == "Updater.exe";
}

}  // namespace

namespace updater {

bool copy_directory_contents(const std::filesystem::path& source_root,
                             const std::filesystem::path& destination_root,
                             bool skip_updater_executable) {
    if (!std::filesystem::exists(source_root) || !std::filesystem::is_directory(source_root)) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(destination_root, ec);
    if (ec) {
        return false;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root, ec)) {
        if (ec) {
            return false;
        }

        const std::filesystem::path relative_path = std::filesystem::relative(entry.path(), source_root, ec);
        if (ec || relative_path.empty()) {
            return false;
        }

        const std::filesystem::path destination_path = destination_root / relative_path;
        if (entry.is_directory()) {
            std::filesystem::create_directories(destination_path, ec);
            if (ec) {
                return false;
            }
            continue;
        }

        if (!entry.is_regular_file() || should_skip_copy(entry.path(), skip_updater_executable)) {
            continue;
        }

        std::filesystem::create_directories(destination_path.parent_path(), ec);
        if (ec) {
            return false;
        }

        std::filesystem::copy_file(entry.path(), destination_path, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            return false;
        }
    }

    return true;
}

bool apply_staged_update(const std::filesystem::path& install_root,
                         const std::filesystem::path& staged_root,
                         const std::filesystem::path& backup_root) {
    if (!std::filesystem::exists(staged_root) || !std::filesystem::is_directory(staged_root)) {
        return false;
    }

    std::error_code ec;
    std::filesystem::remove_all(backup_root, ec);
    ec.clear();

    if (!copy_directory_contents(install_root, backup_root)) {
        return false;
    }

    if (copy_directory_contents(staged_root, install_root, true)) {
        return true;
    }

    return copy_directory_contents(backup_root, install_root);
}

}  // namespace updater
