#pragma once

#include <filesystem>

namespace updater {

bool copy_directory_contents(const std::filesystem::path& source_root,
                             const std::filesystem::path& destination_root,
                             bool skip_updater_executable = false);
bool apply_staged_update(const std::filesystem::path& install_root,
                         const std::filesystem::path& staged_root,
                         const std::filesystem::path& backup_root);

}  // namespace updater
