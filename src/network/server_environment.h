#pragma once

#include <string>

namespace server_environment {

enum class environment {
    production,
    development,
    custom,
};

inline constexpr const char* kProductionServerUrl = "https://api.raythm.net";
inline constexpr const char* kDevelopmentServerUrl = "https://dev-api.raythm.net";
inline constexpr const char* kDefaultCustomServerUrl = "http://localhost:3000";

inline const char* key(environment value) {
    switch (value) {
    case environment::development:
        return "development";
    case environment::custom:
        return "custom";
    case environment::production:
    default:
        return "production";
    }
}

inline const char* label(environment value) {
    switch (value) {
    case environment::development:
        return "Development";
    case environment::custom:
        return "Custom";
    case environment::production:
    default:
        return "Production";
    }
}

inline environment parse(std::string value) {
    if (value == "development" || value == "dev") {
        return environment::development;
    }
    if (value == "custom") {
        return environment::custom;
    }
    return environment::production;
}

inline environment next(environment value) {
    switch (value) {
    case environment::production:
        return environment::development;
    case environment::development:
        return environment::custom;
    case environment::custom:
    default:
        return environment::production;
    }
}

inline environment previous(environment value) {
    switch (value) {
    case environment::production:
        return environment::custom;
    case environment::development:
        return environment::production;
    case environment::custom:
    default:
        return environment::development;
    }
}

inline std::string configured_url(environment value, const std::string& custom_url) {
    switch (value) {
    case environment::development:
        return kDevelopmentServerUrl;
    case environment::custom:
        return custom_url.empty() ? std::string(kDefaultCustomServerUrl) : custom_url;
    case environment::production:
    default:
        return kProductionServerUrl;
    }
}

std::string normalize_url(const std::string& server_url);
std::string active_server_url_from_settings();

}  // namespace server_environment
