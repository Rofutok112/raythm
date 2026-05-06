#include "title/seamless_song_select_view.h"

#include <algorithm>
#include <string>
#include <vector>

#include "network/server_environment.h"
#include "ranking_service.h"
#include "raylib.h"
#include "scene_common.h"
#include "song_select/song_select_confirmation_dialog.h"
#include "song_select/song_select_layout.h"
#include "song_select/song_select_login_dialog.h"
#include "title/center_panel_view.h"
#include "title/local_content_index.h"
#include "title/ranking_panel_view.h"
#include "title/song_list_view.h"
#include "theme.h"
#include "tween.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace title_play_view {
namespace {

constexpr Rectangle kPlayBackButtonRect = {72.0f, 57.0f, 147.0f, 57.0f};
constexpr Rectangle kPlaySongColumnRect = {99.0f, 162.0f, 507.0f, 756.0f};
constexpr Rectangle kPlayMainColumnRect = {657.0f, 150.0f, 603.0f, 780.0f};
constexpr Rectangle kPlayRankingColumnRect = {1317.0f, 153.0f, 507.0f, 774.0f};
constexpr Rectangle kPlayJacketRect = {684.0f, 273.0f, 282.0f, 282.0f};
constexpr Rectangle kPlayChartDetailRect = {1002.0f, 273.0f, 258.0f, 135.0f};
constexpr Rectangle kPlayMetaRect = {684.0f, 582.0f, 576.0f, 90.0f};
constexpr Rectangle kPlayChartButtonsRect = {684.0f, 600.0f, 576.0f, 327.0f};
constexpr Rectangle kPlayRankingHeaderRect = {1317.0f, 153.0f, 507.0f, 54.0f};
constexpr Rectangle kPlayRankingSourceLocalRect = {1551.0f, 147.0f, 129.0f, 51.0f};
constexpr Rectangle kPlayRankingSourceOnlineRect = {1689.0f, 147.0f, 135.0f, 51.0f};
constexpr Rectangle kPlayRankingListRect = {1317.0f, 225.0f, 507.0f, 702.0f};
constexpr float kCreateToolButtonHeight = 50.0f;
constexpr float kCreateToolButtonGap = 8.0f;
constexpr float kCreateToolColumnGap = 12.0f;
constexpr float kCreatePanelSectionGap = 22.0f;
constexpr float kCreatePanelSectionLabelHeight = 24.0f;
constexpr float kContextMenuInnerPadding = 6.0f;
constexpr Rectangle kFallbackOriginRect = {840.0f, 564.0f, 240.0f, 90.0f};
constexpr Vector2 kSeedSongOffset = {-495.0f, 33.0f};
constexpr Vector2 kSeedMainOffset = {0.0f, 9.0f};
constexpr Vector2 kSeedRankingOffset = {495.0f, 39.0f};
constexpr Vector2 kSeedBackOffset = {-642.0f, -291.0f};
constexpr Vector2 kSeedJacketOffset = {-102.0f, -39.0f};
constexpr Vector2 kSeedMetaOffset = {15.0f, 138.0f};
constexpr Vector2 kSeedChartDetailOffset = {228.0f, -15.0f};
constexpr Vector2 kSeedChartButtonsOffset = {81.0f, 240.0f};
constexpr Vector2 kSeedRankingHeaderOffset = {522.0f, -276.0f};
constexpr Vector2 kSeedRankingSourceLocalOffset = {627.0f, -285.0f};
constexpr Vector2 kSeedRankingSourceOnlineOffset = {768.0f, -285.0f};
constexpr Vector2 kSeedRankingListOffset = {522.0f, 30.0f};
constexpr float kWheelScrollStep = 63.0f;
constexpr int kPlaySongContextMenuItemCount = 1;
constexpr int kPlayChartContextMenuItemCount = 1;

enum class create_tool_action {
    create_song,
    edit_song,
    import_song,
    export_song,
    upload_song,
    create_chart,
    edit_chart,
    import_chart,
    export_chart,
    upload_chart,
    edit_mv,
    manage_library,
};

struct create_tool_entry {
    std::string title;
    std::string detail;
    create_tool_action action;
    bool enabled = true;
    bool primary = false;
};

struct create_tool_section {
    std::string title;
    std::vector<create_tool_entry> entries;
};

constexpr int kCreateToolColumnCount = 2;

Rectangle create_tool_rect(Rectangle section_rect, int index, bool primary) {
    if (primary) {
        return {
            section_rect.x,
            section_rect.y,
            section_rect.width,
            kCreateToolButtonHeight,
        };
    }
    const int column = index % kCreateToolColumnCount;
    const int row = index / kCreateToolColumnCount;
    const float width =
        (section_rect.width - kCreateToolColumnGap * static_cast<float>(kCreateToolColumnCount - 1)) /
        static_cast<float>(kCreateToolColumnCount);
    return {
        section_rect.x + static_cast<float>(column) * (width + kCreateToolColumnGap),
        section_rect.y + static_cast<float>(row) * (kCreateToolButtonHeight + kCreateToolButtonGap),
        width,
        kCreateToolButtonHeight,
    };
}

float create_tools_height(const std::vector<create_tool_entry>& entries) {
    if (entries.empty()) {
        return 0.0f;
    }

    int rows = 0;
    int secondary_count = 0;
    for (const create_tool_entry& entry : entries) {
        if (entry.primary) {
            rows += 1;
        } else {
            ++secondary_count;
        }
    }
    rows += (secondary_count + kCreateToolColumnCount - 1) / kCreateToolColumnCount;
    return static_cast<float>(rows) * kCreateToolButtonHeight +
           static_cast<float>(std::max(0, rows - 1)) * kCreateToolButtonGap;
}

std::optional<local_content_index::online_origin> song_upload_origin(const song_select::song_entry* song,
                                                                     const std::string& server_url) {
    if (song == nullptr || song->song.meta.song_id.empty()) {
        return std::nullopt;
    }
    const auto binding = local_content_index::find_song_by_local(server_url, song->song.meta.song_id);
    return binding.has_value() ? std::optional<local_content_index::online_origin>(binding->origin) : std::nullopt;
}

std::optional<local_content_index::online_origin> chart_upload_origin(const song_select::chart_option* chart,
                                                                      const std::string& server_url) {
    if (chart == nullptr || chart->meta.chart_id.empty()) {
        return std::nullopt;
    }
    const auto binding = local_content_index::find_chart_by_local(server_url, chart->meta.chart_id);
    return binding.has_value() ? std::optional<local_content_index::online_origin>(binding->origin) : std::nullopt;
}

bool is_owned_origin(std::optional<local_content_index::online_origin> origin) {
    return origin.has_value() && *origin == local_content_index::online_origin::owned_upload;
}

std::vector<create_tool_section> build_create_tool_sections(const song_select::song_entry* song,
                                                            const song_select::chart_option* chart) {
    const std::string server_url = server_environment::active_server_url();
    const auto song_origin = song_upload_origin(song, server_url);
    const auto chart_origin = chart_upload_origin(chart, server_url);
    const bool song_selected = song != nullptr;
    const bool chart_selected = chart != nullptr;
    const bool owned_song = is_owned_origin(song_origin);
    const bool owned_chart = is_owned_origin(chart_origin);
    const bool song_can_create_chart = song_selected;
    const bool linked_remote_song = song_origin.has_value();
    const bool song_can_upload =
        song_selected &&
        (song->status == content_status::local ||
         (song->status == content_status::modified && owned_song));
    const bool chart_can_upload =
        chart_selected &&
        linked_remote_song &&
        (chart->status == content_status::local ||
         (chart->status == content_status::modified && owned_chart));

    std::string song_publish_title = "UPLOAD SONG";
    std::string song_publish_detail = "Publish selected song";
    if (!song_selected) {
        song_publish_title = "SELECT SONG";
        song_publish_detail = "Song publish unavailable";
    } else if (owned_song && song->status == content_status::modified) {
        song_publish_title = "UPDATE SONG";
        song_publish_detail = "Replace your upload";
    } else if (owned_song) {
        song_publish_title = "UPLOADED";
        song_publish_detail = "No local changes";
    } else if (song->source_status == content_status::official) {
        song_publish_title = "OFFICIAL SONG";
        song_publish_detail = "Song is linked online";
    } else if (song->source_status == content_status::community || linked_remote_song) {
        song_publish_title = "LINKED SONG";
        song_publish_detail = "Not your song upload";
    }

    std::string chart_publish_title = "UPLOAD CHART";
    std::string chart_publish_detail = linked_remote_song ? "Publish to this song" : "Upload song first";
    if (!chart_selected) {
        chart_publish_title = "SELECT CHART";
        chart_publish_detail = "Chart publish unavailable";
    } else if (owned_chart && chart->status == content_status::modified) {
        chart_publish_title = "UPDATE CHART";
        chart_publish_detail = "Replace your upload";
    } else if (owned_chart) {
        chart_publish_title = "UPLOADED";
        chart_publish_detail = "No local changes";
    } else if (chart->source_status == content_status::official) {
        chart_publish_title = "OFFICIAL CHART";
        chart_publish_detail = "Chart is read-only";
    } else if (chart->source_status == content_status::community) {
        chart_publish_title = "COMMUNITY CHART";
        chart_publish_detail = "Not your chart upload";
    }

    return {
        {
            "Song",
            {
                {song_publish_title, song_publish_detail, create_tool_action::upload_song, song_can_upload, true},
                {"NEW SONG", "Create package", create_tool_action::create_song, true, false},
                {"EDIT SONG", "Metadata", create_tool_action::edit_song, song_selected, false},
                {"IMPORT SONG", ".rpack", create_tool_action::import_song, true, false},
                {"EXPORT SONG", ".rpack", create_tool_action::export_song, song_selected, false},
            },
        },
        {
            "Chart",
            {
                {chart_publish_title, chart_publish_detail, create_tool_action::upload_chart, chart_can_upload, true},
                {"NEW CHART", "Add to song", create_tool_action::create_chart, song_can_create_chart, false},
                {"EDIT CHART", "Open editor", create_tool_action::edit_chart, chart_selected, false},
                {"IMPORT CHART", ".rchart", create_tool_action::import_chart, song_selected, false},
                {"EXPORT CHART", ".rchart", create_tool_action::export_chart, chart_selected, false},
            },
        },
        {
            "More",
            {
                {"MV EDITOR", "Visuals", create_tool_action::edit_mv, song_selected, false},
                {"LIBRARY", "Classic tools", create_tool_action::manage_library, true, false},
            },
        },
    };
}

Rectangle context_menu_item_rect(Rectangle menu_rect, int index = 0) {
    return {
        menu_rect.x + kContextMenuInnerPadding,
        menu_rect.y + kContextMenuInnerPadding +
            static_cast<float>(index) *
                (song_select::layout::kContextMenuItemHeight + song_select::layout::kContextMenuItemSpacing),
        menu_rect.width - kContextMenuInnerPadding * 2.0f,
        song_select::layout::kContextMenuItemHeight
    };
}

void enqueue_delete_context_menu(Rectangle menu_rect, const char* label) {
    const auto& t = *g_theme;
    const Rectangle item_rect = context_menu_item_rect(menu_rect);
    const bool hovered = ui::is_hovered(item_rect, song_select::layout::kContextMenuLayer);
    const bool pressed = ui::is_pressed(item_rect, song_select::layout::kContextMenuLayer);
    const Rectangle visual = pressed ? ui::inset(item_rect, 1.5f) : item_rect;
    const Color bg = hovered ? t.row_hover : t.row;
    const Color fill = with_alpha(lerp_color(bg, t.error, hovered ? 0.16f : 0.08f), 228);
    const Color border = with_alpha(lerp_color(t.border, t.error, 0.35f), 220);
    const std::string label_copy = label != nullptr ? label : "";

    ui::enqueue_draw_command(song_select::layout::kContextMenuLayer,
                             [menu_rect, item_rect, visual, fill, border, label_copy]() {
        ui::draw_section(menu_rect);
        ui::draw_rect_f(visual, fill);
        ui::draw_rect_lines(visual, 1.5f, border);
        const Rectangle accent = {
            visual.x + 10.0f,
            visual.y + 9.0f,
            4.0f,
            std::max(0.0f, visual.height - 18.0f),
        };
        ui::draw_rect_f(accent, with_alpha(g_theme->error, 220));
        ui::draw_text_in_rect(label_copy.c_str(), 16,
                              {visual.x + 24.0f, visual.y, visual.width - 34.0f, visual.height},
                              g_theme->text, ui::text_align::left);
    });
}

Rectangle centered_scaled_rect(Rectangle anchor, Rectangle target, float scale, Vector2 offset = {0.0f, 0.0f}) {
    const Vector2 center = {
        anchor.x + anchor.width * 0.5f + offset.x,
        anchor.y + anchor.height * 0.5f + offset.y,
    };
    const float width = target.width * scale;
    const float height = target.height * scale;
    return {
        center.x - width * 0.5f,
        center.y - height * 0.5f,
        width,
        height,
    };
}

Rectangle fallback_origin_rect() {
    return kFallbackOriginRect;
}

Rectangle resolve_origin_rect(Rectangle origin_rect) {
    return origin_rect.width > 0.0f && origin_rect.height > 0.0f ? origin_rect : fallback_origin_rect();
}
}  // namespace

