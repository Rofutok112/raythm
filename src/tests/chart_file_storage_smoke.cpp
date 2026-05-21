#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "chart_file_storage.h"

namespace {

std::vector<unsigned char> bytes_from_string(const std::string& value) {
    return {value.begin(), value.end()};
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "raythm_chart_file_storage_smoke.rchart";

    const std::string chart =
        "[Metadata]\n"
        "chartId=storage-smoke\n"
        "keyCount=4\n"
        "difficulty=Ray\n"
        "chartAuthor=Codex\n"
        "formatVersion=2\n"
        "resolution=480\n"
        "offset=0\n\n"
        "[Timing]\n"
        "bpm,0,120\n"
        "meter,0,4/4\n\n"
        "[Notes]\n"
        "stay,53280,0,ray\n"
        "tap,53280,0\n";

    std::string error_message;
    bool ok = chart_file_storage::write_validated_raw_chart_file(
        path, bytes_from_string(chart), error_message);
    if (!ok) {
        std::cerr << error_message << '\n';
    }

    ok = read_text_file(path) == chart && ok;

    const std::string invalid_chart =
        "[Metadata]\n"
        "chartId=invalid-storage-smoke\n";
    ok = !chart_file_storage::write_validated_raw_chart_file(
        path, bytes_from_string(invalid_chart), error_message) && ok;
    ok = read_text_file(path) == chart && ok;

    std::error_code ec;
    std::filesystem::remove(path, ec);

    if (!ok) {
        std::cerr << "chart_file_storage smoke test failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "chart_file_storage smoke test passed\n";
    return EXIT_SUCCESS;
}
