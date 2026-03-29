#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#include "audio_manager.h"

namespace {
std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}
}

int main() {
    const std::filesystem::path audio_path = repo_root() / "assets" / "songs" / "valid_song" / "audio.mp3";
    if (!std::filesystem::exists(audio_path)) {
        std::cerr << "Audio asset not found: " << audio_path.string() << '\n';
        return 1;
    }

    audio_manager& manager = audio_manager::instance();
    if (!manager.initialize()) {
        std::cerr << "AudioManager initialization failed\n";
        return 1;
    }

    if (!manager.load_bgm(audio_path.string())) {
        std::cerr << "BGM load failed\n";
        return 1;
    }
    manager.set_bgm_volume(0.2f);
    manager.play_bgm();

    if (!manager.load_preview(audio_path.string())) {
        std::cerr << "Preview load failed\n";
        return 1;
    }
    manager.seek_preview(1.0);
    manager.set_preview_volume(0.1f);
    manager.play_preview();

    const int se_voice = manager.play_se(audio_path.string(), 0.05f);
    if (se_voice == 0) {
        std::cerr << "SE play failed\n";
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    manager.update();

    if (!manager.is_bgm_loaded() || !manager.is_preview_loaded()) {
        std::cerr << "Loaded voices became unavailable\n";
        return 1;
    }

    manager.stop_preview();
    manager.stop_bgm();
    manager.stop_all_se();
    manager.shutdown();

    std::cout << "audio_manager smoke test passed\n";
    return 0;
}