layout make_layout(float anim_t, Rectangle origin_rect) {
    const float t = tween::ease_out_cubic(anim_t);
    const Rectangle origin = resolve_origin_rect(origin_rect);

    const Rectangle seed_song = centered_scaled_rect(origin, kPlaySongColumnRect, 0.68f, kSeedSongOffset);
    const Rectangle seed_main = centered_scaled_rect(origin, kPlayMainColumnRect, 0.74f, kSeedMainOffset);
    const Rectangle seed_ranking = centered_scaled_rect(origin, kPlayRankingColumnRect, 0.68f, kSeedRankingOffset);
    const Rectangle seed_back = centered_scaled_rect(origin, kPlayBackButtonRect, 0.9f, kSeedBackOffset);
    const Rectangle seed_jacket = centered_scaled_rect(origin, kPlayJacketRect, 0.82f, kSeedJacketOffset);
    const Rectangle seed_meta = centered_scaled_rect(origin, kPlayMetaRect, 0.8f, kSeedMetaOffset);
    const Rectangle seed_chart_detail = centered_scaled_rect(origin, kPlayChartDetailRect, 0.76f, kSeedChartDetailOffset);
    const Rectangle seed_chart_buttons = centered_scaled_rect(origin, kPlayChartButtonsRect, 0.88f, kSeedChartButtonsOffset);
    const Rectangle seed_ranking_header = centered_scaled_rect(origin, kPlayRankingHeaderRect, 0.7f, kSeedRankingHeaderOffset);
    const Rectangle seed_ranking_source_local = centered_scaled_rect(origin, kPlayRankingSourceLocalRect, 0.8f, kSeedRankingSourceLocalOffset);
    const Rectangle seed_ranking_source_online = centered_scaled_rect(origin, kPlayRankingSourceOnlineRect, 0.8f, kSeedRankingSourceOnlineOffset);
    const Rectangle seed_ranking_list = centered_scaled_rect(origin, kPlayRankingListRect, 0.7f, kSeedRankingListOffset);

    return {
        tween::lerp(seed_back, kPlayBackButtonRect, t),
        tween::lerp(seed_song, kPlaySongColumnRect, t),
        tween::lerp(seed_main, kPlayMainColumnRect, t),
        tween::lerp(seed_ranking, kPlayRankingColumnRect, t),
        tween::lerp(seed_jacket, kPlayJacketRect, t),
        tween::lerp(seed_meta, kPlayMetaRect, t),
        tween::lerp(seed_chart_detail, kPlayChartDetailRect, t),
        tween::lerp(seed_chart_buttons, kPlayChartButtonsRect, t),
        tween::lerp(seed_ranking_header, kPlayRankingHeaderRect, t),
        tween::lerp(seed_ranking_source_local, kPlayRankingSourceLocalRect, t),
        tween::lerp(seed_ranking_source_online, kPlayRankingSourceOnlineRect, t),
        tween::lerp(seed_ranking_list, kPlayRankingListRect, t),
    };
}

