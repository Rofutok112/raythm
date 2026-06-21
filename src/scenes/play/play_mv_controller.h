#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "mv/composition/mv_composition.h"
#include "mv/mv_storage.h"
#include "play/play_session_types.h"
#include "raylib.h"

class play_mv_controller final {
public:
    play_mv_controller();
    ~play_mv_controller();

    play_mv_controller(const play_mv_controller&) = delete;
    play_mv_controller& operator=(const play_mv_controller&) = delete;

    void load_for_song(const std::optional<song_data>& song);
    void reset();
    void notify_song_visual_event(const std::string& event_name, double event_time_ms);
    void draw(const play_session_state& state, double visual_time_ms);

private:
    const mv::composition::asset_ref* find_asset(const std::string& asset_id) const;
    const Texture2D* texture_for_asset(const mv::composition::asset_ref& asset);
    void unload_asset_textures();

    std::optional<mv::mv_package> package_;
    std::optional<mv::composition::mv_composition> composition_;
    std::optional<double> previous_visual_time_ms_;
    std::unordered_map<std::string, Texture2D> asset_textures_;
};
