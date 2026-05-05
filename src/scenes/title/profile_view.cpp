#include "title/profile_view.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "scene_common.h"
#include "theme.h"
#include "tween.h"
#include "ui_clip.h"
#include "ui_draw.h"
#include "virtual_screen.h"

#include "rlgl.h"

namespace title_profile_view {
namespace {

constexpr Rectangle kDialogRect = {210.0f, 96.0f, 1500.0f, 888.0f};
constexpr float kHeaderHeight = 190.0f;
constexpr float kTabHeight = 46.0f;
constexpr float kContentTopGap = 30.0f;
constexpr float kContentOuterPadding = 42.0f;
constexpr float kOpenAnimOffsetY = 30.0f;
constexpr float kOpenAnimScaleInset = 0.035f;
constexpr int kTabCount = 5;
constexpr float kRowHeight = 70.0f;
constexpr float kRowGap = 10.0f;
constexpr float kWheelStep = 78.0f;

Rectangle content_rect() {
    return {
        kDialogRect.x + kContentOuterPadding,
        kDialogRect.y + kHeaderHeight + kTabHeight + kContentTopGap,
        kDialogRect.width - kContentOuterPadding * 2.0f,
        kDialogRect.height - kHeaderHeight - kTabHeight - kContentTopGap - kContentOuterPadding,
    };
}

Rectangle close_rect() {
    return {kDialogRect.x + kDialogRect.width - 132.0f, kDialogRect.y + 24.0f, 92.0f, 42.0f};
}

Rectangle confirm_rect() {
    return {kDialogRect.x + kDialogRect.width - 334.0f,
            kDialogRect.y + kDialogRect.height - 66.0f,
            138.0f,
            42.0f};
}

Rectangle cancel_rect() {
    return {kDialogRect.x + kDialogRect.width - 184.0f,
            kDialogRect.y + kDialogRect.height - 66.0f,
            132.0f,
            42.0f};
}

Rectangle tab_rect(int index) {
    constexpr float width = 178.0f;
    constexpr float gap = 8.0f;
    return {
        kDialogRect.x + 42.0f + static_cast<float>(index) * (width + gap),
        kDialogRect.y + kHeaderHeight,
        width,
        kTabHeight,
    };
}

float max_scroll(Rectangle list_rect, int item_count) {
    const float content_height = static_cast<float>(item_count) * (kRowHeight + kRowGap) - kRowGap;
    return std::max(0.0f, content_height - list_rect.height);
}

Rectangle row_rect(Rectangle list_rect, int visible_index, float scroll_y) {
    const float y = list_rect.y + static_cast<float>(visible_index) * (kRowHeight + kRowGap) - scroll_y;
    return {list_rect.x, y, list_rect.width, kRowHeight};
}

Rectangle row_action_rect(Rectangle row) {
    return {row.x + row.width - 112.0f, row.y + 14.0f, 92.0f, 42.0f};
}

Rectangle overview_card_rect(Rectangle content, int index) {
    constexpr float gap = 12.0f;
    const float width = (content.width - gap * 3.0f) / 4.0f;
    return {
        content.x + static_cast<float>(index) * (width + gap),
        content.y,
        width,
        92.0f,
    };
}

Rectangle settings_delete_account_rect(Rectangle content) {
    return {content.x + 18.0f, content.y + 116.0f, 238.0f, 42.0f};
}

Rectangle account_delete_panel_rect() {
    return {
        kDialogRect.x + (kDialogRect.width - 560.0f) * 0.5f,
        kDialogRect.y + (kDialogRect.height - 232.0f) * 0.5f,
        560.0f,
        232.0f,
    };
}

Rectangle account_delete_password_rect() {
    const Rectangle panel = account_delete_panel_rect();
    return {panel.x + 32.0f, panel.y + 94.0f, panel.width - 64.0f, 42.0f};
}

Rectangle account_delete_confirm_rect() {
    const Rectangle panel = account_delete_panel_rect();
    return {panel.x + panel.width - 308.0f, panel.y + panel.height - 62.0f, 132.0f, 42.0f};
}

Rectangle account_delete_cancel_rect() {
    const Rectangle panel = account_delete_panel_rect();
    return {panel.x + panel.width - 164.0f, panel.y + panel.height - 62.0f, 132.0f, 42.0f};
}

std::string song_label(const auth::community_song_upload& song) {
    return song.title.empty() ? song.id : song.title;
}

std::string song_subtitle(const auth::community_song_upload& song) {
    if (!song.genre.empty() && !song.artist.empty()) {
        return song.artist + " / " + song.genre;
    }
    if (!song.genre.empty()) {
        return song.genre;
    }
    return song.artist;
}

std::string ranking_subtitle(const activity_item& item) {
    std::string result = item.artist;
    if (!item.genre.empty()) {
        result += result.empty() ? item.genre : " / " + item.genre;
    }
    if (!item.difficulty_name.empty()) {
        result += result.empty() ? item.difficulty_name : " / " + item.difficulty_name;
    }
    return result;
}

std::string profile_link_label(const auth::external_link& link) {
    return link.label.empty() ? link.url : link.label;
}

std::string chart_label(const auth::community_chart_upload& chart) {
    if (!chart.song_title.empty() && !chart.difficulty_name.empty()) {
        return chart.song_title + " / " + chart.difficulty_name;
    }
    if (!chart.difficulty_name.empty()) {
        return chart.difficulty_name;
    }
    return chart.id;
}

std::string make_avatar_label(const song_select::auth_state& auth_state) {
    const std::string source = auth_state.display_name.empty() ? auth_state.email : auth_state.display_name;
    if (source.empty()) {
        return "A";
    }

    std::string result;
    result.reserve(2);
    for (char ch : source) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            if (result.size() == 2) {
                break;
            }
        }
    }
    return result.empty() ? "A" : result;
}

