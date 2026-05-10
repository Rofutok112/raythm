#include "network/network_error.h"

#include <utility>

#include "network/json_helpers.h"

namespace network {
namespace {

std::string clean_message(std::string value) {
    value = json::trim(value);
    while (!value.empty() && (value.back() == '.' || value.back() == ' ')) {
        value.pop_back();
    }
    return value;
}

}  // namespace

std::string format_maintenance_message(const std::string& server_message,
                                       const std::string& retry_after) {
    const std::string trimmed_message = clean_message(server_message);
    std::string message = trimmed_message.empty()
        ? "Maintenance mode: raythm-Server is temporarily unavailable"
        : "Maintenance mode: " + trimmed_message;

    const std::string trimmed_retry_after = json::trim(retry_after);
    if (!trimmed_retry_after.empty()) {
        message += ". Retry after " + trimmed_retry_after;
    }

    message += ".";
    return message;
}

error_classification classify_http_error(int status_code,
                                         const std::string& body,
                                         std::string fallback_message,
                                         const std::string& retry_after) {
    if (status_code == 503 && json::extract_string(body, "error").value_or("") == "maintenance") {
        return {
            .kind = error_kind::maintenance,
            .message = format_maintenance_message(
                json::extract_string(body, "message").value_or(""),
                retry_after),
            .retry_after = json::trim(retry_after),
        };
    }

    const std::string message = json::extract_string(body, "message").value_or(std::move(fallback_message));
    return {
        .kind = error_kind::none,
        .message = message,
        .retry_after = json::trim(retry_after),
    };
}

}  // namespace network
