#include "network/json_helpers.h"

#include <cctype>

namespace network::json {
namespace {

std::optional<size_t> find_key_end(const std::string& content, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const size_t key_pos = content.find(token);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    return key_pos + token.size();
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
    std::string result;
    result.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += ch; break;
        }
    }
    return result;
}

std::optional<size_t> find_value_start(const std::string& content, const std::string& key) {
    const auto key_end = find_key_end(content, key);
    if (!key_end.has_value()) {
        return std::nullopt;
    }

    const size_t colon_pos = content.find(':', *key_end);
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    size_t start = colon_pos + 1;
    while (start < content.size() && std::isspace(static_cast<unsigned char>(content[start]))) {
        ++start;
    }

    if (start >= content.size()) {
        return std::nullopt;
    }

    return start;
}

std::optional<std::string> extract_string(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value() || content[*start_opt] != '"') {
        return std::nullopt;
    }

    std::string result;
    bool escaping = false;
    for (size_t index = *start_opt + 1; index < content.size(); ++index) {
        const char ch = content[index];
        if (escaping) {
            switch (ch) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: result += ch; break;
            }
            escaping = false;
            continue;
        }

        if (ch == '\\') {
            escaping = true;
            continue;
        }

        if (ch == '"') {
            return result;
        }

        result += ch;
    }

    return std::nullopt;
}

std::optional<std::string> extract_object(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value() || content[*start_opt] != '{') {
        return std::nullopt;
    }

    size_t depth = 0;
    bool in_string = false;
    bool escaping = false;
    for (size_t index = *start_opt; index < content.size(); ++index) {
        const char ch = content[index];
        if (in_string) {
            if (escaping) {
                escaping = false;
                continue;
            }
            if (ch == '\\') {
                escaping = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }

        if (ch == '"') {
            in_string = true;
            continue;
        }

        if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                return content.substr(*start_opt, index - *start_opt + 1);
            }
        }
    }

    return std::nullopt;
}

std::optional<bool> extract_bool(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value()) {
        return std::nullopt;
    }

    if (content.compare(*start_opt, 4, "true") == 0) {
        return true;
    }

    if (content.compare(*start_opt, 5, "false") == 0) {
        return false;
    }

    return std::nullopt;
}

std::optional<int> extract_int(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value()) {
        return std::nullopt;
    }

    size_t end = *start_opt;
    if (content[end] == '-') {
        ++end;
    }
    while (end < content.size() && std::isdigit(static_cast<unsigned char>(content[end]))) {
        ++end;
    }
    if (end == *start_opt) {
        return std::nullopt;
    }

    try {
        return std::stoi(content.substr(*start_opt, end - *start_opt));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<float> extract_float(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value()) {
        return std::nullopt;
    }

    size_t end = *start_opt;
    if (content[end] == '-') {
        ++end;
    }
    while (end < content.size()) {
        const char ch = content[end];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '.')) {
            break;
        }
        ++end;
    }
    if (end == *start_opt) {
        return std::nullopt;
    }

    try {
        return std::stof(content.substr(*start_opt, end - *start_opt));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string> extract_array(const std::string& content, const std::string& key) {
    const auto start_opt = find_value_start(content, key);
    if (!start_opt.has_value() || content[*start_opt] != '[') {
        return std::nullopt;
    }

    size_t depth = 0;
    bool in_string = false;
    bool escaping = false;
    for (size_t index = *start_opt; index < content.size(); ++index) {
        const char ch = content[index];
        if (in_string) {
            if (escaping) {
                escaping = false;
                continue;
            }
            if (ch == '\\') {
                escaping = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }

        if (ch == '"') {
            in_string = true;
            continue;
        }

        if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return content.substr(*start_opt, index - *start_opt + 1);
            }
        }
    }

    return std::nullopt;
}

std::vector<std::string> extract_objects_from_array(const std::string& array_content) {
    std::vector<std::string> objects;
    size_t index = 0;
    while (index < array_content.size()) {
        index = array_content.find('{', index);
        if (index == std::string::npos) {
            break;
        }

        const size_t start = index;
        size_t depth = 0;
        bool in_string = false;
        bool escaping = false;
        for (; index < array_content.size(); ++index) {
            const char ch = array_content[index];
            if (in_string) {
                if (escaping) {
                    escaping = false;
                    continue;
                }
                if (ch == '\\') {
                    escaping = true;
                } else if (ch == '"') {
                    in_string = false;
                }
                continue;
            }

            if (ch == '"') {
                in_string = true;
                continue;
            }

            if (ch == '{') {
                ++depth;
            } else if (ch == '}') {
                --depth;
                if (depth == 0) {
                    objects.push_back(array_content.substr(start, index - start + 1));
                    ++index;
                    break;
                }
            }
        }
    }
    return objects;
}

}  // namespace network::json
