#pragma once

#include <string>

#include "raylib.h"
#include "title/online_catalog_data_controller.h"
#include "title/online_download_view.h"
#include "title/title_online_mode_controller.h"

class title_browse_feature {
public:
    struct update_callbacks {
        title_online_mode_controller::callbacks online;
    };

    struct poll_result {
        bool downloaded_content = false;
        std::string downloaded_song_id;
        bool catalog_refreshed = false;
        bool select_preview_song = false;
    };

    [[nodiscard]] title_online_view::state& state();
    [[nodiscard]] const title_online_view::state& state() const;

    void on_enter(title_audio_controller& audio_controller);
    void on_exit();

    void request_reload(bool preserve_view = false);
    void select_local_update_target(const std::string& local_song_id,
                                    const std::string& local_chart_id,
                                    bool open_detail);
    void mark_song_removed(const std::string& song_id);
    void start_chart_download_by_remote_id(const std::string& remote_song_id,
                                           const std::string& remote_chart_id);

    [[nodiscard]] const song_select::song_entry* preview_song() const;
    [[nodiscard]] std::string selected_song_id() const;

    void update(float anim_t, Rectangle origin_rect, float dt, title_audio_controller& audio_controller, const update_callbacks& callbacks);
    poll_result poll(bool active);
    void draw(const title_audio_controller& audio_controller, float anim_t, Rectangle origin_rect);

private:
    title_online_view::state state_;
    online_catalog::data_controller data_controller_;
};
