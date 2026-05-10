#pragma once

#include <string>

namespace server_environment {

enum class environment {
    production,
    development,
};

inline constexpr const char* kProductionServerUrl = "https://api.raythm.net";
inline constexpr const char* kDevelopmentServerUrl = "https://dev-api.raythm.net";

inline std::string configured_url(environment value) {
    switch (value) {
    case environment::development:
        return kDevelopmentServerUrl;
    case environment::production:
    default:
        return kProductionServerUrl;
    }
}

environment build_environment();
std::string normalize_url(const std::string& server_url);
std::string active_server_url();

}  // namespace server_environment
