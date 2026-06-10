#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "data_models.h"
#include "managed_content_storage.h"
#include "song_select/song_select_state.h"

namespace song_select::local_catalog_cache_service {

using progress_callback = std::function<void(std::string message, float progress)>;

struct refresh_guard {
    std::string signature;
};

std::optional<catalog_data> load_ready_catalog(progress_callback progress = {});
refresh_guard capture_refresh_guard();
void replace_if_unchanged(const refresh_guard& guard, const std::vector<song_entry>& songs);
void sync_managed_manifest_identity(const managed_content_storage::package_manifest& manifest);
std::optional<float> find_chart_level(const std::string& chart_path);
std::optional<float> find_chart_level(const std::string& chart_path, const std::string& content_signature);
float get_or_calculate_chart_level(const std::string& chart_path, const chart_data& chart);
float get_or_calculate_chart_level(const std::string& chart_path,
                                   const std::string& content_signature,
                                   const chart_data& chart);
float calculate_and_store_chart_level(const std::string& chart_path, const chart_data& chart);
float calculate_and_store_chart_level(const std::string& chart_path,
                                      const std::string& content_signature,
                                      const chart_data& chart);

}  // namespace song_select::local_catalog_cache_service
