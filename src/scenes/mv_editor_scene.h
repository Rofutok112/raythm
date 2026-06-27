#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "data_models.h"
#include "mv/composition/mv_composition.h"
#include "mv/composition/mv_composition_edit_history.h"
#include "mv/mv_storage.h"
#include "mv_editor_context_menu_view.h"
#include "mv_editor_inspector_view.h"
#include "mv_editor_preview_view.h"
#include "mv_editor_side_panel_view.h"
#include "mv_editor_timeline_view.h"
#include "raylib.h"
#include "scene.h"
#include "ui_inspector.h"
#include "ui_text_input.h"

enum class mv_editor_header_action {
    none,
    open_metadata,
    toggle_preview,
    undo,
    redo,
    save,
    back,
};

enum class mv_editor_bottom_tab_action {
    none,
    show_timeline,
    show_project,
};

struct mv_editor_metadata_modal_result {
    bool metadata_changed = false;
};

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

    using context_menu_target = mv_editor_context_menu_target;

    enum class bottom_panel_tab {
        timeline,
        project,
    };

    void validate_composition();
    bool sync_script_assets_from_components();
    void save_mv();
    void commit_history(const std::string& label);
    void update_dirty_from_history();
    void apply_history_snapshot(const mv::composition::edit_snapshot& snapshot);
    bool undo_edit();
    bool redo_edit();
    void reset_inspector_inputs();
    void sync_inspector_inputs(const mv::composition::layer& layer);
    bool inspector_text_input_active() const;
    void add_empty_layer();
    void add_text_layer();
    void add_rect_layer();
    void add_image_layer();
    void add_beat_grid_layer();
    void add_waveform_layer();
    void add_spectrum_layer();
    void import_image_asset_to_project();
    void create_script_asset();
    bool assign_asset_to_selected_component(const mv::composition::asset_ref& asset);
    void add_component_to_selected_layer(const std::string& type);
    void key_selected_transform();
    void delete_transform_keyframes_at_playhead();
    int transform_keyframe_count_at_playhead() const;
    void apply_bottom_tab_action(mv_editor_bottom_tab_action action);
    void apply_metadata_modal_result(const mv_editor_metadata_modal_result& result);
    void apply_hierarchy_result(const mv_editor_hierarchy_result& result);
    void apply_context_menu_result(const mv_editor_context_menu_result& result);
    void apply_project_panel_result(const mv_editor_project_panel_result& result);
    void apply_timeline_layer_row_result(const mv_editor_timeline_layer_row_result& result);
    void apply_timeline_scrub_result(const mv_editor_timeline_scrub_result& result);
    void apply_timeline_drag_start_result(const mv_editor_timeline_drag_start_result& result);
    void apply_timeline_drag_update_result(const mv_editor_timeline_drag_update_result& result);
    void apply_timeline_drag_end_result(const mv_editor_timeline_drag_end_result& result);
    void apply_preview_drag_start_result(const mv_editor_preview_drag_start_result& result);
    void apply_preview_drag_update_result(const mv_editor_preview_drag_update_result& result, Rectangle preview);
    void apply_preview_drag_end_result(const mv_editor_preview_drag_end_result& result);
    bool apply_inspector_layer_name_result(const mv_editor_inspector_layer_name_result& result,
                                           mv::composition::layer& layer);
    bool apply_inspector_transform_input_result(const mv_editor_inspector_transform_input_result& result,
                                                mv::composition::component& transform);
    bool apply_inspector_transform_opacity_result(const mv_editor_inspector_transform_opacity_result& result,
                                                  mv::composition::component& transform);
    bool apply_inspector_transform_card_result(const mv_editor_inspector_transform_card_result& result,
                                               mv::composition::component& transform);
    bool apply_inspector_component_result(const mv_editor_inspector_component_result& result,
                                          mv::composition::component& component);
    bool apply_inspector_component_amount_result(const mv_editor_inspector_component_amount_result& result,
                                                 mv::composition::component& component);
    bool apply_inspector_component_remove_result(const mv_editor_inspector_component_remove_result& result,
                                                 mv::composition::layer& layer);
    bool apply_inspector_add_component_result(const mv_editor_inspector_add_component_result& result);
    void close_metadata_modal();
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
    void hydrate_lua_script_sources(mv::composition::layer& layer) const;
    const Texture2D* texture_for_asset(const mv::composition::asset_ref& asset);
    void unload_asset_textures();
    bool load_preview_audio();
    void stop_preview_audio();
    void set_preview_playing(bool playing);
    void seek_preview_audio_to_playhead();
    double composition_duration_ms() const;
    bool apply_header_action(mv_editor_header_action action);

    song_data song_;
    mv::mv_package package_;
    mv::composition::mv_composition composition_;
    mv::composition::edit_history history_;
    std::vector<std::string> diagnostics_;
    std::string selected_layer_id_;
    std::string selected_project_asset_id_;
    std::unordered_map<std::string, Texture2D> asset_textures_;
    ui::text_input_state name_input_;
    ui::text_input_state author_input_;
    ui::text_input_state layer_name_input_;
    ui::text_input_state layer_text_input_;
    ui::text_input_state layer_fill_input_;
    ui::text_input_state transform_x_input_;
    ui::text_input_state transform_y_input_;
    ui::text_input_state transform_scale_input_;
    std::unordered_map<std::string, ui::text_input_state> component_text_inputs_;
    std::unordered_map<std::string, ui::text_input_state> component_fill_inputs_;
    std::unordered_map<std::string, ui::text_input_state> component_script_entry_inputs_;
    std::unordered_map<std::string, ui::text_input_state> component_script_inputs_;
    std::unordered_map<std::string, ui::inspector::color_picker_state> component_color_pickers_;
    std::string inspector_input_layer_id_;
    bool metadata_modal_open_ = false;
    float metadata_modal_open_anim_ = 0.0f;
    bool dirty_ = false;
    bool metadata_dirty_ = false;
    bool preview_playing_ = false;
    bool preview_audio_loaded_ = false;
    bool inspector_edit_pending_ = false;
    bool context_menu_open_ = false;
    context_menu_target context_menu_target_ = context_menu_target::none;
    bottom_panel_tab bottom_panel_tab_ = bottom_panel_tab::timeline;
    Vector2 context_menu_position_ = {0.0f, 0.0f};
    float inspector_scroll_offset_ = 0.0f;
    bool inspector_scrollbar_dragging_ = false;
    float inspector_scrollbar_drag_offset_ = 0.0f;
    float hierarchy_scroll_offset_ = 0.0f;
    bool hierarchy_scrollbar_dragging_ = false;
    float hierarchy_scrollbar_drag_offset_ = 0.0f;
    float project_scroll_offset_ = 0.0f;
    bool project_scrollbar_dragging_ = false;
    float project_scrollbar_drag_offset_ = 0.0f;
    float timeline_vertical_scroll_offset_ = 0.0f;
    double timeline_horizontal_scroll_ms_ = 0.0;
    float timeline_zoom_ = 1.0f;
    double playhead_ms_ = 0.0;
    mv_preview_drag_mode preview_drag_mode_ = mv_preview_drag_mode::none;
    std::string preview_drag_layer_id_;
    Vector2 preview_drag_origin_mouse_ = {0.0f, 0.0f};
    Rectangle preview_drag_origin_rect_ = {0.0f, 0.0f, 0.0f, 0.0f};
    mv::composition::transform preview_drag_origin_transform_;
    timeline_drag_mode timeline_drag_mode_ = timeline_drag_mode::none;
    std::string timeline_drag_layer_id_;
    float timeline_drag_origin_mouse_x_ = 0.0f;
    double timeline_drag_origin_start_ms_ = 0.0;
    double timeline_drag_origin_duration_ms_ = 0.0;
};
