#include "audio_manager.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bass_path.h"
#include "bass.h"

namespace {
constexpr unsigned long kLowLatencyUpdatePeriodMs = 5;
constexpr unsigned long kLowLatencyDeviceBufferMs = 10;
constexpr unsigned long kSeSampleMaxVoices = 16;
constexpr std::size_t kSeSampleCacheLimit = 8;

struct se_sample_entry {
    unsigned long handle = 0;
    std::uint64_t last_use_generation = 0;
    bool pinned = false;
};

struct se_voice_entry {
    unsigned long handle = 0;
    float local_volume = 1.0f;
    float pan = 0.0f;
    bool stream_fallback = false;
    std::string sample_path;
};

struct loudness_cache_entry {
    std::uintmax_t file_size = 0;
    std::filesystem::file_time_type last_write_time = {};
    audio_loudness_analysis analysis;
};

std::unordered_map<std::string, se_sample_entry>& se_samples() {
    static std::unordered_map<std::string, se_sample_entry> samples;
    return samples;
}

std::unordered_map<int, se_voice_entry>& se_voices() {
    static std::unordered_map<int, se_voice_entry> voices;
    return voices;
}

std::uint64_t& se_sample_generation() {
    static std::uint64_t generation = 0;
    return generation;
}

std::unordered_map<std::string, loudness_cache_entry>& loudness_cache() {
    static std::unordered_map<std::string, loudness_cache_entry> cache;
    return cache;
}

std::mutex& loudness_cache_mutex() {
    static std::mutex mutex;
    return mutex;
}

std::optional<loudness_cache_entry> make_loudness_cache_probe(const std::string& file_path) {
    std::error_code ec;
    const std::filesystem::path path = bass_path::filesystem_path(file_path);
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return std::nullopt;
    }

    loudness_cache_entry entry;
    entry.file_size = std::filesystem::file_size(path, ec);
    if (ec) {
        return std::nullopt;
    }
    entry.last_write_time = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return std::nullopt;
    }
    return entry;
}

unsigned long create_stream_from_path(const std::string& file_path) {
    if (bass_path::is_remote_stream_url(file_path)) {
        return bass_path::create_url_stream(file_path, BASS_STREAM_PRESCAN);
    }
    return bass_path::create_file_stream(file_path);
}

unsigned long create_stream_from_memory_bytes(const std::vector<unsigned char>& bytes, DWORD flags = 0) {
    if (bytes.empty()) {
        return 0;
    }
    return BASS_StreamCreateFile(TRUE, bytes.data(), 0, static_cast<QWORD>(bytes.size()), flags);
}

bool is_pinned_se_sample_path(const std::string& file_path) {
    return bass_path::lowercase_ascii(bass_path::filesystem_path(file_path).filename().string()) == "hitsound.mp3";
}

se_sample_entry* find_or_load_se_sample(const std::string& file_path) {
    auto sample_it = se_samples().find(file_path);
    if (sample_it == se_samples().end()) {
        const unsigned long sample_handle =
            bass_path::load_sample(file_path, kSeSampleMaxVoices, BASS_SAMPLE_OVER_POS);
        if (sample_handle == 0) {
            return nullptr;
        }

        sample_it = se_samples().emplace(file_path, se_sample_entry{
            sample_handle,
            ++se_sample_generation(),
            is_pinned_se_sample_path(file_path),
        }).first;
    }

    sample_it->second.last_use_generation = ++se_sample_generation();
    return &sample_it->second;
}

bool has_active_sample_voice(const std::string& sample_path) {
    for (const auto& [voice_id, voice] : se_voices()) {
        (void)voice_id;
        if (voice.stream_fallback || voice.sample_path != sample_path || voice.handle == 0) {
            continue;
        }

        if (BASS_ChannelIsActive(voice.handle) != BASS_ACTIVE_STOPPED) {
            return true;
        }
    }
    return false;
}

