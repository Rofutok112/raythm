#pragma once

#include <functional>
#include <string>
#include <vector>

#include "song_select/song_preview_controller.h"
#include "song_select/song_catalog_service.h"
#include "song_select/song_select_state.h"
#include "song_select/song_transfer_controller.h"

class title_play_transfer_controller {
public:
    struct catalog_callbacks {
        std::function<void(const std::string&, const std::string&)> set_preferred_selection;
        std::function<void()> stop_preview;
        std::function<void(const std::string&)> mark_online_song_removed;
        std::function<void()> reload_online_catalog;
        std::function<void(const std::string&, const std::string&, bool)> request_play_catalog_reload;
    };

    void on_exit();

    [[nodiscard]] bool busy() const;
    [[nodiscard]] const std::string& busy_label() const;

    void cancel_confirmation(song_select::state& state);
    void poll(song_select::state& state, const catalog_callbacks& callbacks, bool sync_media_on_reload);
    void start_song_import(song_select::state& state);
    void start_chart_import(song_select::state& state,
                            const catalog_callbacks& callbacks,
                            bool sync_media_on_reload);
    void start_song_export(song_select::state& state);
    void start_chart_export(song_select::state& state,
                            const catalog_callbacks& callbacks,
                            bool sync_media_on_reload);
    void draw_or_apply_confirmation(song_select::state& state,
                                    song_select::preview_controller& preview_controller,
                                    const catalog_callbacks& callbacks,
                                    bool sync_media_on_reload);

private:
    void apply_delete_result(song_select::state& state,
                             const song_select::delete_result& result,
                             const catalog_callbacks& callbacks,
                             bool sync_media_on_reload);
    void apply_transfer_result(song_select::state& state,
                               const song_select::transfer_result& result,
                               const catalog_callbacks& callbacks,
                               bool sync_media_on_reload);
    void open_overwrite_song_confirmation(song_select::state& state,
                                          std::vector<song_select::song_import_request> requests);
    void open_overwrite_chart_confirmation(song_select::state& state,
                                           std::vector<song_select::chart_import_request> requests);

    song_select::transfer::controller transfer_controller_;
};
