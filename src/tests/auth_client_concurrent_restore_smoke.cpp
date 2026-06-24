#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app_paths.h"
#include "network/auth_client.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace {

#ifdef _WIN32

std::string http_response(int status, const std::string& body) {
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

std::string user_object() {
    return "\"user\":{"
           "\"id\":\"user-me\","
           "\"email\":\"me@example.test\","
           "\"displayName\":\"Me\","
           "\"avatarUrl\":\"\","
           "\"emailVerified\":true,"
           "\"externalLinks\":[]"
           "}";
}

std::string refreshed_session_body() {
    return "{"
           "\"accessToken\":\"new-token\","
           "\"refreshToken\":\"new-refresh\","
           + user_object() +
           "}";
}

std::string me_body() {
    return "{" + user_object() + "}";
}

class local_auth_server {
public:
    explicit local_auth_server(int expected_requests) : expected_requests_(expected_requests) {
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

    ~local_auth_server() {
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

    std::vector<std::string> requests() const {
        std::lock_guard<std::mutex> lock(requests_mutex_);
        return requests_;
    }

    bool wait_for_requests(int expected) const {
        for (int i = 0; i < 400; ++i) {
            if (request_count_.load(std::memory_order_acquire) >= expected) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

private:
    void run() {
        for (int i = 0; i < expected_requests_; ++i) {
            SOCKET client = accept(socket_, nullptr, nullptr);
            if (client == INVALID_SOCKET) {
                break;
            }
            const std::string request = read_request(client);
            {
                std::lock_guard<std::mutex> lock(requests_mutex_);
                requests_.push_back(request);
                request_count_.store(static_cast<int>(requests_.size()), std::memory_order_release);
            }

            std::string response;
            if (has_line(request, "GET /me ") && has_line(request, "Authorization: Bearer old-token")) {
                response = http_response(401, "{\"message\":\"expired\"}");
            } else if (has_line(request, "POST /auth/refresh ")) {
                refresh_count_.fetch_add(1, std::memory_order_acq_rel);
                response = http_response(200, refreshed_session_body());
            } else if (has_line(request, "GET /me ") && has_line(request, "Authorization: Bearer new-token")) {
                response = http_response(200, me_body());
            } else {
                response = http_response(401, "{\"message\":\"unexpected request\"}");
            }
            send(client, response.data(), static_cast<int>(response.size()), 0);
            shutdown(client, SD_BOTH);
            closesocket(client);
        }
    }

    int expected_requests_ = 0;
    SOCKET socket_ = INVALID_SOCKET;
    unsigned short port_ = 0;
    std::string server_url_;
    std::thread thread_;
    mutable std::mutex requests_mutex_;
    std::vector<std::string> requests_;
    std::atomic<int> request_count_{0};
    std::atomic<int> refresh_count_{0};
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
    const std::filesystem::path temp_root =
        std::filesystem::temp_directory_path() / "raythm-auth-concurrent-restore-smoke";
    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    std::filesystem::create_directories(temp_root, ec);
    assert(!ec);
    _putenv_s("LOCALAPPDATA", temp_root.string().c_str());

    local_auth_server server(8);
    write_saved_session(server.server_url());

    std::vector<auth::operation_result> results(3);
    std::vector<std::thread> workers;
    for (int i = 0; i < 3; ++i) {
        workers.emplace_back([&results, i]() {
            results[static_cast<size_t>(i)] = auth::restore_saved_session();
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }

    for (const auth::operation_result& result : results) {
        assert(result.success);
        assert(result.session_data.has_value());
        assert(result.session_data->access_token == "new-token");
        assert(result.session_data->refresh_token == "new-refresh");
    }

    assert(server.wait_for_requests(4));
    const std::vector<std::string> requests = server.requests();
    int refresh_requests = 0;
    int new_me_requests = 0;
    int old_me_requests = 0;
    for (const std::string& request : requests) {
        if (has_line(request, "POST /auth/refresh ")) {
            ++refresh_requests;
        }
        if (has_line(request, "GET /me ") && has_line(request, "Authorization: Bearer old-token")) {
            ++old_me_requests;
        }
        if (has_line(request, "GET /me ") && has_line(request, "Authorization: Bearer new-token")) {
            ++new_me_requests;
        }
    }
    assert(refresh_requests == 1);
    assert(old_me_requests >= 1);
    assert(new_me_requests >= 1);

    const std::optional<auth::session> saved = auth::load_saved_session();
    assert(saved.has_value());
    assert(saved->access_token == "new-token");
    assert(saved->refresh_token == "new-refresh");

    std::cout << "auth_client_concurrent_restore smoke test passed\n";
    return 0;
#endif
}