void trim_se_sample_cache() {
    while (se_samples().size() > kSeSampleCacheLimit) {
        auto eviction_it = se_samples().end();
        for (auto it = se_samples().begin(); it != se_samples().end(); ++it) {
            if (it->second.handle == 0 || it->second.pinned || has_active_sample_voice(it->first)) {
                continue;
            }

            if (eviction_it == se_samples().end() ||
                it->second.last_use_generation < eviction_it->second.last_use_generation) {
                eviction_it = it;
            }
        }

        if (eviction_it == se_samples().end()) {
            break;
        }

        BASS_SampleFree(eviction_it->second.handle);
        se_samples().erase(eviction_it);
    }
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
    if (owner_thread_set_ && !is_owner_thread()) {
        return false;
    }
    if (!owner_thread_set_) {
        owner_thread_id_ = std::this_thread::get_id();
        owner_thread_set_ = true;
    }
    if (initialized_) {
        return true;
    }

    BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, kLowLatencyUpdatePeriodMs);
    BASS_SetConfig(BASS_CONFIG_DEV_BUFFER, kLowLatencyDeviceBufferMs);
    initialized_ = BASS_Init(-1, 44100, 0, nullptr, nullptr) != FALSE;
    return initialized_;
}

void audio_manager::shutdown() {
    if (owner_thread_set_ && !is_owner_thread()) {
        return;
    }
    stop_all_se();
    free_se_samples();
    if (preview_loading_ && preview_load_future_.valid()) {
        preview_load_future_.wait();
        const preview_load_payload loaded = preview_load_future_.get();
        if (loaded.handle != 0) {
            BASS_StreamFree(loaded.handle);
        }
    }
    for (std::future<preview_load_payload>& stale_future : stale_preview_load_futures_) {
        if (!stale_future.valid()) {
            continue;
        }
        stale_future.wait();
        const preview_load_payload loaded = stale_future.get();
        if (loaded.handle != 0) {
            BASS_StreamFree(loaded.handle);
        }
    }
    stale_preview_load_futures_.clear();
    preview_loading_ = false;
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
    reset_bgm_fade();
    bgm_memory_.clear();
    replace_voice(bgm_handle_, file_path);
    bgm_loudness_ = analyze_or_get_cached_loudness(file_path);
    apply_bgm_volume();
    return is_voice_loaded(bgm_handle_);
}

bool audio_manager::load_bgm_from_memory(std::vector<unsigned char> bytes) {
    if (!ensure_initialized()) {
        return false;
    }
    reset_bgm_fade();
    free_voice(bgm_handle_);
    bgm_memory_ = std::move(bytes);
    bgm_handle_ = create_stream_from_memory(bgm_memory_);
    bgm_loudness_ = {};
    apply_bgm_volume();
    return is_voice_loaded(bgm_handle_);
}

void audio_manager::play_bgm(bool restart) {
    apply_bgm_volume();
    play_voice(bgm_handle_, restart);
}

void audio_manager::pause_bgm() {
    pause_voice(bgm_handle_);
}

void audio_manager::fade_out_bgm(unsigned int duration_ms) {
    if (!is_voice_loaded(bgm_handle_)) {
        return;
    }
    bgm_fade_.start_gain = bgm_fade_.gain;
    bgm_fade_.target_gain = 0.0f;
    bgm_fade_.started_at = std::chrono::steady_clock::now();
    bgm_fade_.duration = std::chrono::milliseconds(duration_ms);
    bgm_fade_.active = duration_ms > 0;
    if (!bgm_fade_.active) {
        bgm_fade_.gain = bgm_fade_.target_gain;
    }
    apply_bgm_volume();
}

void audio_manager::stop_bgm() {
    stop_voice(bgm_handle_);
    reset_bgm_fade();
}

void audio_manager::set_bgm_volume(float volume) {
    bgm_volume_ = std::clamp(volume, 0.0f, 1.0f);
    apply_bgm_volume();
}

void audio_manager::set_bgm_fade_gain(float gain) {
    bgm_fade_.active = false;
    bgm_fade_.gain = std::clamp(gain, 0.0f, 1.0f);
    bgm_fade_.start_gain = bgm_fade_.gain;
    bgm_fade_.target_gain = bgm_fade_.gain;
    apply_bgm_volume();
}

void audio_manager::set_loudness_normalization_enabled(bool enabled) {
    loudness_normalization_enabled_ = enabled;
    apply_bgm_volume();
    apply_preview_volume();
}

bool audio_manager::is_loudness_normalization_enabled() const {
    return loudness_normalization_enabled_;
}

audio_loudness_analysis audio_manager::get_bgm_loudness_analysis() const {
    return bgm_loudness_;
}

