#pragma once

#include <string>

namespace network {

enum class error_kind {
    none,
    maintenance,
};

struct error_classification {
    error_kind kind = error_kind::none;
    std::string message;
    std::string retry_after;

    bool is_maintenance() const {
        return kind == error_kind::maintenance;
    }
};

std::string format_maintenance_message(const std::string& server_message,
                                       const std::string& retry_after = {});

error_classification classify_http_error(int status_code,
                                         const std::string& body,
                                         std::string fallback_message,
                                         const std::string& retry_after = {});

}  // namespace network
