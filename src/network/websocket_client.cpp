#include "network/websocket_client.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace network::websocket {
namespace {

#ifdef _WIN32
struct url_parts {
    std::wstring host;
    std::wstring path_and_query;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
    bool secure = false;
};

std::wstring to_wstring(std::string_view value) {
    return std::wstring(value.begin(), value.end());
}

std::string describe_winhttp_error(DWORD error_code) {
    switch (error_code) {
        case ERROR_WINHTTP_TIMEOUT:
            return "WebSocket connection timed out.";
        case ERROR_WINHTTP_CANNOT_CONNECT:
            return "Could not connect to room WebSocket.";
        case ERROR_WINHTTP_CONNECTION_ERROR:
            return "Room WebSocket connection was interrupted.";
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
            return "The server name could not be resolved.";
        case ERROR_WINHTTP_INVALID_URL:
            return "The WebSocket URL is invalid.";
        case ERROR_WINHTTP_SECURE_FAILURE:
            return "A secure WebSocket connection could not be established.";
        default:
            return "Room WebSocket communication failed.";
    }
}

std::optional<url_parts> parse_url_parts(const std::string& url) {
    std::wstring wide_url = to_wstring(url);
    URL_COMPONENTSW components{};
    components.dwStructSize = sizeof(components);

    wchar_t host_buffer[256];
    wchar_t path_buffer[2048];
    wchar_t extra_buffer[2048];
    components.lpszHostName = host_buffer;
    components.dwHostNameLength = sizeof(host_buffer) / sizeof(wchar_t);
    components.lpszUrlPath = path_buffer;
    components.dwUrlPathLength = sizeof(path_buffer) / sizeof(wchar_t);
    components.lpszExtraInfo = extra_buffer;
    components.dwExtraInfoLength = sizeof(extra_buffer) / sizeof(wchar_t);

    if (WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &components) == FALSE) {
        return std::nullopt;
    }

    url_parts parts;
    parts.host.assign(components.lpszHostName, components.dwHostNameLength);
    parts.path_and_query.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0) {
        parts.path_and_query.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    parts.port = components.nPort;
    parts.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    return parts;
}
#endif

}  // namespace

struct client::impl {
#ifdef _WIN32
    HINTERNET session = nullptr;
    HINTERNET connection = nullptr;
    HINTERNET socket = nullptr;
#endif
    std::atomic<bool> connected{false};
    std::atomic<bool> stop_requested{false};
    std::thread receive_thread;
    mutable std::mutex mutex;
    std::vector<std::string> messages;
    std::string error;

    void set_error(std::string value) {
        std::lock_guard<std::mutex> lock(mutex);
        error = std::move(value);
    }

#ifdef _WIN32
    void receive_loop() {
        std::string current;
        char buffer[8192];
        while (!stop_requested.load() && socket != nullptr) {
            DWORD bytes_read = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type = WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
            const DWORD result = WinHttpWebSocketReceive(socket,
                                                         buffer,
                                                         static_cast<DWORD>(sizeof(buffer)),
                                                         &bytes_read,
                                                         &type);
            if (result != NO_ERROR) {
                if (!stop_requested.load()) {
                    set_error(describe_winhttp_error(result));
                }
                connected.store(false);
                return;
            }
            if (type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
                connected.store(false);
                return;
            }
            if (type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE ||
                type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                current.append(buffer, buffer + bytes_read);
                if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                    std::lock_guard<std::mutex> lock(mutex);
                    messages.push_back(current);
                    current.clear();
                }
            }
        }
        connected.store(false);
    }
#endif
};

client::client() : impl_(std::make_unique<impl>()) {
}

client::~client() {
    close();
}

