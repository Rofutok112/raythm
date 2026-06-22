#include "mv_storage.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#include <windows.h>
#endif

#include "core/app_paths.h"
#include "core/uuid_util.h"
#include "mv/composition/mv_composition_serializer.h"
#include "network/json_helpers.h"
#include "path_utils.h"
#include "updater/update_verify.h"

namespace {
namespace fs = std::filesystem;
namespace json = network::json;

#ifdef _WIN32
FILE* open_file_read_binary(const fs::path& path) {
    return _wfopen(path.c_str(), L"rb");
}

FILE* open_file_write_binary(const fs::path& path) {
    return _wfopen(path.c_str(), L"wb");
}
#else
FILE* open_file_read_binary(const fs::path& path) {
    return std::fopen(path.string().c_str(), "rb");
}

FILE* open_file_write_binary(const fs::path& path) {
    return std::fopen(path.string().c_str(), "wb");
}
#endif

std::string trim(std::string_view value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

bool is_package_relative_path(std::string_view path) {
    if (path.empty()) {
        return false;
    }
    if (path.size() >= 2 && path[1] == ':') {
        return false;
    }
    const fs::path parsed = path_utils::from_utf8(std::string(path));
    if (parsed.is_absolute()) {
        return false;
    }
    for (const auto& part : parsed) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

std::string read_file(const fs::path& path) {
    FILE* file = open_file_read_binary(path);
    if (file == nullptr) {
        return {};
    }

    std::string content;
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return {};
    }

    const long size = std::ftell(file);
    if (size < 0) {
        std::fclose(file);
        return {};
    }

    if (std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return {};
    }

    content.resize(static_cast<size_t>(size));
    if (size > 0) {
        const size_t read = std::fread(content.data(), 1, static_cast<size_t>(size), file);
        if (read != static_cast<size_t>(size)) {
            std::fclose(file);
            return {};
        }
    }

    std::fclose(file);
    return content;
}

std::optional<std::string> try_read_file(const fs::path& path) {
    if (!fs::exists(path) || !fs::is_regular_file(path)) {
        return std::nullopt;
    }

    FILE* file = open_file_read_binary(path);
    if (file == nullptr) {
        return std::nullopt;
    }

    std::string content;
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return std::nullopt;
    }

    const long size = std::ftell(file);
    if (size < 0) {
        std::fclose(file);
        return std::nullopt;
    }

    if (std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return std::nullopt;
    }

    content.resize(static_cast<size_t>(size));
    if (size > 0) {
        const size_t read = std::fread(content.data(), 1, static_cast<size_t>(size), file);
        if (read != static_cast<size_t>(size)) {
            std::fclose(file);
            return std::nullopt;
        }
    }

    std::fclose(file);
    return content;
}

std::optional<mv::mv_metadata> parse_mv_metadata(const std::string& content) {
    mv::mv_metadata meta;
    const auto mv_id = json::extract_string(content, "mvId");
    const auto song_id = json::extract_string(content, "songId");
    const auto name = json::extract_string(content, "name");
    const auto author = json::extract_string(content, "author");
    const auto description = json::extract_string(content, "description");
    const auto composition_file = json::extract_string(content, "compositionFile");

    if (!mv_id.has_value() || !song_id.has_value() || !name.has_value()) {
        return std::nullopt;
    }

    meta.mv_id = trim(*mv_id);
    meta.song_id = trim(*song_id);
    meta.name = *name;
    meta.author = author.value_or("");
    meta.description = description.value_or("");
    meta.format_version = json::extract_int(content, "formatVersion").value_or(1);
    if (composition_file.has_value() && !trim(*composition_file).empty()) {
        meta.composition_file = trim(*composition_file);
    }

    if (meta.mv_id.empty() || meta.song_id.empty() ||
        !is_package_relative_path(meta.composition_file)) {
        return std::nullopt;
    }
    return meta;
}

std::string serialize_mv_json(const mv::mv_metadata& meta) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"mvId\": \"" << json::escape_string(meta.mv_id) << "\",\n";
    out << "  \"songId\": \"" << json::escape_string(meta.song_id) << "\",\n";
    out << "  \"name\": \"" << json::escape_string(meta.name) << "\",\n";
    out << "  \"author\": \"" << json::escape_string(meta.author) << "\",\n";
    out << "  \"description\": \"" << json::escape_string(meta.description) << "\",\n";
    out << "  \"compositionFile\": \""
        << json::escape_string(meta.composition_file.empty() ? "composition.rmvcomp" : meta.composition_file)
        << "\",\n";
    out << "  \"formatVersion\": " << (meta.format_version <= 0 ? 1 : meta.format_version) << "\n";
    out << "}\n";
    return out.str();
}

