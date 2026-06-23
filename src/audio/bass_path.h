#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#include "bass.h"
#include "path_utils.h"

namespace bass_path {

inline std::string lowercase_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline bool is_remote_stream_url(const std::string& path_or_url) {
    const std::string lowered = lowercase_ascii(path_or_url);
    return lowered.rfind("http://", 0) == 0 || lowered.rfind("https://", 0) == 0;
}

inline std::filesystem::path filesystem_path(const std::string& utf8_path) {
    return path_utils::from_utf8(utf8_path);
}

inline HSTREAM create_file_stream(const std::string& utf8_path, DWORD flags = 0) {
#ifdef _WIN32
    const std::wstring wide_path = filesystem_path(utf8_path).wstring();
    return BASS_StreamCreateFile(FALSE, wide_path.c_str(), 0, 0, flags);
#else
    return BASS_StreamCreateFile(FALSE, utf8_path.c_str(), 0, 0, flags);
#endif
}

inline HSTREAM create_url_stream(const std::string& url, DWORD flags = 0) {
    return BASS_StreamCreateURL(url.c_str(), 0, flags, nullptr, nullptr);
}

inline HSTREAM create_stream(const std::string& utf8_path_or_url, DWORD flags = 0) {
    if (is_remote_stream_url(utf8_path_or_url)) {
        return create_url_stream(utf8_path_or_url, flags);
    }
    return create_file_stream(utf8_path_or_url, flags);
}

inline HSAMPLE load_sample(const std::string& utf8_path, DWORD max, DWORD flags = 0) {
#ifdef _WIN32
    const std::wstring wide_path = filesystem_path(utf8_path).wstring();
    return BASS_SampleLoad(FALSE, wide_path.c_str(), 0, 0, max, flags);
#else
    return BASS_SampleLoad(FALSE, utf8_path.c_str(), 0, 0, max, flags);
#endif
}

}  // namespace bass_path

