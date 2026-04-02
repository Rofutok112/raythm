#pragma once

#include <optional>
#include <string>
#include <vector>

#include "raylib.h"
#include "audio_manager.h"
#include "data_models.h"
#include "scene.h"

// 曲選択画面。難易度・キーモードを選んでプレイ画面に遷移する。
class song_select_scene final : public scene {
public:
    explicit song_select_scene(scene_manager& manager, std::string preferred_song_id = "");

    void on_enter() override;
    void on_exit() override;
    void update(float dt) override;
    void draw() override;

private:
    struct chart_option {
        std::string path;
        chart_meta meta;
        content_source source = content_source::legacy_assets;
        bool can_delete = false;
    };

    struct song_entry {
        song_data song;
        std::vector<chart_option> charts;
    };

    enum class context_menu_target {
        none,
        song,
        chart,
    };

    enum class pending_confirmation_action {
        none,
        delete_song,
        delete_chart,
    };

    struct context_menu_state {
        bool open = false;
        context_menu_target target = context_menu_target::none;
        int song_index = -1;
        int chart_index = -1;
        Rectangle rect = {};
    };

    struct confirmation_dialog_state {
        bool open = false;
        pending_confirmation_action action = pending_confirmation_action::none;
        int song_index = -1;
        int chart_index = -1;
    };

    const song_entry* selected_song() const;
    std::vector<const chart_option*> filtered_charts_for_selected_song() const;
    void reload_song_library(const std::string& preferred_song_id = "",
                             const std::string& preferred_chart_id = "");
    void load_jacket_for_selected_song();
    void queue_preview_for_selected_song();
    void start_preview(const song_entry& song);
    void update_preview(float dt);
    void stop_preview_and_unload_jacket();
    const chart_option* selected_chart_for(const std::vector<const chart_option*>& filtered) const;
    void apply_song_selection(int song_index, int chart_index = 0);
    bool handle_song_list_pointer(Vector2 mouse, bool left_pressed, bool right_pressed);
    void open_song_context_menu(int song_index, Vector2 mouse);
    void open_chart_context_menu(int song_index, int chart_index, Vector2 mouse);
    void close_context_menu();
    void queue_status_message(std::string message, bool is_error);
    bool delete_song_content(int song_index);
    bool delete_chart_content(int song_index, int chart_index);
    std::string fallback_song_id_after_song_delete(int song_index) const;
    std::string fallback_chart_id_after_chart_delete(int song_index, int chart_index) const;
    void draw_context_menu();
    void draw_confirmation_dialog();
    void draw_song_details(const song_entry& song, const chart_option* selected_chart,
                           float content_offset_x, unsigned char content_alpha) const;
    void draw_song_list(const std::vector<const chart_option*>& filtered) const;
    void draw_song_row(const song_entry& song, float item_y, bool is_selected, double now) const;
    void draw_chart_rows(const std::vector<const chart_option*>& filtered, float item_y) const;

    // スクロールに必要な総コンテンツ高さを計算する。
    float compute_content_height() const;

    std::vector<song_entry> songs_;
    std::vector<std::string> load_errors_;
    int selected_song_index_ = 0;
    int difficulty_index_ = 0;
    float scroll_y_ = 0.0f;
    float scroll_y_target_ = 0.0f;
    float song_change_anim_t_ = 0.0f;
    float scene_fade_in_t_ = 1.0f;
    std::optional<song_data> pending_preview_song_;
    std::optional<song_data> active_preview_song_;
    std::string preview_song_id_;
    float preview_volume_ = 0.0f;
    int preview_fade_direction_ = 0;
    bool scrollbar_dragging_ = false;
    float scrollbar_drag_offset_ = 0.0f;
    Texture2D jacket_texture_{};
    bool jacket_loaded_ = false;
    context_menu_state context_menu_;
    confirmation_dialog_state confirmation_dialog_;
    std::string preferred_song_id_;
    std::string status_message_;
    bool status_message_is_error_ = false;
};
