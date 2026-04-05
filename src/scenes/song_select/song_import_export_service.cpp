#include "song_select/song_import_export_service.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <system_error>

#include "app_paths.h"
#include "chart_parser.h"
#include "chart_serializer.h"
#include "file_dialog.h"
#include "path_utils.h"
#include "song_loader.h"
#include "song_writer.h"

namespace {
namespace fs = std::filesystem;

std::string sanitize_file_stem(const std::string& value, const char* fallback) {
    std::string sanitized;
    sanitized.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
            case '<':
            case '>':
            case ':':
            case '"':
            case '/':
            case '\\':
            case '|':
            case '?':
            case '*':
                sanitized.push_back('_');
                break;
            default:
                sanitized.push_back(ch);
                break;
        }
    }

    if (sanitized.empty()) {
        sanitized = fallback;
    }
    return sanitized;
}

bool is_valid_song_index(const song_select::state& state, int song_index) {
    return song_index >= 0 && song_index < static_cast<int>(state.songs.size());
}

bool is_valid_chart_index(const song_select::song_entry& song, int chart_index) {
    return chart_index >= 0 && chart_index < static_cast<int>(song.charts.size());
}

std::optional<song_select::chart_option> find_chart_by_id(const song_select::state& state, const std::string& chart_id) {
    for (const auto& song : state.songs) {
        for (const auto& chart : song.charts) {
            if (chart.meta.chart_id == chart_id) {
                return chart;
            }
        }
    }
    return std::nullopt;
}

std::optional<song_select::song_entry> find_song_by_id(const song_select::state& state, const std::string& song_id) {
    for (const auto& song : state.songs) {
        if (song.song.meta.song_id == song_id) {
            return song;
        }
    }
    return std::nullopt;
}

fs::path make_temp_directory(const char* prefix) {
    const auto nonce = static_cast<long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    fs::path temp_dir = fs::temp_directory_path() /
                        (std::string("raythm_") + prefix + "_" + std::to_string(nonce));
    std::error_code ec;
    fs::create_directories(temp_dir, ec);
    if (ec) {
        return {};
    }
    return temp_dir;
}

class scoped_temp_directory {
public:
    explicit scoped_temp_directory(const char* prefix)
        : path_(make_temp_directory(prefix)) {
    }

