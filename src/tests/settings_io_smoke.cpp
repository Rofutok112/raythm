#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#include "app_paths.h"
#include "game_settings.h"
#include "settings_io.h"

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
        if (!active_) {
            return;
        }
        set_local_app_data(previous_value_);
    }

    bool active() const {
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
    const fs::path temp_local_app_data = fs::temp_directory_path() / "raythm_settings_io_smoke";

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

        game_settings defaults;
        defaults.camera_angle_degrees = 30.0f;
        defaults.lane_width = 8.4f;
        defaults.note_height = 1.4f;
        defaults.target_fps = 240;
        defaults.fullscreen = true;
        defaults.dark_mode = true;
        defaults.server_env = server_environment::environment::development;

        initialize_settings_storage(defaults);

        expect(fs::is_directory(app_paths::app_data_root()), "Expected AppData root to be created.", ok);
        expect(fs::is_directory(app_paths::songs_root()), "Expected songs directory to be created.", ok);
        expect(fs::is_directory(app_paths::charts_root()), "Expected charts directory to be created.", ok);
        expect(fs::is_regular_file(app_paths::settings_path()), "Expected settings.json to be created.", ok);

        game_settings loaded;
        load_settings(loaded);
        expect(std::fabs(loaded.camera_angle_degrees - defaults.camera_angle_degrees) < 0.001f,
               "Expected default camera angle to be written to settings.json.",
               ok);
        expect(std::fabs(loaded.lane_width - defaults.lane_width) < 0.001f,
               "Expected lane width above 5.0 to round-trip through settings.json.",
               ok);
        expect(std::fabs(loaded.note_height - defaults.note_height) < 0.001f,
               "Expected default note height to be written to settings.json.",
               ok);
        expect(loaded.target_fps == defaults.target_fps,
               "Expected default target FPS to be written to settings.json.",
               ok);
        expect(loaded.fullscreen == defaults.fullscreen,
               "Expected default fullscreen flag to be written to settings.json.",
               ok);
        expect(loaded.dark_mode == defaults.dark_mode,
               "Expected default dark mode flag to be written to settings.json.",
               ok);
        expect(loaded.server_env == defaults.server_env,
               "Expected server environment to be written to settings.json.",
               ok);
        expect(server_environment::active_server_url_from_settings() == "https://dev-api.raythm.net",
               "Expected active server URL to resolve the development environment.",
               ok);

        game_settings different_defaults = defaults;
        different_defaults.target_fps = 60;
        different_defaults.fullscreen = false;
        initialize_settings_storage(different_defaults);

        game_settings loaded_again;
        load_settings(loaded_again);
        expect(loaded_again.target_fps == defaults.target_fps,
               "Expected existing settings.json not to be overwritten.",
               ok);
        expect(loaded_again.fullscreen == defaults.fullscreen,
               "Expected existing fullscreen setting not to be overwritten.",
               ok);
    }

    fs::remove_all(temp_local_app_data, ec);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "settings_io smoke test passed\n";
    return EXIT_SUCCESS;
}
