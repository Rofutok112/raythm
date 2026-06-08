#include "chart_level_memory_cache.h"

#include <filesystem>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "chart_difficulty.h"
#include "path_utils.h"

namespace {

struct cache_entry {
    std::optional<std::filesystem::file_time_type> write_time;
    std::string content_signature;
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

std::string cache_key_for(const std::string& chart_path, const std::string& content_signature) {
    return content_signature.empty() ? chart_path : chart_path + "\n" + content_signature;
}

}  // namespace

namespace chart_level_memory_cache {

std::optional<float> find_level(const std::string& chart_path) {
    const std::optional<std::filesystem::file_time_type> write_time = chart_write_time(chart_path);
    if (!write_time.has_value()) {
        return std::nullopt;
    }

    std::lock_guard lock(cache_mutex());
    const auto it = cache().find(cache_key_for(chart_path, {}));
    if (it == cache().end() || !it->second.write_time.has_value() || *it->second.write_time != *write_time) {
        return std::nullopt;
    }
    return it->second.level;
}

std::optional<float> find_level(const std::string& chart_path, const std::string& content_signature) {
    if (content_signature.empty()) {
        return find_level(chart_path);
    }

    std::lock_guard lock(cache_mutex());
    const auto it = cache().find(cache_key_for(chart_path, content_signature));
    if (it == cache().end() || it->second.content_signature != content_signature) {
        return std::nullopt;
    }
    return it->second.level;
}

void remember_level(const std::string& chart_path, float level) {
    if (level <= 0.0f) {
        return;
    }
    const std::optional<std::filesystem::file_time_type> write_time = chart_write_time(chart_path);
    if (!write_time.has_value()) {
        return;
    }

    std::lock_guard lock(cache_mutex());
    cache()[cache_key_for(chart_path, {})] = cache_entry{
        .write_time = *write_time,
        .content_signature = {},
        .level = level,
    };
}

void remember_level(const std::string& chart_path, const std::string& content_signature, float level) {
    if (content_signature.empty()) {
        remember_level(chart_path, level);
        return;
    }
    if (level <= 0.0f) {
        return;
    }

    std::lock_guard lock(cache_mutex());
    cache()[cache_key_for(chart_path, content_signature)] = cache_entry{
        .write_time = std::nullopt,
        .content_signature = content_signature,
        .level = level,
    };
}

float get_or_calculate(const std::string& chart_path, const chart_data& chart) {
    if (const std::optional<float> cached = find_level(chart_path); cached.has_value()) {
        return *cached;
    }
    return calculate_and_store(chart_path, chart);
}

float get_or_calculate(const std::string& chart_path, const std::string& content_signature, const chart_data& chart) {
    if (const std::optional<float> cached = find_level(chart_path, content_signature); cached.has_value()) {
        return *cached;
    }
    return calculate_and_store(chart_path, content_signature, chart);
}

float calculate_and_store(const std::string& chart_path, const chart_data& chart) {
    const float level = chart_difficulty::calculate_level(chart);
    const std::optional<std::filesystem::file_time_type> write_time = chart_write_time(chart_path);
    if (!write_time.has_value()) {
        return level;
    }

    std::lock_guard lock(cache_mutex());
    cache()[cache_key_for(chart_path, {})] = cache_entry{
        .write_time = *write_time,
        .content_signature = {},
        .level = level,
    };
    return level;
}

float calculate_and_store(const std::string& chart_path, const std::string& content_signature, const chart_data& chart) {
    if (content_signature.empty()) {
        return calculate_and_store(chart_path, chart);
    }

    const float level = chart_difficulty::calculate_level(chart);
    std::lock_guard lock(cache_mutex());
    cache()[cache_key_for(chart_path, content_signature)] = cache_entry{
        .write_time = std::nullopt,
        .content_signature = content_signature,
        .level = level,
    };
    return level;
}

void clear() {
    std::lock_guard lock(cache_mutex());
    cache().clear();
}

}  // namespace chart_level_memory_cache