update_result update(song_select::state& state, mode view_mode, float anim_t, Rectangle origin_rect, float dt) {
    update_result result;
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool right_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    const float wheel = GetMouseWheelMove();
    const layout current = make_layout(anim_t, origin_rect);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const bool has_selection = song_select::selected_song(state) != nullptr &&
                               song_select::selected_chart_for(state, filtered) != nullptr;
    const bool play_song_menu_open =
        view_mode == mode::play &&
        state.context_menu.open &&
        state.context_menu.target == song_select::context_menu_target::song;
    const bool play_chart_menu_open =
        view_mode == mode::play &&
        state.context_menu.open &&
        state.context_menu.target == song_select::context_menu_target::chart;

    if (state.confirmation_dialog.open) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            state.confirmation_dialog = {};
        }
        return result;
    }

    if (play_song_menu_open || play_chart_menu_open) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            song_select::close_context_menu(state);
            return result;
        }
        if (left_pressed && CheckCollisionPointRec(mouse, context_menu_item_rect(state.context_menu.rect))) {
            const bool deleting_chart = state.context_menu.target == song_select::context_menu_target::chart;
            song_select::open_confirmation_dialog(
                state,
                deleting_chart ? song_select::pending_confirmation_action::delete_chart
                               : song_select::pending_confirmation_action::delete_song,
                "", "", "", "DELETE", state.context_menu.song_index,
                deleting_chart ? state.context_menu.chart_index : -1);
            song_select::close_context_menu(state);
            if (deleting_chart) {
                result.delete_chart_requested = true;
            } else {
                result.delete_song_requested = true;
            }
            return result;
        }
        if ((left_pressed || right_pressed) && !CheckCollisionPointRec(mouse, state.context_menu.rect)) {
            song_select::close_context_menu(state);
            return result;
        }
        return result;
    }

    if (ui::is_clicked(current.back_rect) || IsKeyPressed(KEY_ESCAPE)) {
        result.back_requested = true;
        return result;
    }

    if (IsKeyPressed(KEY_ENTER) && has_selection) {
        result.play_requested = true;
        return result;
    }

    if (view_mode == mode::play) {
        const song_select::song_entry* song = song_select::selected_song(state);
        const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
        const bool update_available =
            (song != nullptr && song->status == content_status::update) ||
            (chart != nullptr && chart->status == content_status::update);
        if (left_pressed && update_available) {
            const bool song_update_clicked =
                song != nullptr &&
                song->status == content_status::update &&
                CheckCollisionPointRec(mouse, title_center_view::song_status_badge_rect(current.main_column));
            const bool chart_update_clicked =
                chart != nullptr &&
                chart->status == content_status::update &&
                CheckCollisionPointRec(mouse, title_center_view::chart_status_badge_rect(current.chart_detail_rect));
            if (song_update_clicked) {
                result.update_song_requested = true;
                return result;
            }
            if (chart_update_clicked) {
                result.update_chart_requested = true;
                return result;
            }
        }

        const title_ranking_view::draw_config ranking_config = {
            .header_rect = current.ranking_header_rect,
            .source_local_rect = current.ranking_source_local_rect,
            .source_online_rect = current.ranking_source_online_rect,
            .list_rect = current.ranking_list_rect,
        };
        if (left_pressed) {
            const auto source = title_ranking_view::hit_test_source(ranking_config, mouse);
            if (source.has_value() && source.value() != state.ranking_panel.selected_source) {
                state.ranking_panel.selected_source = *source;
                result.ranking_source_changed = true;
                return result;
            }
        }
    } else if (left_pressed) {
        const song_select::song_entry* song = song_select::selected_song(state);
        const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
        const std::vector<create_tool_section> sections = build_create_tool_sections(song, chart);
        float section_y = current.ranking_list_rect.y;
        for (const create_tool_section& section : sections) {
            const float tools_y = section_y + kCreatePanelSectionLabelHeight;
            const Rectangle tools_rect{
                current.ranking_list_rect.x,
                tools_y,
                current.ranking_list_rect.width,
                current.ranking_list_rect.height - (tools_y - current.ranking_list_rect.y),
            };
            int primary_index = 0;
            int secondary_index = 0;
            for (const create_tool_entry& entry : section.entries) {
                const int index = entry.primary ? primary_index++ : secondary_index++;
                Rectangle rect = create_tool_rect(tools_rect, index, entry.primary);
                if (!entry.primary) {
                    const float secondary_top =
                        tools_y + static_cast<float>(primary_index) * (kCreateToolButtonHeight + kCreateToolButtonGap);
                    rect.y = secondary_top +
                        static_cast<float>(index / kCreateToolColumnCount) *
                            (kCreateToolButtonHeight + kCreateToolButtonGap);
                }
                if (!entry.enabled || !CheckCollisionPointRec(mouse, rect)) {
                    continue;
                }
                switch (entry.action) {
                case create_tool_action::create_song: result.create_song_requested = true; break;
                case create_tool_action::edit_song: result.edit_song_requested = true; break;
                case create_tool_action::import_song: result.import_song_requested = true; break;
                case create_tool_action::export_song: result.export_song_requested = true; break;
                case create_tool_action::upload_song: result.upload_song_requested = true; break;
                case create_tool_action::create_chart: result.create_chart_requested = true; break;
                case create_tool_action::edit_chart: result.edit_chart_requested = true; break;
                case create_tool_action::import_chart: result.import_chart_requested = true; break;
                case create_tool_action::export_chart: result.export_chart_requested = true; break;
                case create_tool_action::upload_chart: result.upload_chart_requested = true; break;
                case create_tool_action::edit_mv: result.edit_mv_requested = true; break;
                case create_tool_action::manage_library: result.manage_library_requested = true; break;
                }
                return result;
            }
            section_y = tools_y + create_tools_height(section.entries) + kCreatePanelSectionGap;
        }
    }

    if (left_pressed) {
        const int clicked_chart =
            title_center_view::hit_test_chart(current.chart_buttons_rect, state.chart_scroll_y, mouse,
                                              static_cast<int>(filtered.size()));
        if (clicked_chart >= 0) {
            if (state.difficulty_index == clicked_chart) {
                result.play_requested = true;
            } else {
                state.difficulty_index = clicked_chart;
                state.chart_change_anim_t = 1.0f;
                result.chart_selection_changed = true;
            }
            return result;
        }
    }

    if (right_pressed && view_mode == mode::play && !state.songs.empty()) {
        const int clicked_chart =
            title_center_view::hit_test_chart(current.chart_buttons_rect, state.chart_scroll_y, mouse,
                                              static_cast<int>(filtered.size()));
        if (clicked_chart >= 0) {
            if (state.difficulty_index != clicked_chart) {
                state.difficulty_index = clicked_chart;
                state.chart_change_anim_t = 1.0f;
                result.chart_selection_changed = true;
            }
            state.context_menu.open = true;
            state.context_menu.target = song_select::context_menu_target::chart;
            state.context_menu.section = song_select::context_menu_section::chart;
            state.context_menu.song_index = state.selected_song_index;
            state.context_menu.chart_index = clicked_chart;
            state.context_menu.rect = song_select::layout::make_context_menu_rect(
                mouse, kPlayChartContextMenuItemCount);
            return result;
        }
    }

    if (right_pressed && view_mode == mode::play && !state.songs.empty()) {
        const int clicked_song =
            title_song_list_view::hit_test(current.song_column, state.scroll_y, mouse,
                                           static_cast<int>(state.songs.size()));
        if (clicked_song >= 0) {
            if (song_select::apply_song_selection(state, clicked_song, 0)) {
                result.song_selection_changed = true;
            }
            state.context_menu.open = true;
            state.context_menu.target = song_select::context_menu_target::song;
            state.context_menu.section = song_select::context_menu_section::song;
            state.context_menu.song_index = clicked_song;
            state.context_menu.chart_index = -1;
            state.context_menu.rect = song_select::layout::make_context_menu_rect(
                mouse, kPlaySongContextMenuItemCount);
            return result;
        }
    }

    if (left_pressed && !state.songs.empty()) {
        const int clicked_song =
            title_song_list_view::hit_test(current.song_column, state.scroll_y, mouse,
                                           static_cast<int>(state.songs.size()));
        if (clicked_song >= 0) {
            if (song_select::apply_song_selection(state, clicked_song, 0)) {
                result.song_selection_changed = true;
            }
            return result;
        }
    }

    if (!state.songs.empty() && (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))) {
        const int next_index = std::clamp(state.selected_song_index - 1, 0, static_cast<int>(state.songs.size()) - 1);
        if (next_index != state.selected_song_index && song_select::apply_song_selection(state, next_index, 0)) {
            result.song_selection_changed = true;
        }
    } else if (!state.songs.empty() && (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))) {
        const int next_index = std::clamp(state.selected_song_index + 1, 0, static_cast<int>(state.songs.size()) - 1);
        if (next_index != state.selected_song_index && song_select::apply_song_selection(state, next_index, 0)) {
            result.song_selection_changed = true;
        }
    }

    if (!filtered.empty() && (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))) {
        const int next_index = std::clamp(state.difficulty_index - 1, 0, static_cast<int>(filtered.size()) - 1);
        if (next_index != state.difficulty_index) {
            state.difficulty_index = next_index;
            state.chart_change_anim_t = 1.0f;
            result.chart_selection_changed = true;
        }
    } else if (!filtered.empty() && (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))) {
        const int next_index = std::clamp(state.difficulty_index + 1, 0, static_cast<int>(filtered.size()) - 1);
        if (next_index != state.difficulty_index) {
            state.difficulty_index = next_index;
            state.chart_change_anim_t = 1.0f;
            result.chart_selection_changed = true;
        }
    }

    if (CheckCollisionPointRec(mouse, current.song_column) && wheel != 0.0f) {
        state.scroll_y_target -= wheel * kWheelScrollStep;
    } else if (CheckCollisionPointRec(mouse, current.chart_buttons_rect) && wheel != 0.0f) {
        state.chart_scroll_y_target -= wheel * kWheelScrollStep;
    } else if (view_mode == mode::play && CheckCollisionPointRec(mouse, current.ranking_list_rect) && wheel != 0.0f) {
        state.ranking_panel.scroll_y_target -= wheel * kWheelScrollStep;
    }

    state.scroll_y_target = std::clamp(
        state.scroll_y_target, 0.0f,
        title_song_list_view::max_scroll(current.song_column, static_cast<int>(state.songs.size())));
    state.scroll_y = tween::damp(state.scroll_y, state.scroll_y_target, dt, 12.0f, 0.5f);

    state.chart_scroll_y_target = std::clamp(
        state.chart_scroll_y_target, 0.0f,
        title_center_view::max_chart_scroll(current.chart_buttons_rect, static_cast<int>(filtered.size())));
    state.chart_scroll_y = tween::damp(state.chart_scroll_y, state.chart_scroll_y_target, dt, 12.0f, 0.5f);

    if (view_mode == mode::play) {
        state.ranking_panel.scroll_y_target = std::clamp(
            state.ranking_panel.scroll_y_target, 0.0f,
            title_ranking_view::max_scroll(current.ranking_list_rect, state.ranking_panel.listing));
        state.ranking_panel.scroll_y =
            tween::damp(state.ranking_panel.scroll_y, state.ranking_panel.scroll_y_target, dt, 12.0f, 0.5f);
    }

    return result;
}

