#include "chart_file_storage.h"

#include <fstream>
#include <system_error>

#include "chart_parser.h"
#include "chart_serializer.h"
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

bool write_validated_chart_file_with_local_id(const std::filesystem::path& path,
                                              const std::vector<unsigned char>& bytes,
                                              const std::string& local_chart_id,
                                              std::string& error_message) {
    if (local_chart_id.empty()) {
        error_message = "Downloaded chart file was missing a local chart ID.";
        return false;
    }

    std::filesystem::create_directories(path.parent_path());
    const std::filesystem::path raw_temp_path =
        path.parent_path() / (path.filename().string() + ".download.tmp");
    if (!write_binary_file(raw_temp_path, bytes, error_message)) {
        return false;
    }

    const chart_parse_result parsed = chart_parser::parse(path_utils::to_utf8(raw_temp_path));
    std::error_code ec;
    std::filesystem::remove(raw_temp_path, ec);
    if (!parsed.success || !parsed.data.has_value()) {
        error_message = parsed.errors.empty() ? "Downloaded chart file was invalid." : parsed.errors.front();
        return false;
    }

    chart_data chart = *parsed.data;
    chart.meta.chart_id = local_chart_id;
    const std::filesystem::path rewrite_temp_path =
        path.parent_path() / (path.filename().string() + ".rewrite.tmp");
    std::filesystem::remove(rewrite_temp_path, ec);
    if (!chart_serializer::serialize(chart, path_utils::to_utf8(rewrite_temp_path))) {
        error_message = "Failed to prepare downloaded chart data for local storage.";
        std::filesystem::remove(rewrite_temp_path, ec);
        return false;
    }

    const chart_parse_result rewritten = chart_parser::parse(path_utils::to_utf8(rewrite_temp_path));
    if (!rewritten.success || !rewritten.data.has_value() ||
        rewritten.data->meta.chart_id != local_chart_id) {
        error_message = "Failed to validate downloaded chart data for local storage.";
        std::filesystem::remove(rewrite_temp_path, ec);
        return false;
    }

    if (!replace_file_with_temp(rewrite_temp_path, path)) {
        std::filesystem::remove(rewrite_temp_path, ec);
        error_message = "Failed to write downloaded chart data to disk.";
        return false;
    }

    return true;
}

}  // namespace chart_file_storage
