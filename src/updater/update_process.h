#pragma once

#include <chrono>
#include <string_view>

namespace updater {

bool process_name_matches(std::string_view process_name, std::string_view expected_name);
bool ensure_process_stopped(std::string_view process_name, std::chrono::milliseconds wait_timeout);

}  // namespace updater