bool client::connect(const std::string& url,
                     const std::vector<std::pair<std::string, std::string>>& headers) {
    close();
#ifndef _WIN32
    impl_->set_error("WebSocket is only supported on Windows in the current build.");
    (void)url;
    (void)headers;
    return false;
#else
    const std::optional<url_parts> parts = parse_url_parts(url);
    if (!parts.has_value()) {
        impl_->set_error("Invalid WebSocket URL.");
        return false;
    }

    impl_->session = WinHttpOpen(L"raythm/0.1",
                                 WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                 WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS,
                                 0);
    if (impl_->session == nullptr) {
        impl_->set_error(describe_winhttp_error(GetLastError()));
        return false;
    }
    WinHttpSetTimeouts(impl_->session, 5000, 5000, 5000, 5000);

    impl_->connection = WinHttpConnect(impl_->session, parts->host.c_str(), parts->port, 0);
    if (impl_->connection == nullptr) {
        impl_->set_error(describe_winhttp_error(GetLastError()));
        close();
        return false;
    }

    const DWORD request_flags = parts->secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(impl_->connection,
                                           L"GET",
                                           parts->path_and_query.c_str(),
                                           nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           request_flags);
    if (request == nullptr) {
        impl_->set_error(describe_winhttp_error(GetLastError()));
        close();
        return false;
    }

    std::wstring header_block;
    for (const auto& [name, value] : headers) {
        header_block += to_wstring(name);
        header_block += L": ";
        header_block += to_wstring(value);
        header_block += L"\r\n";
    }

    if (WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0) == FALSE ||
        WinHttpSendRequest(request,
                           header_block.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : header_block.c_str(),
                           header_block.empty() ? 0 : static_cast<DWORD>(-1L),
                           WINHTTP_NO_REQUEST_DATA,
                           0,
                           0,
                           0) == FALSE ||
        WinHttpReceiveResponse(request, nullptr) == FALSE) {
        impl_->set_error(describe_winhttp_error(GetLastError()));
        WinHttpCloseHandle(request);
        close();
        return false;
    }

    impl_->socket = WinHttpWebSocketCompleteUpgrade(request, 0);
    WinHttpCloseHandle(request);
    if (impl_->socket == nullptr) {
        impl_->set_error(describe_winhttp_error(GetLastError()));
        close();
        return false;
    }

    impl_->stop_requested.store(false);
    impl_->connected.store(true);
    impl_->receive_thread = std::thread([raw = impl_.get()]() {
        raw->receive_loop();
    });
    return true;
#endif
}

void client::close() {
    impl_->stop_requested.store(true);
#ifdef _WIN32
    if (impl_->socket != nullptr) {
        WinHttpWebSocketClose(impl_->socket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
    }
#endif
    if (impl_->receive_thread.joinable()) {
        impl_->receive_thread.join();
    }
#ifdef _WIN32
    if (impl_->socket != nullptr) {
        WinHttpCloseHandle(impl_->socket);
        impl_->socket = nullptr;
    }
    if (impl_->connection != nullptr) {
        WinHttpCloseHandle(impl_->connection);
        impl_->connection = nullptr;
    }
    if (impl_->session != nullptr) {
        WinHttpCloseHandle(impl_->session);
        impl_->session = nullptr;
    }
#endif
    impl_->connected.store(false);
}

bool client::connected() const {
    return impl_->connected.load();
}

bool client::send_text(const std::string& message) {
#ifndef _WIN32
    (void)message;
    return false;
#else
    if (!connected() || impl_->socket == nullptr) {
        return false;
    }
    const DWORD result = WinHttpWebSocketSend(
        impl_->socket,
        WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        const_cast<char*>(message.data()),
        static_cast<DWORD>(message.size()));
    if (result != NO_ERROR) {
        impl_->set_error(describe_winhttp_error(result));
        impl_->connected.store(false);
        return false;
    }
    return true;
#endif
}

std::vector<std::string> client::poll_messages() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<std::string> result;
    result.swap(impl_->messages);
    return result;
}

std::string client::last_error() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->error;
}

}  // namespace network::websocket
