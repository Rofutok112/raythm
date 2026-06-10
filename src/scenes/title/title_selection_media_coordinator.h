#pragma once

#include <string>

#include "ranking_service.h"
#include "song_select/selection_key.h"
#include "song_select/song_select_ranking_loader.h"
#include "title/title_audio_controller.h"

namespace song_select {
struct state;
}

struct title_selection_media_snapshot {
    song_select::selection_key key;
    title_preview_snapshot preview;
    song_select::ranking_load_controller::snapshot ranking;
};

class title_selection_media_coordinator {
public:
    enum class context {
        none,
        play,
        create,
    };

    title_selection_media_coordinator();

    void reset();
    void reset(song_select::state& state);
    void sync_current(song_select::state& state,
                      title_audio_controller& audio_controller,
                      context active_context,
                      bool force = false);
    void request_ranking_reload(song_select::state& state);
    void poll_ranking_reload(song_select::state& state);
    [[nodiscard]] title_selection_media_snapshot media_snapshot(
        const song_select::state& state,
        const title_audio_controller& audio_controller) const;

private:
    static song_select::ranking_load_request ranking_request_for(const song_select::state& state,
                                                                 const song_select::selection_key& key);
    static ranking_service::listing load_ranking_from_service(std::string chart_id,
                                                              ranking_service::source source,
                                                              int limit);
    void sync_ranking(song_select::state& state,
                      const song_select::selection_key& key,
                      bool force);
    void apply_ranking_request_started(song_select::state& state,
                                       const song_select::ranking_load_request& request) const;
    void apply_ranking_loaded(song_select::state& state,
                              song_select::ranking_load_data loaded) const;
    void mark_online_loading(song_select::state& state,
                             ranking_service::source source) const;
    void reset_ranking_panel_scroll(song_select::state& state) const;

    song_select::ranking_load_controller ranking_controller_;
    song_select::selection_key audio_key_;
    song_select::selection_key jacket_key_;
    song_select::selection_key ranking_key_;
    song_select::selection_key current_key_;
    bool audio_synced_ = false;
    bool jacket_synced_ = false;
    bool ranking_synced_ = false;
};
