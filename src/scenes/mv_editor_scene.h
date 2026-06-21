#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "data_models.h"
#include "mv/composition/mv_composition.h"
#include "mv/composition/mv_composition_edit_history.h"
#include "mv/mv_storage.h"
#include "raylib.h"
#include "scene.h"
#include "ui_text_input.h"

// Song-scoped MV composition editor.
class mv_editor_scene final : public scene {
public:
    mv_editor_scene(scene_manager& manager, song_data song);

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    enum class timeline_drag_mode {
        none,
        move,
        trim_start,
        trim_end,
    };

    void validate_composition();
    void save_mv();
    void commit_history(const std::string& label);
    void update_dirty_from_history();
    void apply_history_snapshot(const mv::composition::edit_snapshot& snapshot);
    bool undo_edit();
    bool redo_edit();
    void reset_inspector_inputs();
    void sync_inspector_inputs(const mv::composition::layer& layer);
    bool inspector_text_input_active() const;
    void add_text_layer();
    void add_rect_layer();
    void add_image_layer();
    void add_beat_grid_layer();
    void add_waveform_layer();
    void add_spectrum_layer();
    void apply_builtin_preset(const std::string& preset_id);
    void key_selected_transform();
    void delete_transform_keyframes_at_playhead();
    int transform_keyframe_count_at_playhead() const;
    void add_flash_event_trigger_at_playhead();
    void add_show_event_trigger_at_playhead();
    void add_text_event_trigger_at_playhead();
    void clear_event_triggers_at_playhead();
    int event_trigger_count_at_playhead() const;
    void add_fade_effect_to_selected_layer();
    void add_pulse_effect_to_selected_layer();
    void add_flash_effect_to_selected_layer();
    void add_shake_effect_to_selected_layer();
    void clear_selected_layer_effects();
    void delete_selected_layer();
    void normalize_layer_z_order();
    bool move_selected_layer(int direction);
    mv::composition::layer* selected_layer();
    const mv::composition::layer* selected_layer() const;
    const mv::composition::asset_ref* find_asset(const std::string& asset_id) const;
    const Texture2D* texture_for_asset(const mv::composition::asset_ref& asset);
    void unload_asset_textures();
    bool load_preview_audio();
    void stop_preview_audio();
    void set_preview_playing(bool playing);
    void seek_preview_audio_to_playhead();
    double composition_duration_ms() const;

    song_data song_;
    mv::mv_package package_;
    mv::composition::mv_composition composition_;
    mv::composition::edit_history history_;
    std::vector<std::string> diagnostics_;
    std::string selected_layer_id_;
    std::unordered_map<std::string, Texture2D> asset_textures_;
    ui::text_input_state name_input_;
    ui::text_input_state author_input_;
    ui::text_input_state layer_name_input_;
    ui::text_input_state layer_text_input_;
    ui::text_input_state layer_fill_input_;
    std::string inspector_input_layer_id_;
    bool metadata_modal_open_ = false;
    float metadata_modal_open_anim_ = 0.0f;
    bool dirty_ = false;
    bool metadata_dirty_ = false;
    bool preview_playing_ = false;
    bool preview_audio_loaded_ = false;
    bool inspector_edit_pending_ = false;
    double playhead_ms_ = 0.0;
    timeline_drag_mode timeline_drag_mode_ = timeline_drag_mode::none;
    std::string timeline_drag_layer_id_;
    float timeline_drag_origin_mouse_x_ = 0.0f;
    double timeline_drag_origin_start_ms_ = 0.0;
    double timeline_drag_origin_duration_ms_ = 0.0;
};
