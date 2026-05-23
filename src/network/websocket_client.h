#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace network::websocket {

class client {
public:
    client();
    ~client();

    client(const client&) = delete;
    client& operator=(const client&) = delete;

    bool connect(const std::string& url,
                 const std::vector<std::pair<std::string, std::string>>& headers);
    void close();
    bool connected() const;
    bool send_text(const std::string& message);
    std::vector<std::string> poll_messages();
    std::string last_error() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace network::websocket
