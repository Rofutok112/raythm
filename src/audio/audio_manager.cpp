#include "audio_manager.h"

#include <algorithm>
#include <unordered_map>

#include "bass.h"

namespace {
constexpr unsigned long kLowLatencyUpdatePeriodMs = 10;
constexpr unsigned long kLowLatencyDeviceBufferMs = 20;
constexpr unsigned long kSeSampleMaxVoices = 16;

struct se_sample_entry {
    unsigned long handle = 0;
};

struct se_voice_entry {
    unsigned long handle = 0;
    float local_volume = 1.0f;
    bool stream_fallback = false;
};

std::unordered_map<std::string, se_sample_entry>& se_samples() {
    static std::unordered_map<std::string, se_sample_entry> samples;
    return samples;
}

std::unordered_map<int, se_voice_entry>& se_voices() {
    static std::unordered_map<int, se_voice_entry> voices;
    return voices;
}

void free_se_samples() {
    for (auto& [path, sample] : se_samples()) {
        (void)path;
        if (sample.handle != 0) {
            BASS_SampleFree(sample.handle);
            sample.handle = 0;
        }
    }
    se_samples().clear();
}
}

audio_manager& audio_manager::instance() {
    static audio_manager manager;
    return manager;
}

audio_manager::~audio_manager() {
    shutdown();
}

bool audio_manager::initialize() {
    if (initialized_) {
        return true;
    }

    BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, kLowLatencyUpdatePeriodMs);
    BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, kLowLatencyDeviceBufferMs);
    initialized_ = BASS_Init(-1, 44100, 0, nullptr, nullptr) != FALSE;
    return initialized_;
}

void audio_manager::shutdown() {
    stop_all_se();
    free_se_samples();
    free_voice(bgm_handle_);
    free_voice(preview_handle_);

    if (initialized_) {
        BASS_Free();
        initialized_ = false;
    }
}

bool audio_manager::is_initialized() const {
    return initialized_;
}

bool audio_manager::load_bgm(const std::string& file_path) {
    if (!ensure_initialized()) {
        return false;
    }
    replace_voice(bgm_handle_, file_path);
    apply_bgm_volume();
    return is_voice_loaded(bgm_handle_);
}

void audio_manager::play_bgm(bool restart) {
    play_voice(bgm_handle_, restart);
}

void audio_manager::pause_bgm() {
    pause_voice(bgm_handle_);
}

void audio_manager::stop_bgm() {
    stop_voice(bgm_handle_);
}

void audio_manager::set_bgm_volume(float volume) {
    bgm_volume_ = std::clamp(volume, 0.0f, 1.0f);
    apply_bgm_volume();
}

void audio_manager::seek_bgm(double seconds) {
    set_voice_position_seconds(bgm_handle_, seconds);
}

bool audio_manager::is_bgm_loaded() const {
    return is_voice_loaded(bgm_handle_);
}

bool audio_manager::is_bgm_playing() const {
    return is_voice_playing(bgm_handle_);
}

double audio_manager::get_bgm_position_seconds() const {
    return get_voice_position_seconds(bgm_handle_);
}

double audio_manager::get_bgm_length_seconds() const {
    return get_voice_length_seconds(bgm_handle_);
}

audio_clock_snapshot audio_manager::get_bgm_clock() const {
    return get_voice_clock(bgm_handle_);
}

double audio_manager::get_output_latency_seconds() const {
    BASS_INFO info = {};
    if (!initialized_ || BASS_GetInfo(&info) == FALSE) {
        return 0.0;
    }
    return static_cast<double>(info.latency) / 1000.0;
}

double audio_manager::get_output_buffer_seconds() const {
    BASS_INFO info = {};
    if (!initialized_ || BASS_GetInfo(&info) == FALSE) {
        return 0.0;
    }
    return static_cast<double>(info.minbuf) / 1000.0;
}

bool audio_manager::load_preview(const std::string& file_path) {
    if (!ensure_initialized()) {
        return false;
    }
    replace_voice(preview_handle_, file_path);
    apply_preview_volume();
    return is_voice_loaded(preview_handle_);
}

