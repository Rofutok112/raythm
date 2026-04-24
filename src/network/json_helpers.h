#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace network::json {

std::string trim(std::string_view value);
std::string escape_string(const std::string& value);

std::optional<size_t> find_value_start(const std::string& content, const std::string& key);
std::optional<std::string> extract_string(const std::string& content, const std::string& key);
std::optional<std::string> extract_object(const std::string& content, const std::string& key);
std::optional<bool> extract_bool(const std::string& content, const std::string& key);
std::optional<int> extract_int(const std::string& content, const std::string& key);
std::optional<float> extract_float(const std::string& content, const std::string& key);
std::optional<std::string> extract_array(const std::string& content, const std::string& key);
std::vector<std::string> extract_objects_from_array(const std::string& array_content);

}  // namespace network::json
