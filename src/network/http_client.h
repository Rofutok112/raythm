#pragma once

#include <string>
#include <utility>
#include <vector>

namespace network::http {

struct response {
    int status_code = 0;
    std::string body;
    std::string error_message;
};

response send_request(const std::string& method,
                      const std::string& url,
                      const std::vector<std::pair<std::string, std::string>>& headers,
                      const std::string& body = {});

}  // namespace network::http
