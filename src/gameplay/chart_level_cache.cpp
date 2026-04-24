#include "chart_level_cache.h"

#include <filesystem>
#include <mutex>
#include <unordered_map>

#include "chart_difficulty.h"
#include "path_utils.h"

namespace {

struct cache_entry {
    std::filesystem::file_time_type write_time{};
    float level = 0.0f;
};

std::mutex& cache_mutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<std::string, cache_entry>& cache() {
    static std::unordered_map<std::string, cache_entry> values;
    return values;
}

std::optional<std::filesystem::file_time_type> chart_write_time(const std::string& chart_path) {
    std::error_code ec;
    const auto write_time = std::filesystem::last_write_time(path_utils::from_utf8(chart_path), ec);
    if (ec) {
        return std::nullopt;
    }
    return write_time;
}

}  // namespace

namespace chart_level_cache {

std::optional<float> find_level(const std::string& chart_path) {
    const std::optional<std::filesystem::file_time_type> write_time = chart_write_time(chart_path);
    if (!write_time.has_value()) {
        return std::nullopt;
    }

    std::lock_guard lock(cache_mutex());
    const auto it = cache().find(chart_path);
    if (it == cache().end() || it->second.write_time != *write_time) {
        return std::nullopt;
    }
    return it->second.level;
}

float get_or_calculate(const std::string& chart_path, const chart_data& chart) {
    if (const std::optional<float> cached = find_level(chart_path); cached.has_value()) {
        return *cached;
    }
    return calculate_and_store(chart_path, chart);
}

float calculate_and_store(const std::string& chart_path, const chart_data& chart) {
    const float level = chart_difficulty::calculate_level(chart);
    const std::optional<std::filesystem::file_time_type> write_time = chart_write_time(chart_path);
    if (!write_time.has_value()) {
        return level;
    }

    std::lock_guard lock(cache_mutex());
    cache()[chart_path] = cache_entry{
        .write_time = *write_time,
        .level = level,
    };
    return level;
}

void clear() {
    std::lock_guard lock(cache_mutex());
    cache().clear();
}

}  // namespace chart_level_cache
