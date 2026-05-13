#include "chart_file_storage.h"

#include <fstream>
#include <system_error>

#include "chart_parser.h"
#include "path_utils.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

bool write_binary_file(const std::filesystem::path& path,
                       const std::vector<unsigned char>& bytes,
                       std::string& error_message) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error_message = "Failed to open a local file for writing.";
        return false;
    }

    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }

    if (!output.good()) {
        error_message = "Failed to write downloaded data to disk.";
        return false;
    }

    return true;
}

bool replace_file_with_temp(const std::filesystem::path& temp_path,
                            const std::filesystem::path& target_path) {
#ifdef _WIN32
    const std::wstring temp = temp_path.wstring();
    const std::wstring target = target_path.wstring();
    return MoveFileExW(temp.c_str(),
                       target.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    std::filesystem::rename(temp_path, target_path, ec);
    return !ec;
#endif
}

}  // namespace

namespace chart_file_storage {

bool write_validated_raw_chart_file(const std::filesystem::path& path,
                                    const std::vector<unsigned char>& bytes,
                                    std::string& error_message) {
    std::filesystem::create_directories(path.parent_path());
    const std::filesystem::path temp_path = path.parent_path() / (path.filename().string() + ".download.tmp");
    if (!write_binary_file(temp_path, bytes, error_message)) {
        return false;
    }

    const chart_parse_result parsed = chart_parser::parse(path_utils::to_utf8(temp_path));
    if (!parsed.success || !parsed.data.has_value()) {
        std::filesystem::remove(temp_path);
        error_message = parsed.errors.empty() ? "Downloaded chart file was invalid." : parsed.errors.front();
        return false;
    }

    if (!replace_file_with_temp(temp_path, path)) {
        std::filesystem::remove(temp_path);
        error_message = "Failed to write downloaded chart data to disk.";
        return false;
    }

    return true;
}

}  // namespace chart_file_storage
