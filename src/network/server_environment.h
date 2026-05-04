#pragma once

#include <string>

namespace server_environment {

enum class environment {
    production,
    development,
};

inline constexpr const char* kProductionServerUrl = "https://api.raythm.net";
inline constexpr const char* kDevelopmentServerUrl = "https://dev-api.raythm.net";

inline const char* key(environment value) {
    switch (value) {
    case environment::development:
        return "development";
    case environment::production:
    default:
        return "production";
    }
}

inline const char* label(environment value) {
    switch (value) {
    case environment::development:
        return "Development";
    case environment::production:
    default:
        return "Production";
    }
}

inline environment parse(std::string value) {
    if (value == "development" || value == "dev") {
        return environment::development;
    }
    return environment::production;
}

inline environment next(environment value) {
    switch (value) {
    case environment::production:
        return environment::development;
    case environment::development:
    default:
        return environment::production;
    }
}

inline environment previous(environment value) {
    switch (value) {
    case environment::production:
        return environment::development;
    case environment::development:
    default:
        return environment::production;
    }
}

inline std::string configured_url(environment value) {
    switch (value) {
    case environment::development:
        return kDevelopmentServerUrl;
    case environment::production:
    default:
        return kProductionServerUrl;
    }
}

std::string normalize_url(const std::string& server_url);
std::string active_server_url_from_settings();

}  // namespace server_environment
