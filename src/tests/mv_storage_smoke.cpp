#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

#include "app_paths.h"
#include "core/path_utils.h"
#include "data_models.h"
#include "mv/mv_storage.h"

namespace {
namespace fs = std::filesystem;

bool set_local_app_data(const std::string& value) {
#ifdef _WIN32
    return _putenv_s("LOCALAPPDATA", value.c_str()) == 0;
#else
    return setenv("LOCALAPPDATA", value.c_str(), 1) == 0;
#endif
}

class local_app_data_guard {
public:
    explicit local_app_data_guard(const fs::path& path) {
        if (const char* current = std::getenv("LOCALAPPDATA")) {
            previous_value_ = current;
        }
        active_ = set_local_app_data(path.string());
    }

    ~local_app_data_guard() {
        if (active_) {
            set_local_app_data(previous_value_);
        }
    }

    [[nodiscard]] bool active() const {
        return active_;
    }

private:
    std::string previous_value_;
    bool active_ = false;
};

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

}  // namespace

int main() {
    const fs::path temp_local_app_data = fs::temp_directory_path() / "raythm_mv_storage_smoke";

    std::error_code ec;
    fs::remove_all(temp_local_app_data, ec);
    ec.clear();
    fs::create_directories(temp_local_app_data, ec);
    if (ec) {
        std::cerr << "Failed to prepare temporary LOCALAPPDATA root\n";
        return EXIT_FAILURE;
    }

    bool ok = true;
    {
        local_app_data_guard guard(temp_local_app_data);
        if (!guard.active()) {
            std::cerr << "Failed to update LOCALAPPDATA for smoke test\n";
            return EXIT_FAILURE;
        }

        app_paths::ensure_directories();

        song_meta song;
        song.song_id = "song-123";
        song.title = "Smoke Song";

        mv::mv_package package = mv::make_default_package_for_song(song);
        package.meta.name = "Smoke MV";
        package.meta.author = "Codex";

        expect(mv::write_mv_json(package.meta, package.directory), "Expected mv.json to be written.", ok);
        expect(mv::save_script(package, "def draw(ctx):\n  return Scene([])\n"), "Expected script to be written.", ok);

        const fs::path import_dir = temp_local_app_data / "MV Import" / "日本語";
        fs::create_directories(import_dir, ec);
        expect(!ec, "Expected to create import directory.", ok);
        const fs::path import_path = import_dir / "imported.rmv";
        {
            std::ofstream out(import_path, std::ios::trunc);
            out << "def draw(ctx):\n  DrawBackground(fill=\"#112233\")\n";
        }
        expect(mv::import_script(package, path_utils::to_utf8(import_path)),
               "Expected import_script to overwrite the package script.",
               ok);
        expect(mv::load_script(package) == "def draw(ctx):\n  DrawBackground(fill=\"#112233\")\n",
               "Expected imported script to replace the previous script.",
               ok);
        expect(mv::import_script(package, path_utils::to_utf8(mv::script_path(package))),
               "Expected import_script to accept re-importing the current script file.",
               ok);

        const auto found = mv::find_first_package_for_song(song.song_id);
        expect(found.has_value(), "Expected to find MV package by song ID.", ok);
        if (found.has_value()) {
            expect(found->meta.mv_id == package.meta.mv_id, "Expected mv_id to round-trip.", ok);
            expect(found->meta.name == package.meta.name, "Expected MV name to round-trip.", ok);
            expect(found->meta.author == package.meta.author, "Expected MV author to round-trip.", ok);
            expect(found->meta.song_id == song.song_id, "Expected song_id to round-trip.", ok);
            expect(mv::load_script(*found) == "def draw(ctx):\n  DrawBackground(fill=\"#112233\")\n",
                   "Expected script source to round-trip.",
                   ok);
        }
    }

    fs::remove_all(temp_local_app_data, ec);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "mv_storage smoke test passed\n";
    return EXIT_SUCCESS;
}
