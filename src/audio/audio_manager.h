#pragma once

#include <array>
#include <cstddef>
#include <string>

class audio;

struct audio_clock_snapshot {
    bool loaded = false;
    bool playing = false;
    double stream_position_seconds = 0.0;
    double audio_time_seconds = 0.0;
    double device_latency_seconds = 0.0;
    double device_buffer_seconds = 0.0;
};

class audio_manager final {
public:
    static audio_manager& instance();

    audio_manager(const audio_manager&) = delete;
    audio_manager& operator=(const audio_manager&) = delete;

    bool initialize();
    void shutdown();
    bool is_initialized() const;

    bool load_bgm(const std::string& file_path);
    void play_bgm(bool restart = true);
    void pause_bgm();
    void stop_bgm();
    void set_bgm_volume(float volume);
    void seek_bgm(double seconds);
    bool is_bgm_loaded() const;
    bool is_bgm_playing() const;
    double get_bgm_position_seconds() const;
    double get_bgm_length_seconds() const;
    audio_clock_snapshot get_bgm_clock() const;
    bool get_bgm_fft256(std::array<float, 128>& spectrum) const;
    bool get_bgm_oscilloscope256(std::array<float, 256>& samples) const;
    double get_output_latency_seconds() const;
    double get_output_buffer_seconds() const;

    bool load_preview(const std::string& file_path);
    void play_preview(bool restart = true);
    void pause_preview();
    void stop_preview();
    void unload_preview();
    void set_preview_volume(float volume);
    void seek_preview(double seconds);
    bool is_preview_loaded() const;
    bool is_preview_playing() const;
    double get_preview_position_seconds() const;
    double get_preview_length_seconds() const;

    bool preload_se(const std::string& file_path);
    int play_se(const std::string& file_path, float volume = 1.0f);
    bool is_se_voice_active(int voice_id) const;
    std::size_t get_active_se_voice_count() const;
    void stop_se(int voice_id);
    void stop_all_se();
    void set_se_volume(float volume);
    void update();

private:
    friend class audio;

    audio_manager() = default;
    ~audio_manager();

    struct managed_voice;

    void retain_legacy_client();
    void release_legacy_client();

    static bool is_voice_loaded(unsigned long handle);
    static bool is_voice_playing(unsigned long handle);
    static double get_voice_position_seconds(unsigned long handle);
    static double get_voice_length_seconds(unsigned long handle);
    static audio_clock_snapshot get_voice_clock(unsigned long handle);
    static void play_voice(unsigned long handle, bool restart);
    static void pause_voice(unsigned long handle);
    static void stop_voice(unsigned long handle);
    static void set_voice_position_seconds(unsigned long handle, double seconds);
    static void free_voice(unsigned long& handle);

    bool ensure_initialized();
    unsigned long create_stream(const std::string& file_path) const;
    void replace_voice(unsigned long& handle, const std::string& file_path) const;
    void apply_bgm_volume() const;
    void apply_preview_volume() const;

    bool initialized_ = false;
    int legacy_client_count_ = 0;
    float bgm_volume_ = 1.0f;
    float preview_volume_ = 1.0f;
    float se_volume_ = 1.0f;
    unsigned long bgm_handle_ = 0;
    unsigned long preview_handle_ = 0;
    int next_se_voice_id_ = 1;
};