    ~scoped_temp_directory() {
        if (path_.empty()) {
            return;
        }

        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    [[nodiscard]] const fs::path& path() const {
        return path_;
    }

    [[nodiscard]] bool valid() const {
        return !path_.empty();
    }

private:
    fs::path path_;
};

#ifdef _WIN32
std::wstring quote_powershell_argument(const fs::path& path) {
    std::wstring value = path.wstring();
    size_t cursor = 0;
    while ((cursor = value.find(L"'", cursor)) != std::wstring::npos) {
        value.replace(cursor, 1, L"''");
        cursor += 2;
    }
    return L"'" + value + L"'";
}

bool run_powershell_command(const std::wstring& script) {
    std::wstring command_line =
        L"powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -Command \"" + script + L"\"";
    return _wsystem(command_line.c_str()) == 0;
}
#endif

bool create_archive_from_directory(const fs::path& source_directory, const fs::path& archive_path) {
#ifdef _WIN32
    std::error_code ec;
    fs::create_directories(archive_path.parent_path(), ec);
    if (ec) {
        return false;
    }

    const fs::path zip_archive_path =
        archive_path.extension() == ".zip" ? archive_path : archive_path.parent_path() / (archive_path.stem().wstring() + L".zip");
    if (zip_archive_path != archive_path && fs::exists(zip_archive_path)) {
        fs::remove(zip_archive_path, ec);
        ec.clear();
    }

    const std::wstring script =
        L"$ErrorActionPreference='Stop'; "
        L"if (Test-Path -LiteralPath " + quote_powershell_argument(zip_archive_path) + L") { "
        L"Remove-Item -LiteralPath " + quote_powershell_argument(zip_archive_path) + L" -Force; } "
        L"Compress-Archive -Path (Join-Path " + quote_powershell_argument(source_directory) +
        L" '*') -DestinationPath " + quote_powershell_argument(zip_archive_path) + L" -CompressionLevel Optimal";
    if (!run_powershell_command(script)) {
        return false;
    }

    if (zip_archive_path == archive_path) {
        return true;
    }

    fs::remove(archive_path, ec);
    ec.clear();
    fs::rename(zip_archive_path, archive_path, ec);
    return !ec;
#else
    (void)source_directory;
    (void)archive_path;
    return false;
#endif
}

bool extract_archive_to_directory(const fs::path& archive_path, const fs::path& destination_directory) {
#ifdef _WIN32
    std::error_code ec;
    fs::remove_all(destination_directory, ec);
    ec.clear();
    fs::create_directories(destination_directory, ec);
    if (ec) {
        return false;
    }

    const fs::path zip_archive_path =
        archive_path.extension() == ".zip"
            ? archive_path
            : destination_directory / (archive_path.stem().wstring() + L".zip");
    if (zip_archive_path != archive_path) {
        fs::copy_file(archive_path, zip_archive_path, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            return false;
        }
    }

    const std::wstring script =
        L"$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath " + quote_powershell_argument(zip_archive_path) +
        L" -DestinationPath " + quote_powershell_argument(destination_directory) + L" -Force";
    const bool extracted = run_powershell_command(script);
    if (zip_archive_path != archive_path) {
        fs::remove(zip_archive_path, ec);
    }
    return extracted;
#else
    (void)archive_path;
    (void)destination_directory;
    return false;
#endif
}

bool copy_file_into_directory(const fs::path& source_path, const fs::path& destination_directory,
                              const fs::path& relative_destination) {
    std::error_code ec;
    fs::create_directories((destination_directory / relative_destination).parent_path(), ec);
    if (ec) {
        return false;
    }

    fs::copy_file(source_path,
                  destination_directory / relative_destination,
                  fs::copy_options::overwrite_existing,
                  ec);
    return !ec;
}

std::optional<fs::path> find_song_json_root(const fs::path& directory) {
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(directory, ec)) {
        if (ec) {
            ec.clear();
            return std::nullopt;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        if (entry.path().filename() == "song.json") {
            return entry.path().parent_path();
        }
    }

    return std::nullopt;
}

}  // namespace

