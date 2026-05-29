#include "updater/update_apply.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <optional>
#include <thread>
#include <system_error>

#include "updater/update_log.h"

namespace {

bool should_skip_copy(const std::filesystem::path& source_path, bool skip_updater_executable) {
    if (!skip_updater_executable) {
        return false;
    }
    return source_path.filename() == "Updater.exe";
}

std::string utf8_path(const std::filesystem::path& path) {
    return path.string();
}

bool copy_file_with_retries(const std::filesystem::path& source_path,
                            const std::filesystem::path& destination_path) {
    for (int attempt = 0; attempt < 5; ++attempt) {
        std::error_code ec;
        std::filesystem::copy_file(source_path, destination_path, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            return true;
        }

        updater::append_update_log(
            "updater",
            "copy failed from " + utf8_path(source_path) + " to " + utf8_path(destination_path) +
                " on attempt " + std::to_string(attempt + 1) + ": " + ec.message());

        ec.clear();
        std::filesystem::remove(destination_path, ec);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return false;
}

bool replace_directory_contents_if_present(const std::filesystem::path& source_root,
                                           const std::filesystem::path& destination_root,
                                           const std::string& label) {
    if (!std::filesystem::exists(source_root) || !std::filesystem::is_directory(source_root)) {
        return true;
    }

    std::error_code ec;
    std::filesystem::remove_all(destination_root, ec);
    if (ec) {
        updater::append_update_log(
            "updater",
            "failed to reset " + label + " directory " + utf8_path(destination_root) + ": " + ec.message());
        return false;
    }

    std::filesystem::create_directories(destination_root, ec);
    if (ec) {
        updater::append_update_log(
            "updater",
            "failed to recreate " + label + " directory " + utf8_path(destination_root) + ": " + ec.message());
        return false;
    }

    return true;
}

std::optional<std::uintmax_t> regular_file_size(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return std::nullopt;
    }

    ec.clear();
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        return std::nullopt;
    }
    return size;
}

bool same_regular_file_contents(const std::filesystem::path& left, const std::filesystem::path& right) {
    const std::optional<std::uintmax_t> left_size = regular_file_size(left);
    const std::optional<std::uintmax_t> right_size = regular_file_size(right);
    if (!left_size.has_value() || !right_size.has_value() || *left_size != *right_size) {
        return false;
    }

    std::ifstream left_input(left, std::ios::binary);
    std::ifstream right_input(right, std::ios::binary);
    if (!left_input.is_open() || !right_input.is_open()) {
        return false;
    }

    constexpr std::streamsize kBufferSize = 8192;
    char left_buffer[kBufferSize];
    char right_buffer[kBufferSize];
    while (left_input && right_input) {
        left_input.read(left_buffer, kBufferSize);
        right_input.read(right_buffer, kBufferSize);
        const std::streamsize left_count = left_input.gcount();
        const std::streamsize right_count = right_input.gcount();
        if (left_count != right_count) {
            return false;
        }
        if (!std::equal(left_buffer, left_buffer + left_count, right_buffer)) {
            return false;
        }
    }

    return left_input.eof() && right_input.eof();
}

bool staged_package_matches_install(const std::filesystem::path& install_root,
                                    const std::filesystem::path& staged_root) {
    return same_regular_file_contents(install_root / "Launcher.exe", staged_root / "Launcher.exe") ||
           same_regular_file_contents(install_root / "raythm.exe", staged_root / "raythm.exe");
}

}  // namespace

namespace updater {

bool copy_directory_contents(const std::filesystem::path& source_root,
                             const std::filesystem::path& destination_root,
                             bool skip_updater_executable) {
    if (!std::filesystem::exists(source_root) || !std::filesystem::is_directory(source_root)) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(destination_root, ec);
    if (ec) {
        return false;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root, ec)) {
        if (ec) {
            append_update_log("updater",
                              "failed to enumerate " + utf8_path(source_root) + ": " + ec.message());
            return false;
        }

        const std::filesystem::path relative_path = std::filesystem::relative(entry.path(), source_root, ec);
        if (ec || relative_path.empty()) {
            append_update_log("updater",
                              "failed to compute relative path for " + utf8_path(entry.path()) +
                                  (ec ? ": " + ec.message() : ""));
            return false;
        }

        const std::filesystem::path destination_path = destination_root / relative_path;
        if (entry.is_directory()) {
            std::filesystem::create_directories(destination_path, ec);
            if (ec) {
                append_update_log("updater",
                                  "failed to create directory " + utf8_path(destination_path) + ": " + ec.message());
                return false;
            }
            continue;
        }

        if (!entry.is_regular_file() || should_skip_copy(entry.path(), skip_updater_executable)) {
            continue;
        }

        std::filesystem::create_directories(destination_path.parent_path(), ec);
        if (ec) {
            append_update_log("updater",
                              "failed to create parent directory " + utf8_path(destination_path.parent_path()) +
                                  ": " + ec.message());
            return false;
        }

        if (!copy_file_with_retries(entry.path(), destination_path)) {
            return false;
        }
    }

    return true;
}

bool apply_staged_update(const std::filesystem::path& install_root,
                         const std::filesystem::path& staged_root,
                         const std::filesystem::path& backup_root,
                         bool skip_updater_executable) {
    if (!std::filesystem::exists(staged_root) || !std::filesystem::is_directory(staged_root)) {
        return false;
    }

    std::error_code ec;
    std::filesystem::remove_all(backup_root, ec);
    ec.clear();

    if (!copy_directory_contents(install_root, backup_root)) {
        return false;
    }

    if (!replace_directory_contents_if_present(staged_root / "assets", install_root / "assets", "assets")) {
        return false;
    }

    if (copy_directory_contents(staged_root, install_root, skip_updater_executable)) {
        return true;
    }

    if (!replace_directory_contents_if_present(backup_root / "assets", install_root / "assets", "assets")) {
        return false;
    }

    return copy_directory_contents(backup_root, install_root, skip_updater_executable);
}

bool repair_installed_updater_from_staged_package(const std::filesystem::path& install_root,
                                                  const std::filesystem::path& staged_root) {
    const std::filesystem::path staged_updater = staged_root / "Updater.exe";
    const std::filesystem::path installed_updater = install_root / "Updater.exe";

    if (!regular_file_size(staged_updater).has_value()) {
        return true;
    }

    if (!staged_package_matches_install(install_root, staged_root)) {
        return true;
    }

    if (same_regular_file_contents(installed_updater, staged_updater)) {
        return true;
    }

    append_update_log("launcher", "refreshing installed updater executable from staged package");
    return copy_file_with_retries(staged_updater, installed_updater);
}

}  // namespace updater
