#include "updater/update_log.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "updater/update_paths.h"

namespace {

std::filesystem::path log_file_path() {
    return updater::update_root() / "update.log";
}

std::string current_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &now_time);
#else
    localtime_r(&now_time, &local_time);
#endif

    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

}  // namespace

namespace updater {

void reset_update_log() {
    ensure_update_directories();
    std::ofstream output(log_file_path(), std::ios::trunc);
    if (!output.is_open()) {
        return;
    }
    output << current_timestamp() << " [log] reset\n";
}

void append_update_log(std::string_view source, std::string_view message) {
    ensure_update_directories();
    std::ofstream output(log_file_path(), std::ios::app);
    if (!output.is_open()) {
        return;
    }
    output << current_timestamp() << " [" << source << "] " << message << '\n';
}

}  // namespace updater
