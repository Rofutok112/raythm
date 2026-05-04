#include "network/server_environment.h"

#include <cctype>
#include <string>

namespace {

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

}  // namespace

namespace server_environment {

environment build_environment() {
#ifdef RAYTHM_SERVER_ENV
    const std::string value = trim(RAYTHM_SERVER_ENV);
    if (value == "development" || value == "dev") {
        return environment::development;
    }
#endif
    return environment::production;
}

std::string normalize_url(const std::string& server_url) {
    std::string normalized = trim(server_url);
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

std::string active_server_url() {
#ifdef RAYTHM_API_BASE_URL
    const std::string override_url = normalize_url(RAYTHM_API_BASE_URL);
    if (!override_url.empty()) {
        return override_url;
    }
#endif
    return normalize_url(configured_url(build_environment()));
}

}  // namespace server_environment
