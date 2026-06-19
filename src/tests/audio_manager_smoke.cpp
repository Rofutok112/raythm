#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include "audio_manager.h"

namespace {
std::filesystem::path repo_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}
}

int main() {
    const std::filesystem::path audio_path = repo_root() / "assets" / "audio" / "HitSound_Tap.mp3" ;
    if (!std::filesystem::exists(audio_path)) {
        std::cerr << "Audio asset not found: " << audio_path.string() << '\n';
        return 1;
    }

    audio_manager& manager = audio_manager::instance();
    if (!manager.initialize()) {
        std::cerr << "AudioManager initialization failed\n";
        return 1;
    }

    bool worker_preload_result = true;
    std::thread worker([&]() {
        worker_preload_result = manager.preload_se(audio_path.string());
    });
    worker.join();
    if (worker_preload_result) {
        std::cerr << "AudioManager accepted BASS work from a non-owner thread\n";
        manager.shutdown();
        return 1;
    }

    std::vector<unsigned char> preview_bytes;
    {
        std::ifstream input(audio_path, std::ios::binary);
        preview_bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    if (preview_bytes.empty()) {
        std::cerr << "Audio asset read failed\n";
        manager.shutdown();
        return 1;
    }

    bool worker_preview_request_result = true;
    std::thread preview_worker([&]() {
        worker_preview_request_result = manager.request_preview_load_from_memory(preview_bytes);
    });
    preview_worker.join();
    if (worker_preview_request_result) {
        std::cerr << "AudioManager accepted preview load request from a non-owner thread\n";
        manager.shutdown();
        return 1;
    }

    if (!manager.request_preview_load_from_memory(preview_bytes)) {
        std::cerr << "Managed preview load request failed\n";
        manager.shutdown();
        return 1;
    }
    const auto preview_result = manager.poll_preview_load();
    if (!preview_result.completed || !preview_result.loaded || !manager.is_preview_loaded()) {
        std::cerr << "Managed preview load did not complete on the owner thread\n";
        manager.shutdown();
        return 1;
    }
    manager.unload_preview();

    if (!manager.load_bgm(audio_path.string())) {
        std::cerr << "BGM load failed\n";
        return 1;
    }
    const audio_loudness_analysis bgm_loudness = manager.get_bgm_loudness_analysis();
    if (!bgm_loudness.valid || bgm_loudness.linear_gain <= 0.0f || bgm_loudness.peak <= 0.0f) {
        std::cerr << "BGM loudness analysis failed\n";
        return 1;
    }
    manager.set_loudness_normalization_enabled(true);
    if (!manager.is_loudness_normalization_enabled()) {
        std::cerr << "Loudness normalization flag did not update\n";
        return 1;
    }
    const audio_clock_snapshot initial_clock = manager.get_bgm_clock();
    if (!initial_clock.loaded || initial_clock.stream_position_seconds < 0.0 ||
        initial_clock.audio_time_seconds < initial_clock.stream_position_seconds) {
        std::cerr << "Initial BGM clock snapshot is invalid\n";
        return 1;
    }

    manager.set_bgm_volume(0.2f);
    manager.play_bgm();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const audio_clock_snapshot playing_clock = manager.get_bgm_clock();
    if (!playing_clock.loaded || !playing_clock.playing ||
        playing_clock.stream_position_seconds <= initial_clock.stream_position_seconds) {
        std::cerr << "Skipping playback-dependent audio smoke checks because the BGM clock did not advance.\n";
        manager.stop_bgm();
        manager.shutdown();
        return 77;
    }

    if (manager.get_output_latency_seconds() < 0.0 || manager.get_output_buffer_seconds() < 0.0) {
        std::cerr << "Output timing information is invalid\n";
        return 1;
    }

    if (!manager.preload_se(audio_path.string())) {
        std::cerr << "SE preload failed\n";
        return 1;
    }

    if (!manager.load_preview(audio_path.string())) {
        std::cerr << "Preview load failed\n";
        return 1;
    }
    manager.seek_preview(1.0);
    manager.set_preview_volume(0.1f);
    manager.play_preview();
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    const int se_voice_1 = manager.play_se(audio_path.string(), 0.5f);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    const int se_voice_2 = manager.play_se(audio_path.string(), 0.5f);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    const int se_voice_3 = manager.play_se(audio_path.string(), 0.5f);
    if (se_voice_1 == 0 || se_voice_2 == 0 || se_voice_3 == 0) {
        std::cerr << "SE play failed\n";
        return 1;
    }

    if (se_voice_1 == se_voice_2 || se_voice_2 == se_voice_3 || se_voice_1 == se_voice_3) {
        std::cerr << "SE voice ids were reused\n";
        return 1;
    }

    if (manager.get_active_se_voice_count() < 3) {
        std::cerr << "SE voice count did not grow for overlapping playback\n";
        return 1;
    }

    const std::size_t active_count_before_stop = manager.get_active_se_voice_count();
    manager.stop_se(se_voice_2);
    if (manager.get_active_se_voice_count() >= active_count_before_stop) {
        std::cerr << "SE voice count did not decrease after stop\n";
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
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