void audio_manager::play_preview(bool restart) {
    play_voice(preview_handle_, restart);
}

void audio_manager::pause_preview() {
    pause_voice(preview_handle_);
}

void audio_manager::stop_preview() {
    stop_voice(preview_handle_);
}

void audio_manager::unload_preview() {
    stop_voice(preview_handle_);
    free_voice(preview_handle_);
}

void audio_manager::set_preview_volume(float volume) {
    preview_volume_ = std::clamp(volume, 0.0f, 1.0f);
    apply_preview_volume();
}

void audio_manager::seek_preview(double seconds) {
    set_voice_position_seconds(preview_handle_, seconds);
}

bool audio_manager::is_preview_loaded() const {
    return is_voice_loaded(preview_handle_);
}

bool audio_manager::is_preview_playing() const {
    return is_voice_playing(preview_handle_);
}

double audio_manager::get_preview_position_seconds() const {
    return get_voice_position_seconds(preview_handle_);
}

double audio_manager::get_preview_length_seconds() const {
    return get_voice_length_seconds(preview_handle_);
}

int audio_manager::play_se(const std::string& file_path, float volume) {
    if (!ensure_initialized()) {
        return 0;
    }

    unsigned long handle = 0;
    bool stream_fallback = false;
    auto sample_it = se_samples().find(file_path);
    if (sample_it == se_samples().end()) {
        const unsigned long sample_handle = BASS_SampleLoad(
            FALSE, file_path.c_str(), 0, 0, kSeSampleMaxVoices, BASS_SAMPLE_OVER_POS);
        if (sample_handle != 0) {
            sample_it = se_samples().emplace(file_path, se_sample_entry{sample_handle}).first;
        }
    }

    if (sample_it != se_samples().end() && sample_it->second.handle != 0) {
        handle = BASS_SampleGetChannel(sample_it->second.handle, FALSE);
    }

    if (handle == 0) {
        handle = create_stream(file_path);
        stream_fallback = true;
    }

    if (!is_voice_loaded(handle)) {
        return 0;
    }

    const int voice_id = next_se_voice_id_++;
    se_voices()[voice_id] = {handle, std::clamp(volume, 0.0f, 1.0f), stream_fallback};
    BASS_ChannelSetAttribute(handle, BASS_ATTRIB_VOL, se_voices()[voice_id].local_volume * se_volume_);
    play_voice(handle, true);
    return voice_id;
}

bool audio_manager::is_se_voice_active(int voice_id) const {
    const auto it = se_voices().find(voice_id);
    if (it == se_voices().end()) {
        return false;
    }

    return is_voice_loaded(it->second.handle) && BASS_ChannelIsActive(it->second.handle) != BASS_ACTIVE_STOPPED;
}

std::size_t audio_manager::get_active_se_voice_count() const {
    return se_voices().size();
}

void audio_manager::stop_se(int voice_id) {
    auto it = se_voices().find(voice_id);
    if (it == se_voices().end()) {
        return;
    }

    stop_voice(it->second.handle);
    if (it->second.stream_fallback) {
        free_voice(it->second.handle);
    }
    se_voices().erase(it);
}

void audio_manager::stop_all_se() {
    for (auto& [voice_id, voice] : se_voices()) {
        (void)voice_id;
        stop_voice(voice.handle);
        if (voice.stream_fallback) {
            free_voice(voice.handle);
        }
    }
    se_voices().clear();
}

void audio_manager::set_se_volume(float volume) {
    se_volume_ = std::clamp(volume, 0.0f, 1.0f);
    for (auto& [voice_id, voice] : se_voices()) {
        (void)voice_id;
        if (is_voice_loaded(voice.handle)) {
            BASS_ChannelSetAttribute(voice.handle, BASS_ATTRIB_VOL, voice.local_volume * se_volume_);
        }
    }
}

