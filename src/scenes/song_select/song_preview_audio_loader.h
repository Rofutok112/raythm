#pragma once

#include <future>
#include <optional>
#include <string>

#include "data_models.h"
#include "managed_content_storage.h"
#include "song_select/song_select_state.h"

namespace song_select {

class preview_audio_loader {
public:
    struct load_event {
        enum class status {
            none,
            loaded,
            failed,
        };

        status result = status::none;
        std::optional<song_data> song;
    };

    [[nodiscard]] const std::string& target_song_id() const;
    [[nodiscard]] bool loading_or_preparing() const;

    bool request(const song_entry& song);
    load_event update(const song_entry* selected_song);
    void reset();

private:
    bool request_prepared_audio(std::vector<unsigned char> bytes);
    bool request_path_audio(const std::string& audio_source);
    void clear_prepared_audio_state();
    load_event poll_managed_audio_prepare(const song_entry* selected_song);

    std::string target_song_id_;
    std::string pending_managed_audio_path_;
    std::future<managed_content_storage::managed_file_read_result> managed_audio_future_;
    std::optional<song_data> load_song_;
    bool managed_audio_pending_ = false;
    bool audio_load_pending_ = false;
};

}  // namespace song_select