void draw_profile_button(Rectangle rect, const char* label, bool enabled, Color tone) {
    const auto& t = *g_theme;
    const bool hovered = enabled && CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), rect);
    const Color bg = hovered ? t.row_hover : t.row;
    ui::draw_rect_f(rect, with_alpha(lerp_color(bg, tone, hovered ? 0.16f : 0.08f), enabled ? 220 : 95));
    ui::draw_rect_lines(rect, 1.5f, with_alpha(lerp_color(t.border, tone, 0.35f), enabled ? 220 : 115));
    ui::draw_text_in_rect(label, 13, rect, enabled ? t.text : t.text_muted);
}

void draw_empty(Rectangle content, const char* message) {
    ui::draw_section(content);
    ui::draw_text_in_rect(message, 15, content, g_theme->text_muted);
}

void draw_metric_card(Rectangle rect, const char* label, const std::string& value, Color tone) {
    const auto& t = *g_theme;
    ui::draw_rect_f(rect, with_alpha(t.row, 205));
    ui::draw_rect_lines(rect, 1.2f, with_alpha(lerp_color(t.border, tone, 0.35f), 210));
    ui::draw_text_in_rect(label, 12,
                          {rect.x + 18.0f, rect.y + 14.0f, rect.width - 36.0f, 22.0f},
                          t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect(value.c_str(), 23,
                          {rect.x + 18.0f, rect.y + 42.0f, rect.width - 36.0f, 34.0f},
                          tone, ui::text_align::left);
}

void draw_row_shell(Rectangle row) {
    const auto& t = *g_theme;
    const bool hovered = CheckCollisionPointRec(virtual_screen::get_virtual_mouse(), row);
    ui::draw_rect_f(row, with_alpha(hovered ? t.row_hover : t.row, hovered ? 225 : 195));
    ui::draw_rect_lines(row, 1.2f, with_alpha(t.border, 205));
}

tab tab_for_index(int index) {
    switch (index) {
    case 0:
        return tab::overview;
    case 1:
        return tab::activity;
    case 2:
        return tab::songs;
    case 3:
        return tab::charts;
    default:
        return tab::settings;
    }
}

bool is_upload_tab(tab selected_tab) {
    return selected_tab == tab::songs || selected_tab == tab::charts;
}

}  // namespace

