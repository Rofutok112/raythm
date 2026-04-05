#include "updater/update_workflow.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::optional<std::string> parse_prefixed_argument(std::string_view argument, std::string_view prefix) {
    if (!argument.starts_with(prefix)) {
        return std::nullopt;
    }
    return std::string(argument.substr(prefix.size()));
}

}  // namespace

namespace updater {

std::vector<std::string> build_updater_arguments(const update_launch_request& request) {
    std::vector<std::string> arguments;
    arguments.emplace_back("--current-version=" + to_string(request.current_version));
    arguments.emplace_back("--target-version=" + to_string(request.target_release.version));
    arguments.emplace_back("--target-tag=" + request.target_release.tag_name);
    if (!request.target_release.assets.package_url.empty()) {
        arguments.emplace_back("--package-url=" + request.target_release.assets.package_url);
    }
    if (!request.target_release.assets.checksum_url.empty()) {
        arguments.emplace_back("--checksum-url=" + request.target_release.assets.checksum_url);
    }
    return arguments;
}

std::optional<update_launch_request> parse_updater_arguments(int argc, char* argv[]) {
    update_launch_request request{};
    bool has_current_version = false;
    bool has_target_version = false;
    bool has_target_tag = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];

        if (const auto value = parse_prefixed_argument(argument, "--current-version=")) {
            const std::optional<semantic_version> version = parse_semantic_version(*value);
            if (!version.has_value()) {
                return std::nullopt;
            }
            request.current_version = *version;
            has_current_version = true;
            continue;
        }

        if (const auto value = parse_prefixed_argument(argument, "--target-version=")) {
            const std::optional<semantic_version> version = parse_semantic_version(*value);
            if (!version.has_value()) {
                return std::nullopt;
            }
            request.target_release.version = *version;
            has_target_version = true;
            continue;
        }

        if (const auto value = parse_prefixed_argument(argument, "--target-tag=")) {
            request.target_release.tag_name = *value;
            has_target_tag = true;
            continue;
        }

        if (const auto value = parse_prefixed_argument(argument, "--package-url=")) {
            request.target_release.assets.package_url = *value;
            continue;
        }

        if (const auto value = parse_prefixed_argument(argument, "--checksum-url=")) {
            request.target_release.assets.checksum_url = *value;
            continue;
        }
    }

    if (!has_current_version || !has_target_version || !has_target_tag) {
        return std::nullopt;
    }

    return request;
}

}  // namespace updater
