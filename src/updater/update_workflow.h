#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "updater/update_release.h"
#include "updater/update_version.h"

namespace updater {

struct update_launch_request {
    semantic_version current_version;
    latest_release_info target_release;
    std::optional<std::filesystem::path> install_root;
    bool run_from_temp_copy = false;
};

std::vector<std::string> build_updater_arguments(const update_launch_request& request);
std::optional<update_launch_request> parse_updater_arguments(int argc, char* argv[]);

}  // namespace updater