void audio_manager::update() {
    for (auto it = se_voices().begin(); it != se_voices().end();) {
        if (!is_voice_loaded(it->second.handle) || BASS_ChannelIsActive(it->second.handle) == BASS_ACTIVE_STOPPED) {
            if (it->second.stream_fallback) {
                free_voice(it->second.handle);
            }
            it = se_voices().erase(it);
        } else {
            ++it;
        }
    }
}

void audio_manager::retain_legacy_client() {
    if (ensure_initialized()) {
        ++legacy_client_count_;
    }
}

void audio_manager::release_legacy_client() {
    legacy_client_count_ = std::max(0, legacy_client_count_ - 1);
}

bool audio_manager::is_voice_loaded(unsigned long handle) {
    return handle != 0;
}

bool audio_manager::is_voice_playing(unsigned long handle) {
    return handle != 0 && BASS_ChannelIsActive(handle) == BASS_ACTIVE_PLAYING;
}

double audio_manager::get_voice_position_seconds(unsigned long handle) {
    if (handle == 0) {
        return 0.0;
    }

    const QWORD position = BASS_ChannelGetPosition(handle, BASS_POS_BYTE);
    return BASS_ChannelBytes2Seconds(handle, position);
}

double audio_manager::get_voice_length_seconds(unsigned long handle) {
    if (handle == 0) {
        return 0.0;
    }

    const QWORD length = BASS_ChannelGetLength(handle, BASS_POS_BYTE);
    return BASS_ChannelBytes2Seconds(handle, length);
}

audio_clock_snapshot audio_manager::get_voice_clock(unsigned long handle) {
    audio_clock_snapshot snapshot;
    snapshot.loaded = is_voice_loaded(handle);
    snapshot.playing = is_voice_playing(handle);
    snapshot.stream_position_seconds = get_voice_position_seconds(handle);

    BASS_INFO info = {};
    if (BASS_GetInfo(&info) != FALSE) {
        snapshot.device_latency_seconds = static_cast<double>(info.latency) / 1000.0;
        snapshot.device_buffer_seconds = static_cast<double>(info.minbuf) / 1000.0;
    }

    snapshot.audio_time_seconds = snapshot.stream_position_seconds + snapshot.device_latency_seconds;
    return snapshot;
}

void audio_manager::play_voice(unsigned long handle, bool restart) {
    if (handle != 0) {
        BASS_ChannelPlay(handle, restart ? TRUE : FALSE);
    }
}

void audio_manager::pause_voice(unsigned long handle) {
    if (handle != 0) {
        BASS_ChannelPause(handle);
    }
}

void audio_manager::stop_voice(unsigned long handle) {
    if (handle != 0) {
        BASS_ChannelStop(handle);
    }
}

void audio_manager::set_voice_position_seconds(unsigned long handle, double seconds) {
    if (handle != 0) {
        const QWORD position = BASS_ChannelSeconds2Bytes(handle, std::max(0.0, seconds));
        BASS_ChannelSetPosition(handle, position, BASS_POS_BYTE);
    }
}

void audio_manager::free_voice(unsigned long& handle) {
    if (handle != 0) {
        BASS_StreamFree(handle);
        handle = 0;
    }
}

bool audio_manager::ensure_initialized() {
    return initialize();
}

unsigned long audio_manager::create_stream(const std::string& file_path) const {
    return BASS_StreamCreateFile(FALSE, file_path.c_str(), 0, 0, 0);
}

void audio_manager::replace_voice(unsigned long& handle, const std::string& file_path) const {
    free_voice(handle);
    handle = create_stream(file_path);
}

void audio_manager::apply_bgm_volume() const {
    if (is_voice_loaded(bgm_handle_)) {
        BASS_ChannelSetAttribute(bgm_handle_, BASS_ATTRIB_VOL, bgm_volume_);
    }
}

void audio_manager::apply_preview_volume() const {
    if (is_voice_loaded(preview_handle_)) {
        BASS_ChannelSetAttribute(preview_handle_, BASS_ATTRIB_VOL, preview_volume_);
    }
}