namespace song_select {

transfer_result export_chart_package(const state& state, int song_index, int chart_index) {
    transfer_result result;
    if (!is_valid_song_index(state, song_index)) {
        result.message = "Chart export target is invalid.";
        return result;
    }

    const song_entry& song = state.songs[static_cast<size_t>(song_index)];
    if (!is_valid_chart_index(song, chart_index)) {
        result.message = "Chart export target is invalid.";
        return result;
    }

    const chart_option& chart = song.charts[static_cast<size_t>(chart_index)];
    const chart_parse_result parsed = chart_parser::parse(chart.path);
    if (!parsed.success || !parsed.data.has_value()) {
        result.message = "Failed to read the selected chart.";
        return result;
    }

    const std::string default_name = sanitize_file_stem(
        chart.meta.chart_id.empty() ? chart.meta.difficulty : chart.meta.chart_id,
        "chart") + ".rchart";
    const std::string save_path = file_dialog::save_chart_package_file(default_name);
    if (save_path.empty()) {
        result.cancelled = true;
        return result;
    }

    if (!chart_serializer::serialize(*parsed.data, save_path)) {
        result.message = "Failed to export the chart package.";
        return result;
    }

    result.success = true;
    result.message = "Chart exported.";
    return result;
}

transfer_result import_chart_package(const state& state, int song_index) {
    transfer_result result;
    (void)song_index;
    const std::optional<chart_import_request> request = prepare_chart_import(state, result);
    if (!request.has_value()) {
        return result;
    }
    return import_chart_package(*request);
}

std::optional<chart_import_request> prepare_chart_import(const state& state, transfer_result& result) {
    const std::string source_path = file_dialog::open_chart_package_file();
    if (source_path.empty()) {
        result.cancelled = true;
        return std::nullopt;
    }

    const chart_parse_result parsed = chart_parser::parse(source_path);
    if (!parsed.success || !parsed.data.has_value()) {
        result.message = "Failed to import the chart package.";
        return std::nullopt;
    }

    const std::optional<song_entry> target_song = find_song_by_id(state, parsed.data->meta.song_id);
    if (!target_song.has_value()) {
        result.message = "No song with a matching song ID was found.";
        return std::nullopt;
    }

    const std::optional<chart_option> existing_chart = find_chart_by_id(state, parsed.data->meta.chart_id);
    if (existing_chart.has_value() && existing_chart->source == content_source::official) {
        result.message = "Cannot overwrite an official chart.";
        return std::nullopt;
    }

    return chart_import_request{
        .source_path = source_path,
        .target_song_id = target_song->song.meta.song_id,
        .chart = *parsed.data,
        .overwrite_existing = existing_chart.has_value(),
    };
}

transfer_result import_chart_package(const chart_import_request& request) {
    transfer_result result;
    app_paths::ensure_directories();
    const fs::path destination_path = app_paths::chart_path(request.chart.meta.chart_id);
    if (!chart_serializer::serialize(request.chart, path_utils::to_utf8(destination_path))) {
        result.message = "Failed to save the imported chart.";
        return result;
    }

    result.success = true;
    result.reload_catalog = true;
    result.message = "Chart imported.";
    result.preferred_song_id = request.target_song_id;
    result.preferred_chart_id = request.chart.meta.chart_id;
    return result;
}

transfer_result export_song_package(const state& state, int song_index) {
    transfer_result result;
    if (!is_valid_song_index(state, song_index)) {
        result.message = "Song export target is invalid.";
        return result;
    }

    const std::optional<song_export_request> request = prepare_song_export(state, song_index);
    if (!request.has_value()) {
        result.cancelled = true;
        return result;
    }

    return export_song_package(*request);
}

std::optional<song_export_request> prepare_song_export(const state& state, int song_index) {
    if (!is_valid_song_index(state, song_index)) {
        return std::nullopt;
    }

    const song_entry& song = state.songs[static_cast<size_t>(song_index)];
    const std::string default_name = sanitize_file_stem(song.song.meta.song_id, "song") + ".rpack";
    const std::string save_path = file_dialog::save_song_package_file(default_name);
    if (save_path.empty()) {
        return std::nullopt;
    }

    return song_export_request{
        .song = song,
        .save_path = save_path,
    };
}

transfer_result export_song_package(const song_export_request& request) {
    transfer_result result;
    const fs::path song_directory = path_utils::from_utf8(request.song.song.directory);
    const fs::path audio_source = song_directory / path_utils::from_utf8(request.song.song.meta.audio_file);
    const fs::path jacket_source = song_directory / path_utils::from_utf8(request.song.song.meta.jacket_file);
    const bool has_jacket = !request.song.song.meta.jacket_file.empty();
    if (!fs::exists(audio_source) || !fs::is_regular_file(audio_source)) {
        result.message = "Song package export requires an existing audio file.";
        return result;
    }
    if (has_jacket && (!fs::exists(jacket_source) || !fs::is_regular_file(jacket_source))) {
        result.message = "Song package export requires the configured jacket file.";
        return result;
    }

    scoped_temp_directory staging("song_export");
    if (!staging.valid()) {
        result.message = "Failed to prepare the export directory.";
        return result;
    }

    if (!song_writer::write_song_json(request.song.song.meta, path_utils::to_utf8(staging.path())) ||
        !copy_file_into_directory(audio_source, staging.path(), path_utils::from_utf8(request.song.song.meta.audio_file))) {
        result.message = "Failed to export the song package.";
        return result;
    }
    if (has_jacket &&
        !copy_file_into_directory(jacket_source, staging.path(), path_utils::from_utf8(request.song.song.meta.jacket_file))) {
        result.message = "Failed to export the song package.";
        return result;
    }
    if (!create_archive_from_directory(staging.path(), path_utils::from_utf8(request.save_path))) {
        result.message = "Failed to export the song package.";
        return result;
    }

    result.success = true;
    result.message = "Song package exported.";
    return result;
}

transfer_result import_song_package(const state& state) {
    transfer_result result;
    const std::optional<song_import_request> request = prepare_song_import(state, result);
    if (!request.has_value()) {
        return result;
    }

    return import_song_package(*request);
}

std::optional<song_import_request> prepare_song_import(const state& state, transfer_result& result) {
    const std::string source_path = file_dialog::open_song_package_file();
    if (source_path.empty()) {
        result.cancelled = true;
        return std::nullopt;
    }

    song_import_prepare_result prepared = prepare_song_import_from_path(state, source_path);
    result = prepared.transfer;
    return prepared.request;
}

song_import_prepare_result prepare_song_import_from_path(const state& state, const std::string& source_path) {
    song_import_prepare_result prepared;
    scoped_temp_directory extract_root("song_import_prepare");
    if (!extract_root.valid() ||
        !extract_archive_to_directory(path_utils::from_utf8(source_path), extract_root.path())) {
        prepared.transfer.message = "Failed to extract the song package.";
        return prepared;
    }

    const std::optional<fs::path> extracted_song_root = find_song_json_root(extract_root.path());
    if (!extracted_song_root.has_value()) {
        prepared.transfer.message = "song.json was not found in the package.";
        return prepared;
    }

    const song_load_result loaded = song_loader::load_directory(path_utils::to_utf8(*extracted_song_root),
                                                                content_source::app_data);
    if (!loaded.errors.empty() || loaded.songs.empty()) {
        prepared.transfer.message = loaded.errors.empty() ? "Failed to read the song package metadata."
                                                          : loaded.errors.front();
        return prepared;
    }

    song_data imported_song = loaded.songs.front();
    const std::optional<song_entry> existing_song = find_song_by_id(state, imported_song.meta.song_id);
    if (existing_song.has_value() && existing_song->song.source == content_source::official) {
        prepared.transfer.message = "Cannot overwrite an official song package.";
        return prepared;
    }

    const fs::path persistent_extract_root = make_temp_directory("song_import_stage");
    if (persistent_extract_root.empty()) {
        prepared.transfer.message = "Failed to prepare the import directory.";
        return prepared;
    }

    std::error_code ec;
    const fs::path persistent_song_root = persistent_extract_root / extracted_song_root->filename();
    fs::copy(*extracted_song_root, persistent_song_root,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) {
        prepared.transfer.message = "Failed to prepare the import directory.";
        return prepared;
    }
    imported_song.directory = path_utils::to_utf8(persistent_song_root);

    prepared.request = song_import_request{
        .catalog_state = state,
        .source_path = source_path,
        .extracted_root = path_utils::to_utf8(persistent_extract_root),
        .imported_song = imported_song,
        .overwrite_existing = existing_song.has_value(),
    };
    return prepared;
}

transfer_result import_song_package(const song_import_request& request) {
    transfer_result result;
    const fs::path extracted_song_root = path_utils::from_utf8(request.imported_song.directory);
    const fs::path audio_source = extracted_song_root / path_utils::from_utf8(request.imported_song.meta.audio_file);
    const fs::path jacket_source = extracted_song_root / path_utils::from_utf8(request.imported_song.meta.jacket_file);
    const bool has_jacket = !request.imported_song.meta.jacket_file.empty();
    if (!fs::exists(audio_source) || !fs::is_regular_file(audio_source)) {
        result.message = "The song package is missing its audio file.";
        return result;
    }
    if (has_jacket && (!fs::exists(jacket_source) || !fs::is_regular_file(jacket_source))) {
        result.message = "The song package is missing its jacket file.";
        return result;
    }

    app_paths::ensure_directories();
    const fs::path destination_root = app_paths::song_dir(request.imported_song.meta.song_id);
    std::error_code ec;
    fs::remove_all(destination_root, ec);
    ec.clear();
    fs::create_directories(destination_root, ec);
    if (ec) {
        result.message = "Failed to prepare the user songs directory.";
        return result;
    }

    if (!song_writer::write_song_json(request.imported_song.meta, path_utils::to_utf8(destination_root)) ||
        !copy_file_into_directory(audio_source, destination_root, path_utils::from_utf8(request.imported_song.meta.audio_file))) {
        result.message = "Failed to import the song package.";
        return result;
    }
    if (has_jacket &&
        !copy_file_into_directory(jacket_source, destination_root, path_utils::from_utf8(request.imported_song.meta.jacket_file))) {
        result.message = "Failed to import the song package.";
        return result;
    }

    result.success = true;
    result.reload_catalog = true;
    result.message = "Song package imported.";
    result.preferred_song_id = request.imported_song.meta.song_id;
    return result;
}

void cleanup_song_import_request(song_import_request& request) {
    if (request.extracted_root.empty()) {
        return;
    }

    std::error_code ec;
    fs::remove_all(path_utils::from_utf8(request.extracted_root), ec);
    request.extracted_root.clear();
}

}  // namespace song_select
