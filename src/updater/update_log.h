#pragma once

#include <string_view>

namespace updater {

void reset_update_log();
void append_update_log(std::string_view source, std::string_view message);

}  // namespace updater
