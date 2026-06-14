#include "network/json_helpers.h"

#include <cctype>

#include <nlohmann/json.hpp>

namespace network::json {
namespace {

using json_value = nlohmann::json;

json_value parse_json(const std::string& content) {
    return json_value::parse(content, nullptr, false);
}

const json_value* find_member(const json_value& root, const std::string& key) {
    if (!root.is_object()) {
        return nullptr;
    }
    const auto it = root.find(key);
    if (it == root.end()) {
        return nullptr;
    }
    return &*it;
}

std::optional<json_value> extract_value(const std::string& content, const std::string& key) {
    const json_value root = parse_json(content);
    if (root.is_discarded()) {
        return std::nullopt;
    }
    const json_value* value = find_member(root, key);
    if (value == nullptr) {
        return std::nullopt;
    }
    return *value;
}

}  // namespace

std::string trim(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

std::string escape_string(const std::string& value) {
    const std::string quoted = json_value(value).dump();
    if (quoted.size() < 2) {
        return {};
    }
    return quoted.substr(1, quoted.size() - 2);
}

std::optional<std::string> extract_string(const std::string& content, const std::string& key) {
    const std::optional<json_value> value = extract_value(content, key);
    if (!value.has_value() || !value->is_string()) {
        return std::nullopt;
    }
    try {
        return value->get<std::string>();
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> extract_object(const std::string& content, const std::string& key) {
    const std::optional<json_value> value = extract_value(content, key);
    if (!value.has_value() || !value->is_object()) {
        return std::nullopt;
    }
    return value->dump();
}

std::optional<bool> extract_bool(const std::string& content, const std::string& key) {
    const std::optional<json_value> value = extract_value(content, key);
    if (!value.has_value() || !value->is_boolean()) {
        return std::nullopt;
    }
    try {
        return value->get<bool>();
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> extract_int(const std::string& content, const std::string& key) {
    const std::optional<json_value> value = extract_value(content, key);
    if (!value.has_value() || !(value->is_number_integer() || value->is_number_unsigned())) {
        return std::nullopt;
    }
    try {
        return value->get<int>();
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<float> extract_float(const std::string& content, const std::string& key) {
    const std::optional<json_value> value = extract_value(content, key);
    if (!value.has_value() || !value->is_number()) {
        return std::nullopt;
    }
    try {
        return value->get<float>();
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> extract_array(const std::string& content, const std::string& key) {
    const std::optional<json_value> value = extract_value(content, key);
    if (!value.has_value() || !value->is_array()) {
        return std::nullopt;
    }
    return value->dump();
}

std::vector<std::string> extract_objects_from_array(const std::string& array_content) {
    std::vector<std::string> objects;
    const json_value array = parse_json(array_content);
    if (!array.is_array()) {
        return objects;
    }
    for (const json_value& item : array) {
        if (item.is_object()) {
            objects.push_back(item.dump());
        }
    }
    return objects;
}

std::vector<std::string> extract_strings_from_array(const std::string& array_content) {
    std::vector<std::string> strings;
    const json_value array = parse_json(array_content);
    if (!array.is_array()) {
        return strings;
    }
    for (const json_value& item : array) {
        if (item.is_string()) {
            try {
                strings.push_back(item.get<std::string>());
            } catch (...) {
            }
        }
    }
    return strings;
}

}  // namespace network::json
