#pragma once

#include <chrono>
#include <future>
#include <optional>
#include <string>

#include "data_models.h"
#include "managed_content_storage.h"
#include "song_select/selection_key.h"
#include "song_select/song_select_state.h"

namespace song_select {

class preview_audio_loader {
public:
    enum class load_status {
        idle,
        loading,
        ready,
        failed,
    };

    struct snapshot {
        load_status status = load_status::idle;
        std::optional<selection_key> key;
    };

    struct load_event {
        enum class status {
            none,
            loaded,
            failed,
        };

        status result = status::none;
        std::optional<song_data> song;
    };

    [[nodiscard]] const selection_key& target_key() const;
    [[nodiscard]] const std::string& target_song_id() const;
    [[nodiscard]] bool loading_or_preparing() const;
    [[nodiscard]] load_status status() const;
    [[nodiscard]] snapshot current() const;

    bool request(const selection_key& key, const song_entry& song);
    bool request(const song_entry& song);
    load_event update(const selection_key& current_key, const song_entry* selected_song);
    load_event update(const song_entry* selected_song);
    void reset();

private:
    [[nodiscard]] bool selected_key_matches_target(const selection_key& current_key,
                                                   const song_entry* selected_song) const;
    bool request_prepared_audio(std::vector<unsigned char> bytes);
    bool request_path_audio(const std::string& audio_source);
    void clear_prepared_audio_state();
    load_event poll_managed_audio_prepare(const song_entry* selected_song);

    selection_key target_key_;
    std::string pending_managed_audio_path_;
    std::future<managed_content_storage::managed_file_read_result> managed_audio_future_;
    std::optional<song_data> load_song_;
    std::chrono::steady_clock::time_point load_started_at_{};
    load_status status_ = load_status::idle;
    bool managed_audio_pending_ = false;
    bool audio_load_pending_ = false;
};

}  // namespace song_select
