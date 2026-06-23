#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vector>

namespace file_io {

inline std::vector<unsigned char> read_binary_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size <= 0) {
        return {};
    }

    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!input.good() && !input.eof()) {
        return {};
    }
    return bytes;
}

inline bool write_binary_file(const std::filesystem::path& path, const unsigned char* data, int size) {
    if (data == nullptr || size <= 0) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    output.write(reinterpret_cast<const char*>(data), size);
    return output.good();
}

}  // namespace file_io