void draw(const song_select::state& state,
          const song_select::preview_controller& preview_controller,
          mode view_mode,
          float anim_t,
          Rectangle origin_rect) {
    const auto& t = *g_theme;
    const float play_t = tween::ease_out_cubic(anim_t);
    if (play_t <= 0.01f) {
        return;
    }

    const layout current = make_layout(anim_t, origin_rect);
    const float content_fade_t = std::clamp((play_t - 0.18f) / 0.62f, 0.0f, 1.0f);
    const unsigned char alpha = static_cast<unsigned char>(255.0f * content_fade_t);
    const bool hide_unloaded_content =
        state.catalog_loading && !state.catalog_loaded_once && state.songs.empty();
    const double now = GetTime();
    const Color button_base = t.row_soft;
    const Color button_hover = t.row_soft_hover;
    const Color button_selected = t.row_soft_selected;
    const Color button_selected_hover = t.row_soft_selected_hover;
    const unsigned char normal_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_alpha) / 255);
    const unsigned char hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_hover_alpha) / 255);
    const unsigned char selected_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_alpha) / 255);
    const unsigned char selected_hover_row_alpha =
        static_cast<unsigned char>((static_cast<unsigned short>(alpha) * t.row_soft_selected_hover_alpha) / 255);
    ui::draw_button_colored(current.back_rect, "HOME", 16,
                            with_alpha(button_base, normal_row_alpha), with_alpha(button_hover, hover_row_alpha), with_alpha(t.text, alpha), 1.5f);

    ui::draw_line_ex({current.song_column.x + current.song_column.width + 22.0f, current.song_column.y + 18.0f},
                     {current.song_column.x + current.song_column.width + 22.0f, current.song_column.y + current.song_column.height - 18.0f},
                     1.2f, with_alpha(t.border_light, static_cast<unsigned char>(170.0f * play_t)));
    ui::draw_line_ex({current.ranking_column.x - 24.0f, current.ranking_column.y + 24.0f},
                     {current.ranking_column.x - 24.0f, current.ranking_column.y + current.ranking_column.height - 20.0f},
                     1.2f, with_alpha(t.border_light, static_cast<unsigned char>(170.0f * play_t)));

    if (!hide_unloaded_content) {
        title_song_list_view::draw(state, {
            .column_rect = current.song_column,
            .play_t = play_t,
            .alpha = alpha,
            .button_base = button_base,
            .button_selected = button_selected,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
            .selected_row_alpha = selected_row_alpha,
            .now = now,
        });
    }

    const song_select::song_entry* song = song_select::selected_song(state);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);

    if (!hide_unloaded_content) {
        title_center_view::draw(state, preview_controller, song, chart, filtered, {
            .main_column_rect = current.main_column,
            .jacket_rect = current.jacket_rect,
            .chart_detail_rect = current.chart_detail_rect,
            .chart_buttons_rect = current.chart_buttons_rect,
            .play_t = play_t,
            .alpha = alpha,
            .button_base = button_base,
            .button_selected = button_selected,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
            .selected_row_alpha = selected_row_alpha,
            .now = now,
        });
    }

    if (view_mode == mode::play) {
        if (hide_unloaded_content) {
            return;
        }
        title_ranking_view::draw(state.ranking_panel, {
            .header_rect = current.ranking_header_rect,
            .source_local_rect = current.ranking_source_local_rect,
            .source_online_rect = current.ranking_source_online_rect,
            .list_rect = current.ranking_list_rect,
            .play_t = play_t,
            .alpha = alpha,
            .button_base = button_base,
            .button_hover = button_hover,
            .button_selected = button_selected,
            .button_selected_hover = button_selected_hover,
            .normal_row_alpha = normal_row_alpha,
            .hover_row_alpha = hover_row_alpha,
            .selected_row_alpha = selected_row_alpha,
            .selected_hover_row_alpha = selected_hover_row_alpha,
        });
    } else {
        ui::draw_text_in_rect("CREATE", 22, current.ranking_header_rect, with_alpha(t.text, alpha), ui::text_align::left);
        const std::vector<create_tool_section> sections = build_create_tool_sections(song, chart);
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        float section_y = current.ranking_list_rect.y;
        for (const create_tool_section& section : sections) {
            ui::draw_text_in_rect(section.title.c_str(), 14,
                                  {current.ranking_list_rect.x, section_y,
                                   current.ranking_list_rect.width, kCreatePanelSectionLabelHeight},
                                  with_alpha(t.text_secondary, alpha), ui::text_align::left);
            const float tools_y = section_y + kCreatePanelSectionLabelHeight;
            const Rectangle tools_rect{
                current.ranking_list_rect.x,
                tools_y,
                current.ranking_list_rect.width,
                current.ranking_list_rect.height - (tools_y - current.ranking_list_rect.y),
            };
            int primary_index = 0;
            int secondary_index = 0;
            for (const create_tool_entry& entry : section.entries) {
                const int index = entry.primary ? primary_index++ : secondary_index++;
                Rectangle rect = create_tool_rect(tools_rect, index, entry.primary);
                if (!entry.primary) {
                    const float secondary_top =
                        tools_y + static_cast<float>(primary_index) * (kCreateToolButtonHeight + kCreateToolButtonGap);
                    rect.y = secondary_top +
                        static_cast<float>(index / kCreateToolColumnCount) *
                            (kCreateToolButtonHeight + kCreateToolButtonGap);
                }

                const bool hovered = entry.enabled && CheckCollisionPointRec(mouse, rect);
                const unsigned char row_alpha = !entry.enabled
                    ? static_cast<unsigned char>(normal_row_alpha / 2)
                    : hovered ? hover_row_alpha : normal_row_alpha;
                const Color action_color = entry.primary && entry.enabled
                    ? (entry.title.find("UPDATE") != std::string::npos ? t.accent : t.success)
                    : button_base;
                const Color fill = entry.primary && entry.enabled
                    ? with_alpha(lerp_color(button_base, action_color, hovered ? 0.30f : 0.20f), row_alpha)
                    : with_alpha(button_base, row_alpha);
                const Color border = entry.primary && entry.enabled
                    ? with_alpha(action_color, static_cast<unsigned char>(std::min(255, static_cast<int>(row_alpha) + 38)))
                    : with_alpha(t.border, row_alpha);
                ui::draw_rect_f(rect, fill);
                ui::draw_rect_lines(rect, entry.primary ? 1.6f : 1.2f, border);
                ui::draw_text_in_rect(entry.title.c_str(), entry.primary ? 16 : 13,
                                      {rect.x + 12.0f, rect.y + 5.0f, rect.width - 24.0f, 20.0f},
                                      with_alpha(entry.enabled ? t.text : t.text_muted, alpha), ui::text_align::left);
                ui::draw_text_in_rect(entry.detail.c_str(), 10,
                                      {rect.x + 12.0f, rect.y + 27.0f, rect.width - 24.0f, 15.0f},
                                      with_alpha(t.text_muted, alpha), ui::text_align::left);
            }
            section_y = tools_y + create_tools_height(section.entries) + kCreatePanelSectionGap;
        }
    }

    if (view_mode == mode::play &&
        state.context_menu.open &&
        (state.context_menu.target == song_select::context_menu_target::song ||
         state.context_menu.target == song_select::context_menu_target::chart)) {
        enqueue_delete_context_menu(
            state.context_menu.rect,
            state.context_menu.target == song_select::context_menu_target::chart
                ? "DELETE CHART"
                : "DELETE SONG");
    }

}

}  // namespace title_play_view
