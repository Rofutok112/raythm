#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "chart_parser.h"

namespace {
std::string chart_path(const std::string& file_name) {
    const std::filesystem::path repo_root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    return (repo_root / "assets" / "charts" / file_name).string();
}

bool expect_success(const std::string& path) {
    const chart_parse_result result = chart_parser::parse(path);
    if (!result.success || !result.data.has_value()) {
        std::cerr << "Expected success for " << path << '\n';
        for (const std::string& error : result.errors) {
            std::cerr << "  " << error << '\n';
        }
        return false;
    }

    return true;
}

bool expect_failure(const std::string& path, const std::string& expected_fragment) {
    const chart_parse_result result = chart_parser::parse(path);
    if (result.success) {
        std::cerr << "Expected failure for " << path << '\n';
        return false;
    }

    for (const std::string& error : result.errors) {
        if (error.find(expected_fragment) != std::string::npos) {
            return true;
        }
    }

    std::cerr << "Expected error containing '" << expected_fragment << "' for " << path << '\n';
    for (const std::string& error : result.errors) {
        std::cerr << "  " << error << '\n';
    }
    return false;
}
}

int main() {
    bool ok = true;

    ok = expect_success(chart_path("parser_valid.chart")) && ok;
    ok = expect_failure(chart_path("parser_invalid_overlap.chart"), "overlapping notes") && ok;
    ok = expect_failure(chart_path("parser_invalid_metadata.chart"), "keyCount must be 4 or 6") && ok;

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "chart_parser smoke test passed\n";
    return EXIT_SUCCESS;
}