bool write_file(const fs::path& path, const std::string& content) {
    if (path.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            return false;
        }
    }

    FILE* file = open_file_write_binary(path);
    if (file == nullptr) {
        return false;
    }

    const size_t size = content.size();
    const size_t written = size == 0 ? 0 : std::fwrite(content.data(), 1, size, file);
    const bool ok = (written == size) && (std::fflush(file) == 0);
    std::fclose(file);
    return ok;
}

bool write_bytes(const fs::path& path, const std::vector<unsigned char>& bytes) {
    if (path.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec) {
            return false;
        }
    }

    FILE* file = open_file_write_binary(path);
    if (file == nullptr) {
        return false;
    }

    const size_t size = bytes.size();
    const size_t written = size == 0 ? 0 : std::fwrite(bytes.data(), 1, size, file);
    const bool ok = (written == size) && (std::fflush(file) == 0);
    std::fclose(file);
    return ok;
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
    explicit scoped_temp_directory(const char* prefix) : path_(make_temp_directory(prefix)) {
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
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" + script + L"\"";
    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESHOWWINDOW;
    startup_info.wShowWindow = 0;

    PROCESS_INFORMATION process_info{};
    std::wstring mutable_command_line = command_line;
    const BOOL created = CreateProcessW(
        nullptr,
        mutable_command_line.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup_info,
        &process_info);
    if (!created) {
        return false;
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return exit_code == 0;
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
        archive_path.extension() == ".zip"
            ? archive_path
            : archive_path.parent_path() / (archive_path.stem().wstring() + L".zip");
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

std::optional<fs::path> find_mv_json_root(const fs::path& directory) {
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(directory, ec)) {
        if (ec) {
            ec.clear();
            return std::nullopt;
        }
        if (entry.is_regular_file() && entry.path().filename() == "mv.json") {
            return entry.path().parent_path();
        }
    }
    return std::nullopt;
}

bool is_supported_image_extension(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

std::string asset_extension(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (ext == ".jpeg") {
        return ".jpg";
    }
    return ext.empty() ? ".png" : ext;
}

std::optional<mv::mv_package> load_package_directory(const fs::path& directory) {
    const fs::path json_path = directory / "mv.json";
    const std::string content = read_file(json_path);
    if (content.empty()) {
        return std::nullopt;
    }

    const std::optional<mv::mv_metadata> meta = parse_mv_metadata(content);
    if (!meta.has_value()) {
        return std::nullopt;
    }

    return mv::mv_package{*meta, path_utils::to_utf8(directory)};
}

std::optional<mv::mv_package> load_managed_package_directory(const fs::path& directory) {
    std::optional<mv::managed_storage::package_manifest> manifest =
        mv::managed_storage::read_manifest(directory);
    if (!manifest.has_value()) {
        return std::nullopt;
    }

    mv::mv_metadata meta;
    const managed_content_storage::managed_file_read_result mv_json =
        mv::managed_storage::read_mv_json_asset(*manifest, directory);
    if (mv_json.success) {
        const std::string content(mv_json.bytes.begin(), mv_json.bytes.end());
        if (const std::optional<mv::mv_metadata> parsed = parse_mv_metadata(content)) {
            meta = *parsed;
        }
    }

    meta.mv_id = manifest->local_mv_id.empty() ? mv::managed_storage::local_mv_id(manifest->mv)
                                               : manifest->local_mv_id;
    if (!manifest->local_song_id.empty()) {
        meta.song_id = manifest->local_song_id;
    } else if (meta.song_id.empty()) {
        meta.song_id = manifest->mv.remote_song_id;
    }
    if (meta.name.empty()) {
        meta.name = manifest->mv.remote_mv_id.empty() ? "Managed MV" : manifest->mv.remote_mv_id;
    }
    if (meta.composition_file.empty()) {
        meta.composition_file = "composition.rmvcomp";
    }
    if (meta.format_version <= 0) {
        meta.format_version = 2;
    }
    if (meta.mv_id.empty() || meta.song_id.empty()) {
        return std::nullopt;
    }
    manifest->local_mv_id = meta.mv_id;
    return mv::mv_package{
        meta,
        path_utils::to_utf8(directory),
        storage_policy::managed_package,
        manifest,
    };
}

}  // namespace

namespace mv {

std::vector<mv_package> load_all_packages() {
    std::vector<mv_package> packages;
    const fs::path root = app_paths::mvs_root();
    if (!fs::exists(root) || !fs::is_directory(root)) {
    } else {
        for (const auto& entry : fs::directory_iterator(root)) {
            if (!entry.is_directory()) {
                continue;
            }

            const auto package = load_package_directory(entry.path());
            if (package.has_value()) {
                packages.push_back(*package);
            }
        }
    }

    for (online_content::source source : {online_content::source::community, online_content::source::official}) {
        for (const fs::path& directory : managed_storage::list_package_directories(source)) {
            const auto package = load_managed_package_directory(directory);
            if (package.has_value()) {
                packages.push_back(*package);
            }
        }
    }

    std::sort(packages.begin(), packages.end(), [](const mv_package& left, const mv_package& right) {
        if (left.meta.song_id != right.meta.song_id) {
            return left.meta.song_id < right.meta.song_id;
        }
        if (left.meta.name != right.meta.name) {
            return left.meta.name < right.meta.name;
        }
        return left.meta.mv_id < right.meta.mv_id;
    });
    return packages;
}

std::optional<mv_package> find_first_package_for_song(const std::string& song_id) {
    const std::vector<mv_package> packages = load_all_packages();
    for (const mv_package& package : packages) {
        if (package.meta.song_id == song_id) {
            return package;
        }
    }
    return std::nullopt;
}

mv_package make_default_package_for_song(const song_meta& song) {
    mv_metadata meta;
    meta.mv_id = generate_uuid();
    meta.song_id = song.song_id;
    meta.name = song.title.empty() ? "New MV" : song.title + " MV";
    meta.author.clear();
    meta.composition_file = "composition.rmvcomp";
    meta.format_version = 2;
    mv_package package{meta, path_utils::to_utf8(app_paths::mv_dir(meta.mv_id))};
    package.song_duration_ms = song.duration_seconds > 0.0f
        ? static_cast<double>(song.duration_seconds) * 1000.0
        : 0.0;
    return package;
}

edit_access_result can_edit_package(const mv_package& package) {
    if (package.storage != storage_policy::managed_package) {
        return {};
    }
    std::optional<managed_storage::package_manifest> manifest =
        managed_storage::read_manifest(path_utils::from_utf8(package.directory));
    if (!manifest.has_value()) {
        manifest = package.managed_manifest;
    }
    if (!manifest.has_value()) {
        return {.editable = false, .reason = "Managed MV manifest not found."};
    }
    const managed_storage::edit_access_result access = managed_storage::can_edit(*manifest);
    return {.editable = access.editable, .reason = access.reason};
}

std::filesystem::path composition_path(const mv_package& package) {
    const fs::path dir = path_utils::from_utf8(package.directory);
    const std::string filename = package.meta.composition_file.empty()
        ? "composition.rmvcomp"
        : package.meta.composition_file;
    if (!is_package_relative_path(filename)) {
        return {};
    }
    return dir / path_utils::from_utf8(filename);
}

bool write_mv_json(const mv_metadata& meta, const std::string& directory) {
    const std::string composition_file = meta.composition_file.empty()
        ? "composition.rmvcomp"
        : meta.composition_file;
    if (!is_package_relative_path(composition_file)) {
        return false;
    }
    const std::filesystem::path json_path = path_utils::from_utf8(directory) / "mv.json";
    return write_file(json_path, serialize_mv_json(meta));
}

bool save_metadata(const mv_package& package, std::vector<std::string>* errors) {
    if (package.storage != storage_policy::managed_package) {
        return write_mv_json(package.meta, package.directory);
    }
    const std::string composition_file = package.meta.composition_file.empty()
        ? "composition.rmvcomp"
        : package.meta.composition_file;
    if (!is_package_relative_path(composition_file)) {
        if (errors != nullptr) {
            errors->push_back("MV compositionFile must be package-relative.");
        }
        return false;
    }

    std::optional<managed_storage::package_manifest> manifest =
        managed_storage::read_manifest(path_utils::from_utf8(package.directory));
    if (!manifest.has_value()) {
        manifest = package.managed_manifest;
    }
    if (!manifest.has_value()) {
        if (errors != nullptr) {
            errors->push_back("Managed MV manifest not found.");
        }
        return false;
    }
    const managed_storage::edit_access_result edit_access = managed_storage::can_edit(*manifest);
    if (!edit_access.editable) {
        if (errors != nullptr) {
            errors->push_back(edit_access.reason.empty()
                ? "Managed MV package is not editable."
                : edit_access.reason);
        }
        return false;
    }
    std::string error_message;
    if (!managed_storage::write_mv_json_asset(*manifest, serialize_mv_json(package.meta), error_message)) {
        if (errors != nullptr) {
            errors->push_back(error_message.empty()
                ? "Failed to write encrypted managed MV metadata."
                : error_message);
        }
        return false;
    }
    return true;
}

composition::mv_composition make_default_composition_for_song(const mv_package& package) {
    composition::mv_composition composition =
        composition::make_default_for_song(package.meta.mv_id, package.song_duration_ms);
    const std::string title = package.meta.name.empty() ? "New MV" : package.meta.name;
    for (composition::layer& current : composition.layers) {
        if (composition::component* renderer = composition::renderable_component(current);
            renderer != nullptr && renderer->type == "text" && renderer->text == "New MV") {
            renderer->text = title;
        }
    }
    return composition;
}

std::optional<composition::mv_composition> load_composition(const mv_package& package,
                                                            std::vector<std::string>* errors) {
    if (package.storage == storage_policy::managed_package) {
        std::optional<managed_storage::package_manifest> manifest =
            managed_storage::read_manifest(path_utils::from_utf8(package.directory));
        if (!manifest.has_value()) {
            manifest = package.managed_manifest;
        }
        if (!manifest.has_value()) {
            if (errors != nullptr) {
                errors->push_back("Managed MV manifest not found.");
            }
            return std::nullopt;
        }
        managed_storage::composition_read_result read =
            managed_storage::read_composition(*manifest, path_utils::from_utf8(package.directory));
        if (!read.success) {
            if (errors != nullptr) {
                *errors = read.errors;
            }
            return std::nullopt;
        }
        return read.composition;
    }

    const fs::path path = composition_path(package);
    if (path.empty()) {
        if (errors != nullptr) {
            errors->push_back("MV compositionFile must be package-relative.");
        }
        return std::nullopt;
    }
    const std::optional<std::string> content = try_read_file(path);
    if (!content.has_value()) {
        if (errors != nullptr) {
            errors->push_back("MV composition file not found.");
        }
        return std::nullopt;
    }

    composition::parse_result parsed = composition::parse(*content);
    if (!parsed.success) {
        if (errors != nullptr) {
            *errors = parsed.errors;
        }
        return std::nullopt;
    }
    return parsed.composition;
}

bool save_composition(const mv_package& package, const composition::mv_composition& composition) {
    if (package.storage == storage_policy::managed_package) {
        std::optional<managed_storage::package_manifest> manifest =
            managed_storage::read_manifest(path_utils::from_utf8(package.directory));
        if (!manifest.has_value()) {
            return false;
        }
        if (!managed_storage::can_edit(*manifest).editable) {
            return false;
        }
        std::string error_message;
        return managed_storage::write_composition(*manifest, composition, error_message);
    }

    composition::mv_composition normalized = composition;
    normalized.composition_id = package.meta.mv_id;
    const fs::path path = composition_path(package);
    return !path.empty() && write_file(path, composition::serialize(normalized));
}

bool import_composition(const mv_package& package, const std::string& source_path_utf8,
                        std::vector<std::string>* errors) {
    const fs::path source_path = path_utils::from_utf8(source_path_utf8);
    const std::optional<std::string> content = try_read_file(source_path);
    if (!content.has_value()) {
        if (errors != nullptr) {
            errors->push_back("Failed to read MV composition source file.");
        }
        return false;
    }
    composition::parse_result parsed = composition::parse(*content);
    if (!parsed.success) {
        if (errors != nullptr) {
            *errors = parsed.errors;
        }
        return false;
    }
    return save_composition(package, parsed.composition);
}

bool export_composition(const mv_package& package, const std::string& destination_path_utf8) {
    const fs::path destination_path = path_utils::from_utf8(destination_path_utf8);
    const std::optional<composition::mv_composition> composition = load_composition(package);
    if (!composition.has_value()) {
        return false;
    }
    composition::mv_composition normalized = *composition;
    normalized.composition_id = package.meta.mv_id;
    return write_file(destination_path, composition::serialize(normalized));
}

bool export_package(const mv_package& package,
                    const std::string& destination_path_utf8,
                    std::vector<std::string>* errors) {
    const std::optional<composition::mv_composition> composition = load_composition(package, errors);
    if (!composition.has_value()) {
        if (errors != nullptr && errors->empty()) {
            errors->push_back("Failed to load MV composition for package export.");
        }
        return false;
    }

    scoped_temp_directory staging("mv_export");
    if (!staging.valid()) {
        if (errors != nullptr) {
            errors->push_back("Failed to prepare MV package export directory.");
        }
        return false;
    }

    mv_metadata export_meta = package.meta;
    export_meta.composition_file = export_meta.composition_file.empty()
        ? "composition.rmvcomp"
        : export_meta.composition_file;
    if (!is_package_relative_path(export_meta.composition_file)) {
        if (errors != nullptr) {
            errors->push_back("MV compositionFile must be package-relative.");
        }
        return false;
    }
    if (!write_mv_json(export_meta, path_utils::to_utf8(staging.path()))) {
        if (errors != nullptr) {
            errors->push_back("Failed to write MV package metadata.");
        }
        return false;
    }

    composition::mv_composition normalized = *composition;
    normalized.composition_id = package.meta.mv_id;
    if (!write_file(staging.path() / path_utils::from_utf8(export_meta.composition_file),
                    composition::serialize(normalized))) {
        if (errors != nullptr) {
            errors->push_back("Failed to write MV package composition.");
        }
        return false;
    }

    for (const composition::asset_ref& asset : normalized.assets) {
        if (!is_package_relative_path(asset.path)) {
            if (errors != nullptr) {
                errors->push_back("MV asset path must be package-relative.");
            }
            return false;
        }
        std::vector<std::string> asset_errors;
        const std::optional<std::vector<unsigned char>> bytes =
            read_asset_bytes(package, asset, &asset_errors);
        if (!bytes.has_value()) {
            if (errors != nullptr) {
                if (!asset_errors.empty()) {
                    errors->insert(errors->end(), asset_errors.begin(), asset_errors.end());
                } else {
                    errors->push_back("Failed to read MV package asset.");
                }
            }
            return false;
        }
        if (!write_bytes(staging.path() / path_utils::from_utf8(asset.path), *bytes)) {
            if (errors != nullptr) {
                errors->push_back("Failed to write MV package asset.");
            }
            return false;
        }
    }

    if (!create_archive_from_directory(staging.path(), path_utils::from_utf8(destination_path_utf8))) {
        if (errors != nullptr) {
            errors->push_back("Failed to create MV package archive.");
        }
        return false;
    }
    return true;
}

bool import_package(const std::string& source_path_utf8,
                    const std::string& target_song_id,
                    std::vector<std::string>* errors) {
    scoped_temp_directory extract_root("mv_import");
    if (!extract_root.valid() ||
        !extract_archive_to_directory(path_utils::from_utf8(source_path_utf8), extract_root.path())) {
        if (errors != nullptr) {
            errors->push_back("Failed to extract MV package archive.");
        }
        return false;
    }

    const std::optional<fs::path> extracted_mv_root = find_mv_json_root(extract_root.path());
    if (!extracted_mv_root.has_value()) {
        if (errors != nullptr) {
            errors->push_back("mv.json was not found in the MV package.");
        }
        return false;
    }

    std::optional<mv_package> source_package = load_package_directory(*extracted_mv_root);
    if (!source_package.has_value()) {
        if (errors != nullptr) {
            errors->push_back("Failed to read MV package metadata.");
        }
        return false;
    }

    if (!target_song_id.empty()) {
        source_package->meta.song_id = target_song_id;
    }
    if (source_package->meta.composition_file.empty()) {
        source_package->meta.composition_file = "composition.rmvcomp";
    }
    if (!is_package_relative_path(source_package->meta.composition_file)) {
        if (errors != nullptr) {
            errors->push_back("MV compositionFile must be package-relative.");
        }
        return false;
    }

    const std::optional<composition::mv_composition> source_composition =
        load_composition(*source_package, errors);
    if (!source_composition.has_value()) {
        if (errors != nullptr && errors->empty()) {
            errors->push_back("Failed to read MV package composition.");
        }
        return false;
    }

    std::vector<std::pair<composition::asset_ref, std::vector<unsigned char>>> asset_bytes;
    asset_bytes.reserve(source_composition->assets.size());
    for (const composition::asset_ref& asset : source_composition->assets) {
        if (!is_package_relative_path(asset.path)) {
            if (errors != nullptr) {
                errors->push_back("MV asset path must be package-relative.");
            }
            return false;
        }
        std::vector<std::string> asset_errors;
        std::optional<std::vector<unsigned char>> bytes =
            read_asset_bytes(*source_package, asset, &asset_errors);
        if (!bytes.has_value()) {
            if (errors != nullptr) {
                if (!asset_errors.empty()) {
                    errors->insert(errors->end(), asset_errors.begin(), asset_errors.end());
                } else {
                    errors->push_back("The MV package is missing an asset file.");
                }
            }
            return false;
        }
        asset_bytes.push_back({asset, std::move(*bytes)});
    }

    app_paths::ensure_directories();
    const fs::path destination_root = app_paths::mv_dir(source_package->meta.mv_id);
    std::error_code ec;
    fs::remove_all(destination_root, ec);
    ec.clear();
    fs::create_directories(destination_root, ec);
    if (ec) {
        if (errors != nullptr) {
            errors->push_back("Failed to prepare the MV package directory.");
        }
        return false;
    }

    mv_package destination_package{source_package->meta, path_utils::to_utf8(destination_root)};
    if (!write_mv_json(destination_package.meta, destination_package.directory) ||
        !save_composition(destination_package, *source_composition)) {
        if (errors != nullptr) {
            errors->push_back("Failed to finalize the imported MV package.");
        }
        return false;
    }
    for (const auto& [asset, bytes] : asset_bytes) {
        if (!write_bytes(destination_root / path_utils::from_utf8(asset.path), bytes)) {
            if (errors != nullptr) {
                errors->push_back("Failed to install MV package asset.");
            }
            return false;
        }
    }
    return true;
}

bool ensure_composition_package(const mv_package& package) {
    if (!write_mv_json(package.meta, package.directory)) {
        return false;
    }
    const fs::path root = path_utils::from_utf8(package.directory);
    std::error_code ec;
    fs::create_directories(root / "assets" / "images", ec);
    if (ec) {
        return false;
    }
    fs::create_directories(root / "assets" / "generated", ec);
    if (ec) {
        return false;
    }
    if (fs::exists(composition_path(package))) {
        return true;
    }
    return save_composition(package, make_default_composition_for_song(package));
}

std::optional<composition::asset_ref> import_image_asset(const mv_package& package,
                                                         const std::string& source_path_utf8,
                                                         std::vector<std::string>* errors) {
    const fs::path source_path = path_utils::from_utf8(source_path_utf8);
    if (!fs::exists(source_path) || !fs::is_regular_file(source_path)) {
        if (errors != nullptr) {
            errors->push_back("Image asset source file not found.");
        }
        return std::nullopt;
    }
    if (!is_supported_image_extension(source_path)) {
        if (errors != nullptr) {
            errors->push_back("MV image assets must be PNG or JPEG files.");
        }
        return std::nullopt;
    }

    const std::optional<std::string> bytes = try_read_file(source_path);
    if (!bytes.has_value()) {
        if (errors != nullptr) {
            errors->push_back("Failed to read MV image asset.");
        }
        return std::nullopt;
    }

    if (package.storage != storage_policy::managed_package && !ensure_composition_package(package)) {
        if (errors != nullptr) {
            errors->push_back("Failed to prepare MV package asset directories.");
        }
        return std::nullopt;
    }

    const std::string sha256 = updater::compute_sha256_hex(std::string_view(*bytes));
    const std::string id = "asset-image-" + sha256.substr(0, 12);
    const std::string relative_path = "assets/images/" + id + asset_extension(source_path);

    if (package.storage == storage_policy::managed_package) {
        std::optional<managed_storage::package_manifest> manifest =
            managed_storage::read_manifest(path_utils::from_utf8(package.directory));
        if (!manifest.has_value()) {
            if (errors != nullptr) {
                errors->push_back("Managed MV manifest not found.");
            }
            return std::nullopt;
        }
        const managed_storage::edit_access_result edit_access = managed_storage::can_edit(*manifest);
        if (!edit_access.editable) {
            if (errors != nullptr) {
                errors->push_back(edit_access.reason.empty()
                    ? "Managed MV package is not editable."
                    : edit_access.reason);
            }
            return std::nullopt;
        }
        std::string error_message;
        if (!managed_storage::write_asset_file(*manifest, id, relative_path, *bytes, error_message)) {
            if (errors != nullptr) {
                errors->push_back(error_message.empty()
                    ? "Failed to write encrypted managed MV image asset."
                    : error_message);
            }
            return std::nullopt;
        }
        composition::asset_ref asset;
        asset.id = id;
        asset.type = "image";
        asset.path = relative_path;
        asset.sha256 = sha256;
        return asset;
    }

    const fs::path destination = path_utils::from_utf8(package.directory) / path_utils::from_utf8(relative_path);
    std::error_code ec;
    fs::create_directories(destination.parent_path(), ec);
    if (ec) {
        if (errors != nullptr) {
            errors->push_back("Failed to create MV image asset directory.");
        }
        return std::nullopt;
    }
    fs::copy_file(source_path, destination, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        if (errors != nullptr) {
            errors->push_back("Failed to copy MV image asset into the package.");
        }
        return std::nullopt;
    }

    composition::asset_ref asset;
    asset.id = id;
    asset.type = "image";
    asset.path = relative_path;
    asset.sha256 = sha256;
    return asset;
}

std::filesystem::path resolve_asset_path(const mv_package& package, const composition::asset_ref& asset) {
    if (!is_package_relative_path(asset.path)) {
        return {};
    }
    if (package.storage == storage_policy::managed_package) {
        return {};
    }
    return path_utils::from_utf8(package.directory) / path_utils::from_utf8(asset.path);
}

std::optional<std::vector<unsigned char>> read_asset_bytes(const mv_package& package,
                                                           const composition::asset_ref& asset,
    std::vector<std::string>* errors) {
    if (!is_package_relative_path(asset.path)) {
        if (errors != nullptr) {
            errors->push_back("MV asset path must be package-relative.");
        }
        return std::nullopt;
    }
    if (package.storage == storage_policy::managed_package) {
        std::optional<managed_storage::package_manifest> manifest =
            managed_storage::read_manifest(path_utils::from_utf8(package.directory));
        if (!manifest.has_value()) {
            if (errors != nullptr) {
                errors->push_back("Managed MV manifest not found.");
            }
            return std::nullopt;
        }
        managed_content_storage::managed_file_read_result read =
            managed_storage::read_asset_file(*manifest, path_utils::from_utf8(package.directory), asset.id);
        if (!read.success) {
            if (errors != nullptr) {
                errors->push_back(read.error_message.empty()
                    ? "Failed to decrypt managed MV asset."
                    : read.error_message);
            }
            return std::nullopt;
        }
        return read.bytes;
    }

    const fs::path path = resolve_asset_path(package, asset);
    const std::optional<std::string> bytes = try_read_file(path);
    if (!bytes.has_value()) {
        if (errors != nullptr) {
            errors->push_back("MV asset file not found.");
        }
        return std::nullopt;
    }
    return std::vector<unsigned char>(bytes->begin(), bytes->end());
}

}  // namespace mv
