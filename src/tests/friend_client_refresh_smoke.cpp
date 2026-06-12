#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "app_paths.h"
#include "network/friend_client.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace {

#ifdef _WIN32

std::string http_response(int status, std::string body) {
    const char* reason = status == 200 ? "OK" : "Unauthorized";
    return "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Connection: close\r\n"
           "\r\n" + body;
}

std::string read_request(SOCKET client) {
    std::string request;
    char buffer[4096];
    while (request.find("\r\n\r\n") == std::string::npos) {
        const int received = recv(client, buffer, static_cast<int>(sizeof(buffer)), 0);
        if (received <= 0) {
            break;
        }
        request.append(buffer, buffer + received);
    }
    return request;
}

bool has_line(const std::string& request, const std::string& needle) {
    return request.find(needle) != std::string::npos;
}

std::string refreshed_session_body(const std::string& server_url) {
    return "{"
           "\"accessToken\":\"new-token\","
           "\"refreshToken\":\"new-refresh\","
           "\"user\":{"
           "\"id\":\"user-me\","
           "\"email\":\"me@example.test\","
           "\"displayName\":\"Me\","
           "\"avatarUrl\":\"\","
           "\"emailVerified\":true,"
           "\"externalLinks\":[]"
           "}"
           "}";
}

class local_friend_server {
public:
    local_friend_server() {
        WSADATA data{};
        assert(WSAStartup(MAKEWORD(2, 2), &data) == 0);
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        assert(socket_ != INVALID_SOCKET);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        assert(bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        assert(listen(socket_, SOMAXCONN) == 0);

        sockaddr_in bound{};
        int bound_size = sizeof(bound);
        assert(getsockname(socket_, reinterpret_cast<sockaddr*>(&bound), &bound_size) == 0);
        port_ = ntohs(bound.sin_port);
        server_url_ = "http://127.0.0.1:" + std::to_string(port_);
        thread_ = std::thread([this]() { run(); });
    }

    ~local_friend_server() {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        WSACleanup();
    }

    const std::string& server_url() const {
        return server_url_;
    }

    const std::vector<std::string>& requests() const {
        return requests_;
    }

    bool wait_for_requests(int expected) const {
        for (int i = 0; i < 200; ++i) {
            if (request_count_.load(std::memory_order_acquire) >= expected) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

private:
    void run() {
        for (int i = 0; i < 4; ++i) {
            SOCKET client = accept(socket_, nullptr, nullptr);
            if (client == INVALID_SOCKET) {
                break;
            }
            const std::string request = read_request(client);
            requests_.push_back(request);
            request_count_.store(static_cast<int>(requests_.size()), std::memory_order_release);

            std::string response;
            if (has_line(request, "GET /friends ") && has_line(request, "Authorization: Bearer old-token")) {
                response = http_response(401, "{\"message\":\"expired\"}");
            } else if (has_line(request, "GET /me ")) {
                response = http_response(401, "{\"message\":\"expired\"}");
            } else if (has_line(request, "POST /auth/refresh ")) {
                response = http_response(200, refreshed_session_body(server_url_));
            } else if (has_line(request, "GET /friends ") && has_line(request, "Authorization: Bearer new-token")) {
                response = http_response(200,
                                         "{"
                                         "\"pendingRequestCount\":0,"
                                         "\"unreadInviteCount\":0,"
                                         "\"friends\":[{\"id\":\"friend-a\",\"displayName\":\"Aki\"}]"
                                         "}");
            } else {
                response = http_response(401, "{\"message\":\"unexpected request\"}");
            }
            send(client, response.data(), static_cast<int>(response.size()), 0);
            shutdown(client, SD_BOTH);
            closesocket(client);
        }
    }

    SOCKET socket_ = INVALID_SOCKET;
    unsigned short port_ = 0;
    std::string server_url_;
    std::thread thread_;
    std::vector<std::string> requests_;
    std::atomic<int> request_count_{0};
};

#endif

void write_saved_session(const std::string& server_url) {
    app_paths::ensure_directories();
    std::ofstream output(app_paths::auth_session_path(), std::ios::binary | std::ios::trunc);
    assert(output.is_open());
    output << "{\n"
           << "  \"serverUrl\": \"" << server_url << "\",\n"
           << "  \"accessToken\": \"old-token\",\n"
           << "  \"refreshToken\": \"old-refresh\",\n"
           << "  \"user\": {\n"
           << "    \"id\": \"user-me\",\n"
           << "    \"email\": \"me@example.test\",\n"
           << "    \"displayName\": \"Me\",\n"
           << "    \"avatarUrl\": \"\",\n"
           << "    \"emailVerified\": true,\n"
           << "    \"externalLinks\": []\n"
           << "  }\n"
           << "}\n";
}

}  // namespace

int main() {
#ifndef _WIN32
    return 0;
#else
    const std::filesystem::path temp_root = std::filesystem::temp_directory_path() / "raythm-friend-refresh-smoke";
    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    std::filesystem::create_directories(temp_root, ec);
    assert(!ec);
    _putenv_s("LOCALAPPDATA", temp_root.string().c_str());

    local_friend_server server;
    write_saved_session(server.server_url());

    const friend_client::friend_listing_result result = friend_client::fetch_friends();
    assert(result.success);
    assert(result.listing.has_value());
    assert(result.listing->friends.size() == 1);
    assert(result.listing->friends.front().id == "friend-a");
    assert(server.wait_for_requests(4));
    assert(server.requests().size() == 4);
    assert(has_line(server.requests()[0], "GET /friends "));
    assert(has_line(server.requests()[1], "GET /me "));
    assert(has_line(server.requests()[2], "POST /auth/refresh "));
    assert(has_line(server.requests()[3], "Authorization: Bearer new-token"));

    std::cout << "friend_client_refresh smoke test passed\n";
    return 0;
#endif
}
