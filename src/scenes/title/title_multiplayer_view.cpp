#include "title/title_multiplayer_view.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <string>
#include <thread>
#include <utility>

#include "network/auth_client.h"
#include "scene_common.h"
#include "theme.h"
#include "title/title_layout.h"
#include "tween.h"
#include "ui_draw.h"
#include "ui_hit.h"

namespace {

constexpr Rectangle kBackRect = {39.0f, 983.0f, 330.0f, 58.0f};
constexpr Rectangle kWideMainRect = {390.0f, 109.0f, 1488.0f, 932.0f};
constexpr float kPanelPadding = 24.0f;
constexpr float kRowHeight = 78.0f;
constexpr float kRowGap = 10.0f;

Rectangle moved(Rectangle rect, float play_view_anim, Rectangle origin) {
    const float t = tween::ease_out_cubic(play_view_anim);
    if (origin.width <= 0.0f) {
        return rect;
    }
    const Vector2 origin_center = {origin.x + origin.width * 0.5f, origin.y + origin.height * 0.5f};
    const Vector2 rect_center = {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
    rect.x += (origin_center.x - rect_center.x) * (1.0f - t);
    rect.y += (origin_center.y - rect_center.y) * (1.0f - t);
    return rect;
}

Rectangle back_rect(float anim, Rectangle origin) {
    return moved(kBackRect, anim, origin);
}

Rectangle refresh_rect(float anim, Rectangle origin) {
    return moved({kWideMainRect.x + kPanelPadding, kWideMainRect.y + kWideMainRect.height - 76.0f,
                  200.0f, 52.0f}, anim, origin);
}

Rectangle open_setup_rect(float anim, Rectangle origin) {
    return moved({kWideMainRect.x + kWideMainRect.width - kPanelPadding - 220.0f,
                  kWideMainRect.y + kWideMainRect.height - 76.0f, 220.0f, 52.0f}, anim, origin);
}

Rectangle create_rect(float anim, Rectangle origin) {
    return moved({kWideMainRect.x + kPanelPadding + 450.0f, kWideMainRect.y + 232.0f,
                  220.0f, 50.0f}, anim, origin);
}

Rectangle join_room_rect(float anim, Rectangle origin) {
    return moved({kWideMainRect.x + kWideMainRect.width - kPanelPadding - 220.0f,
                  kWideMainRect.y + kWideMainRect.height - 76.0f, 220.0f, 52.0f}, anim, origin);
}

Rectangle ready_rect(float anim, Rectangle origin) {
    return moved({kWideMainRect.x + kPanelPadding, kWideMainRect.y + kWideMainRect.height - 74.0f,
                  176.0f, 52.0f}, anim, origin);
}

Rectangle start_rect(float anim, Rectangle origin) {
    return moved({kWideMainRect.x + kPanelPadding + 194.0f, kWideMainRect.y + kWideMainRect.height - 74.0f,
                  176.0f, 52.0f}, anim, origin);
}

Rectangle leave_rect(float anim, Rectangle origin) {
    return moved({kWideMainRect.x + kWideMainRect.width - kPanelPadding - 176.0f,
                  kWideMainRect.y + kWideMainRect.height - 74.0f, 176.0f, 52.0f}, anim, origin);
}

Rectangle room_row_rect(Rectangle list_panel, int index) {
    return {list_panel.x + kPanelPadding,
            list_panel.y + 96.0f + static_cast<float>(index) * (kRowHeight + kRowGap),
            list_panel.width - kPanelPadding * 2.0f,
            kRowHeight};
}

Rectangle visibility_rect(float anim, Rectangle origin, int index) {
    const Rectangle base = {kWideMainRect.x + kPanelPadding + static_cast<float>(index) * 156.0f,
                            kWideMainRect.y + 168.0f, 140.0f, 44.0f};
    return moved(base, anim, origin);
}

Rectangle password_rect(float anim, Rectangle origin) {
    return moved({kWideMainRect.x + kPanelPadding, kWideMainRect.y + 232.0f, 430.0f, 50.0f}, anim, origin);
}

Rectangle apply_song_rect(float anim, Rectangle origin) {
    return moved({kWideMainRect.x + kWideMainRect.width - 220.0f, kWideMainRect.y + 232.0f,
                  196.0f, 46.0f}, anim, origin);
}

Rectangle song_row_rect(Rectangle song_panel, int index) {
    return {song_panel.x + 14.0f, song_panel.y + 48.0f + static_cast<float>(index) * 48.0f,
            song_panel.width * 0.46f - 20.0f, 40.0f};
}

Rectangle chart_row_rect(Rectangle song_panel, int index) {
    const float x = song_panel.x + song_panel.width * 0.48f;
    return {x, song_panel.y + 48.0f + static_cast<float>(index) * 48.0f,
            song_panel.x + song_panel.width - x - 14.0f, 40.0f};
}

multiplayer_client::room_settings selected_room_settings(const title_multiplayer_view::state& state) {
    return state.playable_catalog.selected_room_settings(state.selected_song_index, state.selected_chart_index);
}

bool selected_online_chart_installed(const title_multiplayer_view::state& state) {
    return state.playable_catalog.selected_chart_installed(state.selected_song_index, state.selected_chart_index);
}

bool can_submit_room_settings(const title_multiplayer_view::state& state) {
    return !state.loading && selected_online_chart_installed(state);
}

template <typename Work>
void start_request(title_multiplayer_view::state& state, std::string label, Work work) {
    if (state.loading) {
        return;
    }
    state.loading = true;
    state.loading_label = std::move(label);
    state.status_message = state.loading_label + "...";

    std::promise<multiplayer_client::operation_result> promise;
    state.pending = promise.get_future();
    std::thread([promise = std::move(promise), work = std::move(work)]() mutable {
        try {
            const std::optional<auth::session> session = auth::load_saved_session();
            if (!session.has_value()) {
                multiplayer_client::operation_result result;
                result.message = "Sign in to use multiplayer.";
                result.unauthorized = true;
                promise.set_value(std::move(result));
                return;
            }
            promise.set_value(work(*session));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void request_list(title_multiplayer_view::state& state,
                  const song_select::state& play_state) {
    if (state.loading) {
        return;
    }
    const std::vector<song_select::song_entry> local_songs = play_state.songs;
    state.online_content_local_song_count = local_songs.size();
    start_request(state, "Loading rooms", [local_songs](const auth::session& session) {
        multiplayer_client::operation_result rooms =
            multiplayer_client::list_rooms(session.server_url, session.access_token);
        if (!rooms.success) {
            return rooms;
        }
        title_multiplayer_content::online_content_result content =
            title_multiplayer_content::load_online_content(session.server_url, local_songs);
        if (content.success) {
            rooms.online_songs = std::move(content.songs);
            rooms.server_url = content.server_url;
        } else if (!content.message.empty()) {
            rooms.message = content.message;
        }
        if (rooms.server_url.empty()) {
            rooms.server_url = session.server_url;
        }
        rooms.online_content_loaded = true;
        return rooms;
    });
}

void request_refresh(title_multiplayer_view::state& state,
                     const song_select::state& play_state) {
    if (!state.current_room.has_value()) {
        request_list(state, play_state);
        return;
    }

    const std::string room_id = state.current_room->room_id;
    start_request(state, "Refreshing room", [room_id](const auth::session& session) {
        return multiplayer_client::fetch_room(session.server_url, session.access_token, room_id);
    });
}

void request_apply_settings(title_multiplayer_view::state& state,
                            const song_select::state&) {
    if (!state.current_room.has_value()) {
        return;
    }

    const std::string room_id = state.current_room->room_id;
    const multiplayer_client::room_settings settings = selected_room_settings(state);
    const std::string visibility = state.private_room ? "private" : "public";
    const std::string password = state.setup_password_input.value;
    start_request(state, "Updating room", [room_id, settings, visibility, password](const auth::session& session) {
        return multiplayer_client::update_room_settings(
            session.server_url,
            session.access_token,
            room_id,
            settings,
            visibility,
            password);
    });
}

void apply_result(title_multiplayer_view::state& state,
                  multiplayer_client::operation_result result,
                  const song_select::state& play_state) {
    state.loading = false;
    state.loading_label.clear();
    if (!result.success) {
        state.status_message = result.message.empty() ? "Multiplayer request failed." : result.message;
        return;
    }
    if (result.room_list_loaded) {
        state.rooms = std::move(result.rooms);
        state.selected_room_index = std::clamp(state.selected_room_index, 0,
                                              std::max(0, static_cast<int>(state.rooms.size()) - 1));
        state.status_message = state.rooms.empty() ? "No public rooms yet." : "Room list updated.";
    }
    if (result.online_content_loaded) {
        state.playable_catalog.replace(std::move(result.online_songs), result.server_url, play_state.songs);
        state.selected_song_index = state.playable_catalog.clamped_song_index(state.selected_song_index);
        state.selected_chart_index = 0;
    }
    if (result.room.has_value()) {
        if (result.room->status == "closed") {
            state.current_room.reset();
            state.play_launch_room_id.clear();
            state.current_screen = title_multiplayer_view::screen::browser;
            state.status_message = "Room closed.";
            request_list(state, play_state);
            return;
        }
        state.current_room = std::move(result.room);
        state.current_screen = title_multiplayer_view::screen::room;
        state.status_message = "Room updated.";
    }
}

void poll_request(title_multiplayer_view::state& state, const song_select::state& play_state) {
    if (!state.loading || state.pending.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }
    try {
        apply_result(state, state.pending.get(), play_state);
    } catch (const std::exception& ex) {
        state.loading = false;
        state.status_message = ex.what();
    } catch (...) {
        state.loading = false;
        state.status_message = "Multiplayer request failed.";
    }
}

void draw_button(Rectangle rect, const char* label, bool enabled) {
    const auto& t = *g_theme;
    const bool hovered = enabled && ui::is_hovered(rect);
    ui::draw_rect_f(rect, enabled ? (hovered ? t.row_selected_hover : t.row_selected) : t.row);
    ui::draw_rect_lines(rect, 1.8f, enabled ? t.border_active : t.border);
    ui::draw_text_in_rect(label, 16, rect, enabled ? t.text : t.text_hint, ui::text_align::center);
}

void draw_segment(Rectangle rect, const char* label, bool selected, bool enabled) {
    const auto& t = *g_theme;
    const bool hovered = enabled && ui::is_hovered(rect);
    ui::draw_rect_f(rect, selected ? t.row_selected : (hovered ? t.row_hover : t.row_soft));
    ui::draw_rect_lines(rect, 1.4f, selected ? t.border_active : t.border);
    ui::draw_text_in_rect(label, 14, rect, enabled ? t.text : t.text_hint, ui::text_align::center);
}

void draw_room_summary(const multiplayer_client::room_state& room, Rectangle rect, bool selected) {
    const auto& t = *g_theme;
    ui::draw_rect_f(rect, selected ? t.row_selected : t.row_soft);
    ui::draw_rect_lines(rect, 1.5f, selected ? t.border_active : t.border);
    const std::string lock = room.requires_password ? "LOCKED" : (room.visibility == "private" ? "PRIVATE" : "PUBLIC");
    ui::draw_text_in_rect(room.room_code.c_str(), 20,
                          {rect.x + 14.0f, rect.y + 9.0f, 106.0f, 28.0f},
                          t.text, ui::text_align::left);
    const std::string meta = room.status + " / " + std::to_string(room.members.size()) + " players";
    ui::draw_text_in_rect(meta.c_str(), 12,
                          {rect.x + 14.0f, rect.y + 38.0f, 146.0f, 22.0f},
                          t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect(lock.c_str(), 11,
                          {rect.x + rect.width - 78.0f, rect.y + 9.0f, 64.0f, 22.0f},
                          room.requires_password || room.visibility == "private" ? t.text_secondary : t.accent,
                          ui::text_align::right);
    const std::string chart = room.settings.selected_chart_id.empty()
        ? "No chart selected"
        : room.settings.selected_chart_id;
    ui::draw_text_in_rect(chart.c_str(), 12,
                          {rect.x + 14.0f, rect.y + 58.0f, rect.width - 28.0f, 18.0f},
                          t.text_secondary, ui::text_align::right);
}

void draw_setting_row(const char* label, const char* value, Rectangle rect) {
    const auto& t = *g_theme;
    ui::draw_rect_f(rect, t.row_soft);
    ui::draw_rect_lines(rect, 1.1f, t.border);
    ui::draw_text_in_rect(label, 13, {rect.x + 14.0f, rect.y, 190.0f, rect.height},
                          t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect(value, 14, {rect.x + 214.0f, rect.y, rect.width - 228.0f, rect.height},
                          t.text, ui::text_align::right);
}

void request_join_room(title_multiplayer_view::state& state, const std::string& room_id, const std::string& password) {
    start_request(state, "Joining room", [room_id, password](const auth::session& session) {
        return multiplayer_client::join_room(session.server_url, session.access_token, room_id, password);
    });
}

}  // namespace

namespace title_multiplayer_view {

void on_enter(state& state, const song_select::state& play_state) {
    state.entered = true;
    request_list(state, play_state);
}

update_result update(state& state,
                     const song_select::state& play_state,
                     float play_view_anim,
                     Rectangle entry_origin_rect,
                     float) {
    update_result result;
    if (!state.entered) {
        on_enter(state, play_state);
    }
    poll_request(state, play_state);

    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (state.current_screen == screen::browser) {
            result.back_requested = true;
        } else {
            state.current_screen = screen::browser;
        }
        return result;
    }

    if (ui::is_clicked(back_rect(play_view_anim, entry_origin_rect))) {
        if (state.current_screen == screen::browser) {
            result.back_requested = true;
        } else {
            state.current_screen = screen::browser;
        }
        return result;
    }

    if (state.current_screen == screen::browser && ui::is_clicked(open_setup_rect(play_view_anim, entry_origin_rect))) {
        state.current_screen = screen::setup;
        if (state.playable_catalog.empty() &&
            !play_state.songs.empty() &&
            state.online_content_local_song_count == 0) {
            request_list(state, play_state);
        }
        return result;
    }

    if (state.current_screen == screen::password_prompt &&
        ui::is_clicked(join_room_rect(play_view_anim, entry_origin_rect)) &&
        state.selected_room_index >= 0 &&
        state.selected_room_index < static_cast<int>(state.rooms.size())) {
        const std::string room_id = state.rooms[static_cast<size_t>(state.selected_room_index)].room_id;
        const std::string password = state.join_password_input.value;
        request_join_room(state, room_id, password);
        return result;
    }

    if (state.current_screen == screen::browser && ui::is_clicked(refresh_rect(play_view_anim, entry_origin_rect))) {
        request_refresh(state, play_state);
    }
    if (state.current_screen == screen::setup && ui::is_clicked(visibility_rect(play_view_anim, entry_origin_rect, 0))) {
        state.private_room = false;
    }
    if (state.current_screen == screen::setup && ui::is_clicked(visibility_rect(play_view_anim, entry_origin_rect, 1))) {
        state.private_room = true;
    }
    if (state.current_screen == screen::setup &&
        can_submit_room_settings(state) &&
        ui::is_clicked(create_rect(play_view_anim, entry_origin_rect))) {
        const multiplayer_client::room_settings settings = selected_room_settings(state);
        const std::string visibility = state.private_room ? "private" : "public";
        const std::string password = state.setup_password_input.value;
        start_request(state, "Creating room", [settings, visibility, password](const auth::session& session) {
            return multiplayer_client::create_room(session.server_url, session.access_token, settings, visibility, password);
        });
    }
    if ((state.current_screen == screen::setup || state.current_screen == screen::room) &&
        can_submit_room_settings(state) &&
        ui::is_clicked(apply_song_rect(play_view_anim, entry_origin_rect))) {
        request_apply_settings(state, play_state);
    }

    if (state.current_screen == screen::browser) {
        const Rectangle list_rect = moved(kWideMainRect, play_view_anim, entry_origin_rect);
        for (int index = 0; index < static_cast<int>(state.rooms.size()); ++index) {
            const Rectangle row = room_row_rect(list_rect, index);
            if (CheckCollisionRecs(row, list_rect) && ui::is_clicked(row)) {
                state.selected_room_index = index;
                const multiplayer_client::room_state& room = state.rooms[static_cast<size_t>(index)];
                if (room.requires_password) {
                    state.join_password_input.value.clear();
                    state.join_password_input.cursor = 0;
                    state.current_screen = screen::password_prompt;
                } else {
                    request_join_room(state, room.room_id, "");
                }
            }
        }
    }

    if (state.current_screen == screen::setup || state.current_screen == screen::room) {
        if (state.playable_catalog.empty() &&
            !state.loading &&
            !play_state.songs.empty() &&
            state.online_content_local_song_count == 0) {
            request_list(state, play_state);
        }
        const Rectangle song_panel = moved({kWideMainRect.x + kPanelPadding, kWideMainRect.y + 330.0f,
                                            kWideMainRect.width - kPanelPadding * 2.0f, 560.0f},
                                           play_view_anim, entry_origin_rect);
        for (int index = 0; index < static_cast<int>(state.playable_catalog.size()); ++index) {
            const Rectangle row = song_row_rect(song_panel, index);
            if (row.y + row.height > song_panel.y + song_panel.height) {
                break;
            }
            if (ui::is_clicked(row)) {
                state.selected_song_index = index;
                state.selected_chart_index = 0;
            }
        }
        if (!state.playable_catalog.empty()) {
            const multiplayer_client::online_song* song = state.playable_catalog.song_at(state.selected_song_index);
            const auto& charts = song->charts;
            for (int index = 0; index < static_cast<int>(charts.size()); ++index) {
                const Rectangle row = chart_row_rect(song_panel, index);
                if (row.y + row.height > song_panel.y + song_panel.height) {
                    break;
                }
                if (ui::is_clicked(row)) {
                    state.selected_chart_index = index;
                }
            }
        }
    }

    if (state.current_screen == screen::room && state.current_room.has_value()) {
        const multiplayer_client::room_state& room = *state.current_room;
        const auto me = auth::load_session_summary();
        const bool ready = std::any_of(room.members.begin(), room.members.end(), [&](const auto& member) {
            return member.display_name == me.display_name && member.ready;
        });
        if (ui::is_clicked(ready_rect(play_view_anim, entry_origin_rect))) {
            const std::string room_id = room.room_id;
            start_request(state, ready ? "Clearing ready" : "Setting ready",
                          [room_id, ready](const auth::session& session) {
                              return multiplayer_client::set_ready(session.server_url, session.access_token,
                                                                   room_id, !ready);
                          });
        }
        if (ui::is_clicked(start_rect(play_view_anim, entry_origin_rect))) {
            const std::string room_id = room.room_id;
            start_request(state, "Starting room", [room_id](const auth::session& session) {
                return multiplayer_client::start_room(session.server_url, session.access_token, room_id);
            });
        }
        if (ui::is_clicked(leave_rect(play_view_anim, entry_origin_rect))) {
            const std::string room_id = room.room_id;
            start_request(state, "Leaving room", [room_id](const auth::session& session) {
                return multiplayer_client::leave_room(session.server_url, session.access_token, room_id);
            });
        }
    }

    return result;
}

void draw(state& state, const song_select::state& play_state, float play_view_anim, Rectangle entry_origin_rect) {
    const auto& t = *g_theme;
    const Rectangle back = back_rect(play_view_anim, entry_origin_rect);
    draw_button(back, state.current_screen == screen::browser ? "BACK" : "ROOM LIST", true);

    const Rectangle main_panel = moved(kWideMainRect, play_view_anim, entry_origin_rect);
    ui::draw_rect_f(main_panel, t.panel);
    ui::draw_rect_lines(main_panel, 1.5f, t.border);

    if (state.current_screen == screen::browser) {
        ui::draw_text_in_rect("MULTIPLAY ROOMS", 26,
                              {main_panel.x + kPanelPadding, main_panel.y + 18.0f,
                               main_panel.width - kPanelPadding * 2.0f, 38.0f},
                              t.text, ui::text_align::left);
        ui::draw_text_in_rect(state.loading ? state.loading_label.c_str() : state.status_message.c_str(), 14,
                              {main_panel.x + 360.0f, main_panel.y + 24.0f,
                               main_panel.width - 390.0f, 28.0f},
                              t.text_muted, ui::text_align::right);
        const Rectangle list_rect = {main_panel.x, main_panel.y + 74.0f, main_panel.width, main_panel.height - 168.0f};
        if (state.rooms.empty()) {
            ui::draw_text_in_rect("No public rooms.", 20, list_rect, t.text_muted, ui::text_align::center);
        } else {
            ui::begin_scissor_rect(list_rect);
            for (int index = 0; index < static_cast<int>(state.rooms.size()); ++index) {
                const Rectangle row = room_row_rect(main_panel, index);
                if (row.y + row.height >= list_rect.y && row.y <= list_rect.y + list_rect.height) {
                    draw_room_summary(state.rooms[static_cast<size_t>(index)], row, index == state.selected_room_index);
                }
            }
            EndScissorMode();
        }
        draw_button(refresh_rect(play_view_anim, entry_origin_rect), "REFRESH", !state.loading);
        draw_button(open_setup_rect(play_view_anim, entry_origin_rect), "CREATE ROOM", !state.loading);
        return;
    }

    if (state.current_screen == screen::password_prompt) {
        ui::draw_text_in_rect("ROOM PASSWORD", 26,
                              {main_panel.x + kPanelPadding, main_panel.y + 18.0f,
                               main_panel.width - kPanelPadding * 2.0f, 38.0f},
                              t.text, ui::text_align::left);
        if (state.selected_room_index >= 0 && state.selected_room_index < static_cast<int>(state.rooms.size())) {
            const multiplayer_client::room_state& room = state.rooms[static_cast<size_t>(state.selected_room_index)];
            draw_room_summary(room,
                              {main_panel.x + kPanelPadding, main_panel.y + 96.0f,
                               main_panel.width - kPanelPadding * 2.0f, kRowHeight},
                              true);
        }
        ui::draw_text_input({main_panel.x + kPanelPadding, main_panel.y + 214.0f, 460.0f, 54.0f},
                            state.join_password_input, "Password", "required",
                            nullptr, ui::draw_layer::base, 16, 64,
                            ui::default_text_input_filter, 98.0f, true);
        ui::draw_text_in_rect(state.loading ? state.loading_label.c_str() : state.status_message.c_str(), 14,
                              {main_panel.x + kPanelPadding, main_panel.y + 292.0f,
                               main_panel.width - kPanelPadding * 2.0f, 28.0f},
                              t.text_muted, ui::text_align::left);
        draw_button(join_room_rect(play_view_anim, entry_origin_rect), "JOIN ROOM", !state.loading);
        return;
    }

    const bool room_screen = state.current_screen == screen::room && state.current_room.has_value();
    ui::draw_text_in_rect(room_screen ? "ROOM" : "ROOM SETUP", 26,
                          {main_panel.x + kPanelPadding, main_panel.y + 18.0f,
                           main_panel.width - kPanelPadding * 2.0f, 38.0f},
                          t.text, ui::text_align::left);

    if (room_screen) {
        const multiplayer_client::room_state& room = *state.current_room;
        const std::string heading = "CODE " + room.room_code + " / " + room.status;
        ui::draw_text_in_rect(heading.c_str(), 18,
                              {main_panel.x + kPanelPadding, main_panel.y + 70.0f, 520.0f, 30.0f},
                              t.text_secondary, ui::text_align::left);
        const Rectangle member_rect = {main_panel.x + main_panel.width - 530.0f, main_panel.y + 86.0f,
                                       506.0f, main_panel.height - 176.0f};
        ui::draw_rect_f(member_rect, t.section);
        ui::draw_text_in_rect("MEMBERS", 14, {member_rect.x + 14.0f, member_rect.y + 12.0f, 200.0f, 24.0f},
                              t.text_muted, ui::text_align::left);
        for (int index = 0; index < static_cast<int>(room.members.size()); ++index) {
            const auto& member = room.members[static_cast<size_t>(index)];
            const Rectangle row = {member_rect.x + 14.0f, member_rect.y + 48.0f + static_cast<float>(index) * 66.0f,
                                   member_rect.width - 28.0f, 56.0f};
            ui::draw_rect_f(row, member.ready ? t.row_selected : t.row_soft);
            const std::string name = member.display_name + (member.connected ? "" : " / disconnected");
            ui::draw_text_in_rect(name.c_str(), 16, {row.x + 14.0f, row.y, 260.0f, row.height},
                                  t.text, ui::text_align::left);
            ui::draw_text_in_rect(member.ready ? "READY" : "WAIT", 14,
                                  {row.x + row.width - 110.0f, row.y, 96.0f, row.height},
                                  member.ready ? t.text : t.text_muted, ui::text_align::right);
        }
        draw_button(ready_rect(play_view_anim, entry_origin_rect), "READY", !state.loading);
        draw_button(start_rect(play_view_anim, entry_origin_rect), "START", !state.loading);
        draw_button(leave_rect(play_view_anim, entry_origin_rect), "LEAVE", !state.loading);
    }

    ui::draw_text_in_rect("OPTIONS", 16,
                          {main_panel.x + kPanelPadding, main_panel.y + 128.0f, 300.0f, 28.0f},
                          t.text_secondary, ui::text_align::left);
    draw_segment(visibility_rect(play_view_anim, entry_origin_rect, 0), "PUBLIC",
                 !state.private_room, !state.loading);
    draw_segment(visibility_rect(play_view_anim, entry_origin_rect, 1), "PRIVATE",
                 state.private_room, !state.loading);
    ui::draw_text_input(password_rect(play_view_anim, entry_origin_rect),
                        state.setup_password_input, "Password", "optional",
                        nullptr, ui::draw_layer::base, 15, 24,
                        ui::default_text_input_filter, 94.0f, true);
    draw_button(room_screen ? apply_song_rect(play_view_anim, entry_origin_rect)
                            : create_rect(play_view_anim, entry_origin_rect),
                room_screen ? "APPLY SONG" : "CREATE ROOM",
                can_submit_room_settings(state));

    const Rectangle song_panel = {main_panel.x + kPanelPadding, main_panel.y + 330.0f,
                                  room_screen ? main_panel.width - 578.0f : main_panel.width - kPanelPadding * 2.0f,
                                  560.0f};
    ui::draw_rect_f(song_panel, t.section);
    ui::draw_text_in_rect("ONLINE SONGS", 12,
                          {song_panel.x + 14.0f, song_panel.y + 12.0f, song_panel.width * 0.45f, 24.0f},
                          t.text_muted, ui::text_align::left);
    ui::draw_text_in_rect("CHARTS", 12,
                          {song_panel.x + song_panel.width * 0.48f, song_panel.y + 12.0f,
                           song_panel.width * 0.5f, 24.0f},
                          t.text_muted, ui::text_align::left);
    if (state.playable_catalog.empty()) {
        ui::draw_text_in_rect("No online charts available.", 18, song_panel, t.text_muted, ui::text_align::center);
        return;
    }

    ui::begin_scissor_rect(song_panel);
    const int selected_song = state.playable_catalog.clamped_song_index(state.selected_song_index);
    const auto& songs = state.playable_catalog.songs();
    for (int index = 0; index < static_cast<int>(songs.size()); ++index) {
        const Rectangle row = song_row_rect(song_panel, index);
        if (row.y + row.height > song_panel.y + song_panel.height) {
            break;
        }
        const bool selected = index == selected_song;
        ui::draw_rect_f(row, selected ? t.row_selected : t.row_soft);
        ui::draw_rect_lines(row, 1.0f, selected ? t.border_active : t.border);
        const auto& song = songs[static_cast<size_t>(index)];
        const std::string label = song.artist.empty() ? song.title : song.title + " / " + song.artist;
        ui::draw_text_in_rect(label.c_str(),
                              13, ui::inset(row, 8.0f), t.text, ui::text_align::left);
        ui::draw_text_in_rect(song.installed ? "DOWNLOADED" : "NOT DOWNLOADED",
                              11,
                              {row.x + row.width - 132.0f, row.y, 124.0f, row.height},
                              song.installed ? t.accent : t.text_muted,
                              ui::text_align::right);
    }

    const auto& charts = songs[static_cast<size_t>(selected_song)].charts;
    const int selected_chart = charts.empty()
        ? 0
        : std::clamp(state.selected_chart_index, 0, static_cast<int>(charts.size()) - 1);
    for (int index = 0; index < static_cast<int>(charts.size()); ++index) {
        const Rectangle row = chart_row_rect(song_panel, index);
        if (row.y + row.height > song_panel.y + song_panel.height) {
            break;
        }
        const bool selected = index == selected_chart;
        ui::draw_rect_f(row, selected ? t.row_selected : t.row_soft);
        ui::draw_rect_lines(row, 1.0f, selected ? t.border_active : t.border);
        const auto& chart = charts[static_cast<size_t>(index)];
        const std::string level = chart.level > 0 ? " Lv." + std::to_string(chart.level) : "";
        const std::string label = std::to_string(chart.key_count) + "K / " + chart.difficulty_name + level;
        ui::draw_text_in_rect(label.c_str(), 13, ui::inset(row, 8.0f), t.text, ui::text_align::left);
        ui::draw_text_in_rect(chart.installed ? "READY" : "MISSING",
                              11,
                              {row.x + row.width - 84.0f, row.y, 76.0f, row.height},
                              chart.installed ? t.accent : t.text_muted,
                              ui::text_align::right);
    }
    EndScissorMode();
}

}  // namespace title_multiplayer_view