Rectangle bounds() {
    return kDialogRect;
}

void open(state& profile) {
    if (!profile.open) {
        profile.open_anim = 0.0f;
    }
    profile.open = true;
    profile.closing = false;
    profile.pending_delete = delete_target::none;
    profile.pending_id.clear();
    profile.pending_label.clear();
    profile.delete_password_input.active = false;
    profile.delete_password_input.has_selection = false;
    profile.delete_password_input.mouse_selecting = false;
}

void close(state& profile) {
    if (profile.open) {
        profile.closing = true;
    } else {
        profile.open_anim = 0.0f;
    }
    profile.pending_delete = delete_target::none;
    profile.pending_id.clear();
    profile.pending_label.clear();
    profile.delete_password_input.active = false;
    profile.delete_password_input.has_selection = false;
    profile.delete_password_input.mouse_selecting = false;
}

void clamp_scroll(state& profile) {
    const Rectangle content = content_rect();
    profile.activity_scroll =
        std::clamp(profile.activity_scroll, 0.0f, max_scroll(content, static_cast<int>(profile.activity.size())));
    profile.song_scroll =
        std::clamp(profile.song_scroll, 0.0f, max_scroll(content, static_cast<int>(profile.uploads.songs.size())));
    profile.chart_scroll =
        std::clamp(profile.chart_scroll, 0.0f, max_scroll(content, static_cast<int>(profile.uploads.charts.size())));
}