audio_loudness_analysis audio_manager::get_preview_loudness_analysis() const {
    return preview_loudness_;
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

bool audio_manager::get_bgm_fft256(std::array<float, 128>& spectrum) const {
    spectrum.fill(0.0f);
    if (!is_voice_playing(bgm_handle_)) {
        return false;
    }

    return BASS_ChannelGetData(bgm_handle_, spectrum.data(),
                               BASS_DATA_FFT256 | BASS_DATA_FFT_REMOVEDC) != static_cast<DWORD>(-1);
}

bool audio_manager::get_bgm_fft1024(std::array<float, 512>& spectrum) const {
    spectrum.fill(0.0f);
    if (!is_voice_playing(bgm_handle_)) {
        return false;
    }

    return BASS_ChannelGetData(bgm_handle_, spectrum.data(),
                               BASS_DATA_FFT1024 | BASS_DATA_FFT_REMOVEDC) != static_cast<DWORD>(-1);
}

bool audio_manager::get_bgm_fft4096(std::array<float, 2048>& spectrum) const {
    spectrum.fill(0.0f);
    if (!is_voice_playing(bgm_handle_)) {
        return false;
    }

    return BASS_ChannelGetData(bgm_handle_, spectrum.data(),
                               BASS_DATA_FFT4096 | BASS_DATA_FFT_REMOVEDC) != static_cast<DWORD>(-1);
}

bool audio_manager::get_bgm_oscilloscope256(std::array<float, 256>& samples) const {
    samples.fill(0.0f);
    if (!is_voice_loaded(bgm_handle_)) {
        return false;
    }

    BASS_CHANNELINFO info = {};
    if (BASS_ChannelGetInfo(bgm_handle_, &info) == FALSE || info.chans <= 0) {
        return false;
    }

    const std::size_t channels = static_cast<std::size_t>(info.chans);
    std::vector<float> interleaved(samples.size() * channels, 0.0f);
    const DWORD requested_bytes =
        static_cast<DWORD>(interleaved.size() * sizeof(float)) | BASS_DATA_FLOAT;
    const DWORD bytes_read = BASS_ChannelGetData(bgm_handle_, interleaved.data(), requested_bytes);
    if (bytes_read == static_cast<DWORD>(-1) || bytes_read == 0) {
        return false;
    }

    const std::size_t samples_read = bytes_read / sizeof(float);
    const std::size_t frames_read = std::min(samples.size(), samples_read / channels);
    if (frames_read == 0) {
        return false;
    }

    for (std::size_t frame = 0; frame < frames_read; ++frame) {
        float mono = 0.0f;
        const std::size_t base = frame * channels;
        for (std::size_t channel = 0; channel < channels; ++channel) {
            mono += interleaved[base + channel];
        }
        samples[frame] = std::clamp(mono / static_cast<float>(channels), -1.0f, 1.0f);
    }
    return true;
}

double audio_manager::get_bgm_sample_rate_hz() const {
    return get_voice_sample_rate_hz(bgm_handle_);
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
    ++preview_load_generation_;
    active_preview_load_generation_ = preview_load_generation_;
    if (preview_load_future_.valid()) {
        stale_preview_load_futures_.push_back(std::move(preview_load_future_));
    }
    preview_loading_ = false;
    reset_preview_fade();
    preview_memory_.clear();
    replace_voice(preview_handle_, file_path);
    preview_path_ = file_path;
    preview_loudness_ = analyze_or_get_cached_loudness(file_path);
    apply_preview_volume();
    return is_voice_loaded(preview_handle_);
}

bool audio_manager::load_preview_from_memory(std::vector<unsigned char> bytes) {
    if (!ensure_initialized()) {
        return false;
    }
    ++preview_load_generation_;
    active_preview_load_generation_ = preview_load_generation_;
    if (preview_load_future_.valid()) {
        stale_preview_load_futures_.push_back(std::move(preview_load_future_));
    }
    preview_loading_ = false;
    reset_preview_fade();
    free_voice(preview_handle_);
    preview_memory_ = std::move(bytes);
    preview_handle_ = create_stream_from_memory(preview_memory_);
    preview_path_.clear();
    preview_loudness_ = {};
    apply_preview_volume();
    return is_voice_loaded(preview_handle_);
}

bool audio_manager::request_preview_load(const std::string& file_path) {
    if (!ensure_initialized()) {
        return false;
    }

    ++preview_load_generation_;
    active_preview_load_generation_ = preview_load_generation_;
    if (preview_load_future_.valid()) {
        stale_preview_load_futures_.push_back(std::move(preview_load_future_));
    }

    reset_preview_fade();
    preview_loading_ = true;
    const std::string source = file_path;
    preview_path_ = file_path;
    preview_loudness_ = {};
    const unsigned int generation = active_preview_load_generation_;
    std::promise<preview_load_payload> promise;
    preview_load_future_ = promise.get_future();
    promise.set_value({generation, 0, source, {}});
    return true;
}

bool audio_manager::request_preview_load_from_memory(std::vector<unsigned char> bytes) {
    if (!ensure_initialized() || bytes.empty()) {
        return false;
    }

    ++preview_load_generation_;
    active_preview_load_generation_ = preview_load_generation_;
    if (preview_load_future_.valid()) {
        stale_preview_load_futures_.push_back(std::move(preview_load_future_));
    }

    reset_preview_fade();
    preview_loading_ = true;
    preview_path_.clear();
    preview_loudness_ = {};
    const unsigned int generation = active_preview_load_generation_;
    std::promise<preview_load_payload> promise;
    preview_load_future_ = promise.get_future();
    promise.set_value({generation, 0, {}, std::move(bytes)});
    return true;
}

audio_manager::async_preview_load_result audio_manager::poll_preview_load() {
    async_preview_load_result result;
    for (auto it = stale_preview_load_futures_.begin(); it != stale_preview_load_futures_.end();) {
        if (!it->valid() || it->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            if (it->valid()) {
                const preview_load_payload stale = it->get();
                if (stale.handle != 0) {
                    BASS_StreamFree(stale.handle);
                }
            }
            it = stale_preview_load_futures_.erase(it);
        } else {
            ++it;
        }
    }

    if (!preview_loading_ || !preview_load_future_.valid()) {
        return result;
    }
    if (preview_load_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return result;
    }

    result.completed = true;
    preview_loading_ = false;
    preview_load_payload loaded;
    try {
        loaded = preview_load_future_.get();
    } catch (...) {
        loaded = {};
    }

    if (loaded.generation != preview_load_generation_) {
        if (loaded.handle != 0) {
            BASS_StreamFree(loaded.handle);
        }
        return result;
    }

    if (loaded.handle == 0) {
        if (!loaded.source_path.empty()) {
            loaded.handle = create_stream(loaded.source_path);
        } else if (!loaded.memory.empty()) {
            loaded.handle = create_stream_from_memory(loaded.memory);
        }
    }

    free_voice(preview_handle_);
    preview_handle_ = loaded.handle;
    preview_memory_ = std::move(loaded.memory);
    preview_loudness_ = {};
    reset_preview_fade();
    apply_preview_volume();
    result.loaded = is_voice_loaded(preview_handle_);
    return result;
}

void audio_manager::cancel_preview_load_request() {
    ++preview_load_generation_;
    active_preview_load_generation_ = preview_load_generation_;
    if (preview_load_future_.valid()) {
        stale_preview_load_futures_.push_back(std::move(preview_load_future_));
    }
    preview_loading_ = false;
}

bool audio_manager::is_preview_loading() const {
    return preview_loading_;
}

void audio_manager::play_preview(bool restart) {
    apply_preview_volume();
    play_voice(preview_handle_, restart);
}

void audio_manager::pause_preview() {
    pause_voice(preview_handle_);
}

void audio_manager::stop_preview() {
    stop_voice(preview_handle_);
    reset_preview_fade();
}

void audio_manager::unload_preview() {
    ++preview_load_generation_;
    active_preview_load_generation_ = preview_load_generation_;
    if (preview_load_future_.valid()) {
        stale_preview_load_futures_.push_back(std::move(preview_load_future_));
    }
    preview_loading_ = false;
    stop_voice(preview_handle_);
    free_voice(preview_handle_);
    preview_memory_.clear();
    reset_preview_fade();
    preview_path_.clear();
    preview_loudness_ = {};
}

void audio_manager::set_preview_volume(float volume) {
    preview_volume_ = std::clamp(volume, 0.0f, 1.0f);
    apply_preview_volume();
}

void audio_manager::set_preview_fade_gain(float gain) {
    preview_fade_.active = false;
    preview_fade_.gain = std::clamp(gain, 0.0f, 1.0f);
    preview_fade_.start_gain = preview_fade_.gain;
    preview_fade_.target_gain = preview_fade_.gain;
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

bool audio_manager::get_preview_fft256(std::array<float, 128>& spectrum) const {
    spectrum.fill(0.0f);
    if (!is_voice_playing(preview_handle_)) {
        return false;
    }

    return BASS_ChannelGetData(preview_handle_, spectrum.data(),
                               BASS_DATA_FFT256 | BASS_DATA_FFT_REMOVEDC) != static_cast<DWORD>(-1);
}

bool audio_manager::get_preview_fft1024(std::array<float, 512>& spectrum) const {
    spectrum.fill(0.0f);
    if (!is_voice_playing(preview_handle_)) {
        return false;
    }

    return BASS_ChannelGetData(preview_handle_, spectrum.data(),
                               BASS_DATA_FFT1024 | BASS_DATA_FFT_REMOVEDC) != static_cast<DWORD>(-1);
}

bool audio_manager::get_preview_fft4096(std::array<float, 2048>& spectrum) const {
    spectrum.fill(0.0f);
    if (!is_voice_playing(preview_handle_)) {
        return false;
    }

    return BASS_ChannelGetData(preview_handle_, spectrum.data(),
                               BASS_DATA_FFT4096 | BASS_DATA_FFT_REMOVEDC) != static_cast<DWORD>(-1);
}

bool audio_manager::get_preview_oscilloscope256(std::array<float, 256>& samples) const {
    samples.fill(0.0f);
    if (!is_voice_playing(preview_handle_)) {
        return false;
    }

    BASS_CHANNELINFO info = {};
    if (BASS_ChannelGetInfo(preview_handle_, &info) == FALSE || info.chans <= 0) {
        return false;
    }

    const std::size_t channels = static_cast<std::size_t>(info.chans);
    std::vector<float> interleaved(samples.size() * channels, 0.0f);
    const DWORD requested_bytes =
        static_cast<DWORD>(interleaved.size() * sizeof(float)) | BASS_DATA_FLOAT;
    const DWORD bytes_read = BASS_ChannelGetData(preview_handle_, interleaved.data(), requested_bytes);
    if (bytes_read == static_cast<DWORD>(-1) || bytes_read == 0) {
        return false;
    }

    const std::size_t samples_read = bytes_read / sizeof(float);
    const std::size_t frames_read = std::min(samples.size(), samples_read / channels);
    if (frames_read == 0) {
        return false;
    }

    for (std::size_t frame = 0; frame < frames_read; ++frame) {
        float mono = 0.0f;
        const std::size_t base = frame * channels;
        for (std::size_t channel = 0; channel < channels; ++channel) {
            mono += interleaved[base + channel];
        }
        samples[frame] = std::clamp(mono / static_cast<float>(channels), -1.0f, 1.0f);
    }
    return true;
}

double audio_manager::get_preview_sample_rate_hz() const {
    return get_voice_sample_rate_hz(preview_handle_);
}

int audio_manager::play_se(const std::string& file_path, float volume, float pan) {
    if (!ensure_initialized()) {
        return 0;
    }

    unsigned long handle = 0;
    bool stream_fallback = false;
    if (se_sample_entry* sample = find_or_load_se_sample(file_path); sample != nullptr && sample->handle != 0) {
        handle = BASS_SampleGetChannel(sample->handle, FALSE);
    }

    if (handle == 0) {
        handle = create_stream(file_path);
        stream_fallback = true;
    }

    if (!is_voice_loaded(handle)) {
        return 0;
    }

    const int voice_id = next_se_voice_id_++;
    se_voices()[voice_id] = {handle, std::clamp(volume, 0.0f, 1.0f), std::clamp(pan, -1.0f, 1.0f), stream_fallback,
                             stream_fallback ? std::string{} : file_path};
    BASS_ChannelSetAttribute(handle, BASS_ATTRIB_NOBUFFER, 1.0f);
    BASS_ChannelSetAttribute(handle, BASS_ATTRIB_VOL, se_voices()[voice_id].local_volume * se_volume_);
    BASS_ChannelSetAttribute(handle, BASS_ATTRIB_PAN, se_voices()[voice_id].pan);
    play_voice(handle, true);
    trim_se_sample_cache();
    return voice_id;
}

bool audio_manager::preload_se(const std::string& file_path) {
    if (!ensure_initialized()) {
        return false;
    }

    if (find_or_load_se_sample(file_path) == nullptr) {
        return false;
    }

    trim_se_sample_cache();
    return true;
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
    if (update_fade(bgm_fade_)) {
        apply_bgm_volume();
    }
    if (update_fade(preview_fade_)) {
        apply_preview_volume();
    }

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
    if (position == static_cast<QWORD>(-1)) {
        return 0.0;
    }
    return BASS_ChannelBytes2Seconds(handle, position);
}

double audio_manager::get_voice_length_seconds(unsigned long handle) {
    if (handle == 0) {
        return 0.0;
    }

    const QWORD length = BASS_ChannelGetLength(handle, BASS_POS_BYTE);
    if (length == static_cast<QWORD>(-1)) {
        return 0.0;
    }
    return BASS_ChannelBytes2Seconds(handle, length);
}

double audio_manager::get_voice_sample_rate_hz(unsigned long handle) {
    if (handle == 0) {
        return 44100.0;
    }

    BASS_CHANNELINFO info = {};
    if (BASS_ChannelGetInfo(handle, &info) == FALSE || info.freq == 0) {
        return 44100.0;
    }
    return static_cast<double>(info.freq);
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
    if (owner_thread_set_ && !is_owner_thread()) {
        return false;
    }
    return initialize();
}

bool audio_manager::is_owner_thread() const {
    return !owner_thread_set_ || owner_thread_id_ == std::this_thread::get_id();
}

unsigned long audio_manager::create_stream(const std::string& file_path) const {
    return create_stream_from_path(file_path);
}

unsigned long audio_manager::create_stream_from_memory(const std::vector<unsigned char>& bytes) const {
    return create_stream_from_memory_bytes(bytes);
}

void audio_manager::replace_voice(unsigned long& handle, const std::string& file_path) const {
    free_voice(handle);
    handle = create_stream(file_path);
}

audio_loudness_analysis audio_manager::analyze_or_get_cached_loudness(const std::string& file_path) const {
    const std::optional<loudness_cache_entry> probe = make_loudness_cache_probe(file_path);
    if (!probe.has_value()) {
        return {};
    }

    {
        std::scoped_lock lock(loudness_cache_mutex());
        const auto it = loudness_cache().find(file_path);
        if (it != loudness_cache().end() &&
            it->second.file_size == probe->file_size &&
            it->second.last_write_time == probe->last_write_time) {
            return it->second.analysis;
        }
    }

    loudness_cache_entry entry = *probe;
    entry.analysis = analyze_audio_loudness(file_path);

    std::scoped_lock lock(loudness_cache_mutex());
    loudness_cache()[file_path] = entry;
    return entry.analysis;
}

void audio_manager::reset_bgm_fade() {
    bgm_fade_ = {};
}

void audio_manager::reset_preview_fade() {
    preview_fade_ = {};
}

bool audio_manager::update_fade(volume_fade_state& fade) {
    if (!fade.active) {
        return false;
    }

    if (fade.duration.count() <= 0) {
        fade.gain = fade.target_gain;
        fade.active = false;
        return true;
    }

    const auto elapsed = std::chrono::steady_clock::now() - fade.started_at;
    const float t = std::clamp(
        static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) /
            static_cast<float>(fade.duration.count()),
        0.0f,
        1.0f);
    fade.gain = fade.start_gain + (fade.target_gain - fade.start_gain) * t;
    if (t >= 1.0f) {
        fade.gain = fade.target_gain;
        fade.active = false;
    }
    return true;
}

void audio_manager::apply_bgm_volume() const {
    if (is_voice_loaded(bgm_handle_)) {
        const float normalized_gain =
            loudness_normalization_enabled_ && bgm_loudness_.valid ? bgm_loudness_.linear_gain : 1.0f;
        BASS_ChannelSetAttribute(bgm_handle_, BASS_ATTRIB_VOL,
                                 std::clamp(bgm_volume_ * bgm_fade_.gain * normalized_gain, 0.0f, 4.0f));
    }
}

void audio_manager::apply_preview_volume() const {
    if (is_voice_loaded(preview_handle_)) {
        const float normalized_gain =
            loudness_normalization_enabled_ && preview_loudness_.valid ? preview_loudness_.linear_gain : 1.0f;
        BASS_ChannelSetAttribute(preview_handle_, BASS_ATTRIB_VOL,
                                 std::clamp(preview_volume_ * preview_fade_.gain * normalized_gain, 0.0f, 4.0f));
    }
}