command update(state& profile, bool request_active) {
    if (!profile.open) {
        return {};
    }
    if (profile.closing) {
        return {};
    }

    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const float wheel = GetMouseWheelMove();
    const bool busy = profile.loading || profile.deleting || request_active;
    const Rectangle content = content_rect();

    if (!busy && profile.pending_delete != delete_target::none &&
        (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        profile.pending_delete = delete_target::none;
        profile.pending_id.clear();
        profile.pending_label.clear();
        profile.delete_password_input.active = false;
        profile.delete_password_input.has_selection = false;
        profile.delete_password_input.mouse_selecting = false;
        return {};
    }

    if (!busy && (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        close(profile);
        return {.type = command_type::close};
    }

    if (!busy && wheel != 0.0f && CheckCollisionPointRec(mouse, content)) {
        if (profile.selected_tab == tab::activity) {
            profile.activity_scroll = std::clamp(
                profile.activity_scroll - wheel * kWheelStep, 0.0f,
                max_scroll(content, static_cast<int>(profile.activity.size())));
        } else if (profile.selected_tab == tab::songs) {
            profile.song_scroll = std::clamp(
                profile.song_scroll - wheel * kWheelStep, 0.0f,
                max_scroll(content, static_cast<int>(profile.uploads.songs.size())));
        } else if (profile.selected_tab == tab::charts) {
            profile.chart_scroll = std::clamp(
                profile.chart_scroll - wheel * kWheelStep, 0.0f,
                max_scroll(content, static_cast<int>(profile.uploads.charts.size())));
        }
    }

    if (!left_pressed) {
        return {};
    }

    if (profile.pending_delete != delete_target::none) {
        if (profile.pending_delete == delete_target::account) {
            if (!busy && CheckCollisionPointRec(mouse, account_delete_confirm_rect())) {
                return {.type = command_type::delete_account, .password = profile.delete_password_input.value};
            }
            if (!busy && CheckCollisionPointRec(mouse, account_delete_cancel_rect())) {
                profile.pending_delete = delete_target::none;
                profile.delete_password_input.active = false;
                profile.delete_password_input.has_selection = false;
                profile.delete_password_input.mouse_selecting = false;
            }
            return {};
        }
        if (!busy && CheckCollisionPointRec(mouse, confirm_rect())) {
            const command_type type =
                profile.pending_delete == delete_target::song ? command_type::delete_song : command_type::delete_chart;
            return {.type = type, .id = profile.pending_id};
        }
        if (!busy && CheckCollisionPointRec(mouse, cancel_rect())) {
            profile.pending_delete = delete_target::none;
            profile.pending_id.clear();
            profile.pending_label.clear();
        }
        return {};
    }

    if (!busy && CheckCollisionPointRec(mouse, close_rect())) {
        close(profile);
        return {.type = command_type::close};
    }

    for (int i = 0; i < kTabCount; ++i) {
        if (CheckCollisionPointRec(mouse, tab_rect(i))) {
            profile.selected_tab = tab_for_index(i);
            return {};
        }
    }

    if (!busy && profile.uploads.success && profile.selected_tab == tab::songs) {
        for (int i = 0; i < static_cast<int>(profile.uploads.songs.size()); ++i) {
            const Rectangle row = row_rect(content, i, profile.song_scroll);
            if (row.y + row.height < content.y || row.y > content.y + content.height) {
                continue;
            }
            if (CheckCollisionPointRec(mouse, row_action_rect(row))) {
                profile.pending_delete = delete_target::song;
                profile.pending_id = profile.uploads.songs[static_cast<size_t>(i)].id;
                profile.pending_label = song_label(profile.uploads.songs[static_cast<size_t>(i)]);
                return {};
            }
        }
    }

    if (!busy && profile.uploads.success && profile.selected_tab == tab::charts) {
        for (int i = 0; i < static_cast<int>(profile.uploads.charts.size()); ++i) {
            const Rectangle row = row_rect(content, i, profile.chart_scroll);
            if (row.y + row.height < content.y || row.y > content.y + content.height) {
                continue;
            }
            if (CheckCollisionPointRec(mouse, row_action_rect(row))) {
                profile.pending_delete = delete_target::chart;
                profile.pending_id = profile.uploads.charts[static_cast<size_t>(i)].id;
                profile.pending_label = chart_label(profile.uploads.charts[static_cast<size_t>(i)]);
                return {};
            }
        }
    }

    if (!busy && profile.selected_tab == tab::settings &&
        CheckCollisionPointRec(mouse, settings_delete_account_rect(content))) {
        profile.pending_delete = delete_target::account;
        profile.delete_password_input.value.clear();
        profile.delete_password_input.cursor = 0;
        profile.delete_password_input.has_selection = false;
        profile.delete_password_input.selection_anchor = 0;
        profile.delete_password_input.mouse_selecting = false;
        profile.delete_password_input.scroll_x = 0.0f;
        profile.delete_password_input.active = true;
        return {};
    }

    return {};
}

void draw(state& profile, const song_select::auth_state& auth_state, bool request_active, ui::draw_layer layer) {
    if (!profile.open) {
        return;
    }

    ui::enqueue_draw_command(layer, [&profile, auth_state, request_active, layer]() {
        const auto& t = *g_theme;
        const Vector2 mouse = virtual_screen::get_virtual_mouse();
        const bool busy = profile.loading || profile.deleting || request_active || profile.closing;
        const float anim_t = tween::ease_out_cubic(std::clamp(profile.open_anim, 0.0f, 1.0f));
        const float scale = 1.0f - (1.0f - anim_t) * kOpenAnimScaleInset;
        const float offset_y = (1.0f - anim_t) * kOpenAnimOffsetY;
        const Vector2 center = {
            kDialogRect.x + kDialogRect.width * 0.5f,
            kDialogRect.y + kDialogRect.height * 0.5f,
        };

        DrawRectangle(0, 0, kScreenWidth, kScreenHeight,
                      with_alpha(BLACK, static_cast<unsigned char>(160.0f * anim_t)));
        rlPushMatrix();
        rlTranslatef(center.x, center.y + offset_y, 0.0f);
        rlScalef(scale, scale, 1.0f);
        rlTranslatef(-center.x, -center.y, 0.0f);
        ui::draw_panel(kDialogRect);

        const Rectangle avatar = {kDialogRect.x + 44.0f, kDialogRect.y + 42.0f, 96.0f, 96.0f};
        ui::draw_rect_f(avatar, with_alpha(t.accent, 210));
        ui::draw_rect_lines(avatar, 2.0f, with_alpha(t.border_active, 230));
        ui::draw_text_in_rect(make_avatar_label(auth_state).c_str(), 28, avatar, t.bg);

        const std::string display_name =
            auth_state.display_name.empty() ? auth_state.email : auth_state.display_name;
        ui::draw_text_in_rect(display_name.c_str(), 27,
                              {avatar.x + avatar.width + 24.0f, avatar.y + 4.0f, 620.0f, 38.0f},
                              t.text, ui::text_align::left);
        ui::draw_text_in_rect(auth_state.email.c_str(), 14,
                              {avatar.x + avatar.width + 25.0f, avatar.y + 47.0f, 620.0f, 24.0f},
                              t.text_muted, ui::text_align::left);
        ui::draw_text_in_rect(auth_state.email_verified ? "Verified profile" : "Email verification pending",
                              13,
                              {avatar.x + avatar.width + 25.0f, avatar.y + 76.0f, 260.0f, 24.0f},
                              auth_state.email_verified ? t.success : t.error, ui::text_align::left);
        if (!auth_state.external_links.empty()) {
            std::string links_label;
            const size_t count = std::min<size_t>(auth_state.external_links.size(), 3);
            for (size_t i = 0; i < count; ++i) {
                if (i > 0) {
                    links_label += " / ";
                }
                links_label += profile_link_label(auth_state.external_links[i]);
            }
            ui::draw_text_in_rect(links_label.c_str(), 12,
                                  {avatar.x + avatar.width + 25.0f, avatar.y + 106.0f, 620.0f, 22.0f},
                                  t.accent, ui::text_align::left);
        }

        const std::string songs_count = std::to_string(profile.uploads.songs.size()) + " songs";
        const std::string charts_count = std::to_string(profile.uploads.charts.size()) + " charts";
        ui::draw_text_in_rect(songs_count.c_str(), 15,
                              {kDialogRect.x + 900.0f, avatar.y + 22.0f, 180.0f, 28.0f},
                              t.text, ui::text_align::left);
        ui::draw_text_in_rect(charts_count.c_str(), 15,
                              {kDialogRect.x + 900.0f, avatar.y + 58.0f, 180.0f, 28.0f},
                              t.text, ui::text_align::left);

        draw_profile_button(close_rect(), "CLOSE", !busy, t.text_muted);

        const Rectangle tab_bar = {
            kDialogRect.x + 42.0f,
            kDialogRect.y + kHeaderHeight,
            kDialogRect.width - 84.0f,
            kTabHeight,
        };
        ui::draw_rect_f(tab_bar, with_alpha(t.bg, 70));
        ui::draw_rect_lines({tab_bar.x, tab_bar.y + tab_bar.height - 1.0f, tab_bar.width, 1.0f},
                            1.0f, with_alpha(t.border, 180));

        const char* tab_labels[] = {"OVERVIEW", "ACTIVITY", "SONGS", "CHARTS", "SETTINGS"};
        for (int i = 0; i < kTabCount; ++i) {
            const tab current_tab = tab_for_index(i);
            const bool selected = current_tab == profile.selected_tab;
            const Rectangle rect = tab_rect(i);
            const bool hovered = CheckCollisionPointRec(mouse, rect);
            if (hovered || selected) {
                ui::draw_rect_f(rect, with_alpha(selected ? t.row_selected : t.row_hover, selected ? 135 : 90));
            }
            ui::draw_text_in_rect(tab_labels[i], 14, rect, selected ? t.text : t.text_secondary);
            if (selected) {
                ui::draw_rect_f({rect.x + 18.0f, rect.y + rect.height - 4.0f, rect.width - 36.0f, 3.0f},
                                with_alpha(t.accent, 235));
            }
        }

        const Rectangle content = content_rect();
        if (profile.loading && !profile.loaded_once) {
            draw_empty(content, "Loading profile...");
        } else if (profile.selected_tab == tab::overview) {
            ui::draw_section(content);
            draw_metric_card(overview_card_rect(content, 0), "Uploaded Songs",
                             std::to_string(profile.uploads.songs.size()), t.accent);
            draw_metric_card(overview_card_rect(content, 1), "Uploaded Charts",
                             std::to_string(profile.uploads.charts.size()), t.success);
            draw_metric_card(overview_card_rect(content, 2), "Recent Plays",
                             std::to_string(profile.activity.size()), t.text);
            draw_metric_card(overview_card_rect(content, 3), "#1 Records",
                             std::to_string(profile.first_place_records.size()), t.rank_ss);

            const Rectangle recent = {
                content.x,
                content.y + 118.0f,
                content.width,
                112.0f,
            };
            ui::draw_rect_f(recent, with_alpha(t.row, 185));
            ui::draw_rect_lines(recent, 1.2f, with_alpha(t.border, 205));
            ui::draw_text_in_rect("Recent Activity", 16,
                                  {recent.x + 18.0f, recent.y + 16.0f, recent.width - 36.0f, 26.0f},
                                  t.text, ui::text_align::left);
            if (profile.activity.empty()) {
                ui::draw_text_in_rect("No recent play activity yet.", 13,
                                      {recent.x + 18.0f, recent.y + 54.0f, recent.width - 36.0f, 22.0f},
                                      t.text_muted, ui::text_align::left);
            } else {
                const auto& item = profile.activity.front();
                ui::draw_text_in_rect(item.song_title.c_str(), 15,
                                      {recent.x + 18.0f, recent.y + 48.0f, 540.0f, 24.0f},
                                      t.text, ui::text_align::left);
                const std::string subtitle = ranking_subtitle(item);
                ui::draw_text_in_rect(subtitle.c_str(), 11,
                                      {recent.x + 18.0f, recent.y + 76.0f, 540.0f, 18.0f},
                                      t.text_muted, ui::text_align::left);
                ui::draw_text_in_rect(item.local_summary.c_str(), 13,
                                      {recent.x + 650.0f, recent.y + 50.0f, 310.0f, 22.0f},
                                      t.text_secondary, ui::text_align::left);
                ui::draw_text_in_rect(item.online_summary.c_str(), 13,
                                      {recent.x + 650.0f, recent.y + 76.0f, 360.0f, 22.0f},
                                      t.accent, ui::text_align::left);
            }

            const Rectangle first_place = {
                content.x,
                recent.y + recent.height + 12.0f,
                content.width,
                112.0f,
            };
            ui::draw_rect_f(first_place, with_alpha(t.row, 185));
            ui::draw_rect_lines(first_place, 1.2f, with_alpha(t.border, 205));
            ui::draw_text_in_rect("#1 Records", 16,
                                  {first_place.x + 18.0f, first_place.y + 16.0f,
                                   first_place.width - 36.0f, 26.0f},
                                  t.text, ui::text_align::left);
            if (profile.first_place_records.empty()) {
                ui::draw_text_in_rect("No #1 online records yet.", 13,
                                      {first_place.x + 18.0f, first_place.y + 54.0f,
                                       first_place.width - 36.0f, 22.0f},
                                      t.text_muted, ui::text_align::left);
            } else {
                const auto& item = profile.first_place_records.front();
                ui::draw_text_in_rect(item.song_title.c_str(), 15,
                                      {first_place.x + 18.0f, first_place.y + 48.0f, 540.0f, 24.0f},
                                      t.text, ui::text_align::left);
                const std::string subtitle = ranking_subtitle(item);
                ui::draw_text_in_rect(subtitle.c_str(), 11,
                                      {first_place.x + 18.0f, first_place.y + 76.0f, 540.0f, 18.0f},
                                      t.text_muted, ui::text_align::left);
                ui::draw_text_in_rect(item.online_summary.c_str(), 13,
                                      {first_place.x + 650.0f, first_place.y + 62.0f, 360.0f, 22.0f},
                                      t.rank_ss, ui::text_align::left);
            }
        } else if (profile.selected_tab == tab::activity) {
            if (profile.activity.empty()) {
                draw_empty(content, "No recent play activity yet.");
            } else {
                ui::scoped_clip_rect clip(content);
                for (int i = 0; i < static_cast<int>(profile.activity.size()); ++i) {
                    const Rectangle row = row_rect(content, i, profile.activity_scroll);
                    if (row.y + row.height < content.y || row.y > content.y + content.height) {
                        continue;
                    }
                    const auto& item = profile.activity[static_cast<size_t>(i)];
                    draw_row_shell(row);
                    ui::draw_text_in_rect(item.song_title.c_str(), 15,
                                          {row.x + 18.0f, row.y + 9.0f, 520.0f, 24.0f},
                                          t.text, ui::text_align::left);
                    const std::string subtitle = ranking_subtitle(item);
                    ui::draw_text_in_rect(subtitle.c_str(), 11,
                                          {row.x + 18.0f, row.y + 38.0f, 520.0f, 18.0f},
                                          t.text_muted, ui::text_align::left);
                    ui::draw_text_in_rect(item.local_summary.c_str(), 13,
                                          {row.x + 650.0f, row.y + 10.0f, 310.0f, 22.0f},
                                          t.text_secondary, ui::text_align::left);
                    ui::draw_text_in_rect(item.online_summary.c_str(), 13,
                                          {row.x + 650.0f, row.y + 38.0f, 360.0f, 22.0f},
                                          t.accent, ui::text_align::left);
                }
            }
        } else if (is_upload_tab(profile.selected_tab) && !profile.uploads.success) {
            draw_empty(content, profile.uploads.message.empty()
                                    ? "Uploaded content could not be loaded."
                                    : profile.uploads.message.c_str());
        } else if (profile.selected_tab == tab::songs) {
            if (profile.uploads.songs.empty()) {
                draw_empty(content, "No uploaded songs.");
            } else {
                ui::scoped_clip_rect clip(content);
                for (int i = 0; i < static_cast<int>(profile.uploads.songs.size()); ++i) {
                    const Rectangle row = row_rect(content, i, profile.song_scroll);
                    if (row.y + row.height < content.y || row.y > content.y + content.height) {
                        continue;
                    }
                    const auto& song = profile.uploads.songs[static_cast<size_t>(i)];
                    draw_row_shell(row);
                    ui::draw_text_in_rect(song_label(song).c_str(), 15,
                                          {row.x + 18.0f, row.y + 9.0f, row.width - 160.0f, 24.0f},
                                          t.text, ui::text_align::left);
                    const std::string subtitle = song_subtitle(song);
                    ui::draw_text_in_rect(subtitle.c_str(), 11,
                                          {row.x + 18.0f, row.y + 38.0f, row.width - 160.0f, 18.0f},
                                          t.text_muted, ui::text_align::left);
                    draw_profile_button(row_action_rect(row), "DELETE", !busy, t.error);
                }
            }
        } else if (profile.selected_tab == tab::charts) {
            if (profile.uploads.charts.empty()) {
                draw_empty(content, "No uploaded charts.");
            } else {
                ui::scoped_clip_rect clip(content);
                for (int i = 0; i < static_cast<int>(profile.uploads.charts.size()); ++i) {
                    const Rectangle row = row_rect(content, i, profile.chart_scroll);
                    if (row.y + row.height < content.y || row.y > content.y + content.height) {
                        continue;
                    }
                    const auto& chart = profile.uploads.charts[static_cast<size_t>(i)];
                    draw_row_shell(row);
                    ui::draw_text_in_rect(chart_label(chart).c_str(), 15,
                                          {row.x + 18.0f, row.y + 9.0f, row.width - 160.0f, 24.0f},
                                          t.text, ui::text_align::left);
                    ui::draw_text_in_rect(chart.chart_author.c_str(), 11,
                                          {row.x + 18.0f, row.y + 38.0f, row.width - 160.0f, 18.0f},
                                          t.text_muted, ui::text_align::left);
                    draw_profile_button(row_action_rect(row), "DELETE", !busy, t.error);
                }
            }
        } else {
            ui::draw_section(content);
            ui::draw_text_in_rect("Settings", 18,
                                  {content.x + 18.0f, content.y + 18.0f, content.width - 36.0f, 30.0f},
                                  t.text, ui::text_align::left);
            ui::draw_text_in_rect("Delete this account from raythm-Server.",
                                  13,
                                  {content.x + 18.0f, content.y + 56.0f, 560.0f, 22.0f},
                                  t.text_secondary, ui::text_align::left);
            ui::draw_text_in_rect("This does not delete local songs or charts.",
                                  12,
                                  {content.x + 18.0f, content.y + 78.0f, 560.0f, 20.0f},
                                  t.text_muted, ui::text_align::left);
            draw_profile_button(settings_delete_account_rect(content), "DELETE ACCOUNT", !busy, t.error);
        }

        if (profile.loading && profile.loaded_once) {
            ui::draw_text_in_rect("Refreshing...", 13,
                                  {kDialogRect.x + 42.0f, kDialogRect.y + kDialogRect.height - 104.0f,
                                   260.0f, 24.0f},
                                  t.text_muted, ui::text_align::left);
        }
        if (profile.deleting) {
            ui::draw_text_in_rect("Deleting...", 13,
                                  {kDialogRect.x + 42.0f, kDialogRect.y + kDialogRect.height - 104.0f,
                                   260.0f, 24.0f},
                                  t.text_muted, ui::text_align::left);
        }

        if (profile.pending_delete == delete_target::account) {
            const Rectangle confirm_panel = account_delete_panel_rect();
            ui::draw_rect_f(confirm_panel, with_alpha(t.panel, 248));
            ui::draw_rect_lines(confirm_panel, 2.0f, with_alpha(t.error, 220));
            ui::draw_text_in_rect("Delete Account", 20,
                                  {confirm_panel.x + 32.0f, confirm_panel.y + 24.0f,
                                   confirm_panel.width - 64.0f, 30.0f},
                                  t.text, ui::text_align::left);
            ui::draw_text_in_rect("Enter your password to permanently delete this server account.",
                                  12,
                                  {confirm_panel.x + 32.0f, confirm_panel.y + 58.0f,
                                   confirm_panel.width - 64.0f, 22.0f},
                                  t.text_secondary, ui::text_align::left);
            ui::draw_text_input(account_delete_password_rect(), profile.delete_password_input,
                                "Pass", "Delete account password", nullptr,
                                layer, 13, 64, ui::default_text_input_filter, 92.0f, true);
            draw_profile_button(account_delete_confirm_rect(), "DELETE", !busy, t.error);
            draw_profile_button(account_delete_cancel_rect(), "CANCEL", !busy, t.text_muted);
        } else if (profile.pending_delete != delete_target::none) {
            const Rectangle confirm_panel = {
                kDialogRect.x + 540.0f,
                kDialogRect.y + kDialogRect.height - 124.0f,
                600.0f,
                74.0f,
            };
            ui::draw_rect_f(confirm_panel, with_alpha(t.panel, 245));
            ui::draw_rect_lines(confirm_panel, 2.0f, with_alpha(t.error, 210));
            const std::string message = "Delete " + profile.pending_label + "? Hidden from Community listings.";
            ui::draw_text_in_rect(message.c_str(), 12,
                                  {confirm_panel.x + 18.0f, confirm_panel.y + 14.0f,
                                   confirm_panel.width - 36.0f, 22.0f},
                                  t.text, ui::text_align::left);
            draw_profile_button(confirm_rect(), "DELETE", !busy, t.error);
            draw_profile_button(cancel_rect(), "CANCEL", !busy, t.text_muted);
        }
        rlPopMatrix();
    });
}

}  // namespace title_profile_view
