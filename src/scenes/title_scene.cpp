#include "title_scene.h"

#include <array>
#include <cctype>
#include <memory>
#include <string>

#include "raylib.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select_scene.h"
#include "song_select/song_select_layout.h"
#include "song_select/song_select_navigation.h"
#include "theme.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

constexpr Rectangle kScreenRect = {0.0f, 0.0f, static_cast<float>(kScreenWidth), static_cast<float>(kScreenHeight)};
constexpr Rectangle kTitleClosedHeaderRect = ui::place(kScreenRect, 860.0f, 182.0f,
                                                       ui::anchor::center, ui::anchor::center,
                                                       {0.0f, -54.0f});
constexpr Rectangle kTitleOpenHeaderRect = ui::place(kScreenRect, 760.0f, 170.0f,
                                                     ui::anchor::top_left, ui::anchor::top_left,
                                                     {72.0f, 84.0f});
constexpr Rectangle kSpectrumRect = ui::place(kScreenRect, 1040.0f, 332.0f,
                                              ui::anchor::bottom_center, ui::anchor::bottom_center,
                                              {0.0f, -56.0f});
constexpr Rectangle kAccountChipRect = ui::place(kScreenRect, 264.0f, 58.0f,
                                                 ui::anchor::top_right, ui::anchor::top_right,
                                                 {-28.0f, 20.0f});
constexpr const char* kTitleIntroPath = "assets/audio/title_intro.mp3";
constexpr const char* kTitleLoopPath = "assets/audio/title_loop.mp3";
constexpr float kHomeButtonWidth = 232.0f;
constexpr float kHomeButtonHeight = 78.0f;
constexpr float kHomeButtonGap = 18.0f;
constexpr float kHomeAnimSpeed = 6.5f;
constexpr float kHomeButtonRowY = 376.0f;
constexpr float kHomeButtonIntroOffsetY = 24.0f;
constexpr float kAccountChipInteractiveThreshold = 0.2f;
constexpr ui::draw_layer kTitleModalLayer = ui::draw_layer::modal;

struct home_entry {
    const char* label;
    const char* detail;
    bool enabled;
    title_scene::transition_target target;
};

constexpr std::array<home_entry, 4> kHomeEntries = {{
    {"PLAY", "Solo song select.", true, title_scene::transition_target::song_select},
    {"MULTIPLAY", "Room battles soon.", false, title_scene::transition_target::song_select},
    {"ONLINE", "Browse and download.", false, title_scene::transition_target::song_select},
    {"CREATE", "Open creation tools.", true, title_scene::transition_target::song_create},
}};

float ease_out_cubic(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv * inv;
}

std::string make_avatar_label(const auth::session_summary& summary) {
    const std::string source = summary.logged_in
        ? (summary.display_name.empty() ? summary.email : summary.display_name)
        : "A";
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

std::string make_avatar_label(const song_select::auth_state& auth_state) {
    const auth::session_summary summary = {
        auth_state.logged_in,
        {},
        auth_state.email,
        auth_state.display_name,
        auth_state.email_verified,
    };
    return make_avatar_label(summary);
}

Vector2 lerp_vec2(Vector2 from, Vector2 to, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return {
        from.x + (to.x - from.x) * clamped,
        from.y + (to.y - from.y) * clamped,
    };
}

const char* account_name_for(const song_select::auth_state& auth_state) {
    if (!auth_state.logged_in) {
        return "ACCOUNT";
    }
    return auth_state.display_name.empty() ? auth_state.email.c_str() : auth_state.display_name.c_str();
}

const char* account_status_for(const song_select::auth_state& auth_state) {
    if (!auth_state.logged_in) {
        return "Manage account";
    }
    return auth_state.email_verified ? "Verified profile" : "Manage account";
}

Rectangle home_button_rect(int index, float anim_t) {
    const float eased = ease_out_cubic(anim_t);
    const float total_width =
        static_cast<float>(kHomeEntries.size()) * kHomeButtonWidth +
        static_cast<float>(kHomeEntries.size() - 1) * kHomeButtonGap;
    Rectangle rect = {
        (static_cast<float>(kScreenWidth) - total_width) * 0.5f +
            static_cast<float>(index) * (kHomeButtonWidth + kHomeButtonGap),
        kHomeButtonRowY + (1.0f - eased) * kHomeButtonIntroOffsetY,
        kHomeButtonWidth,
        kHomeButtonHeight
    };
    return rect;
}

}  // namespace

title_scene::title_scene(scene_manager& manager, bool start_with_home_open, bool play_intro_fade) :
    scene(manager),
    start_with_home_open_(start_with_home_open),
    play_intro_fade_(play_intro_fade) {
}

void title_scene::start_transition(transition_target target) {
    if (transitioning_to_song_select_) {
        return;
    }
    transition_target_ = target;
    transitioning_to_song_select_ = true;
    transition_fade_.restart(scene_fade::direction::out, 0.3f, 0.65f);
}

void title_scene::on_enter() {
    bgm_controller_.configure(kTitleIntroPath, kTitleLoopPath);
    spectrum_visualizer_.reset();
    bgm_controller_.on_enter();
    auth_overlay::refresh_auth_state(auth_state_);
    if (play_intro_fade_) {
        intro_fade_.restart(scene_fade::direction::in, 1.0f, 1.0f);
        intro_hold_t_ = 0.5f;
    } else {
        intro_fade_.restart(scene_fade::direction::in, 0.0f, 0.0f);
        intro_hold_t_ = 0.0f;
    }
    home_menu_open_ = start_with_home_open_;
    suppress_home_pointer_until_release_ = false;
    home_menu_anim_ = start_with_home_open_ ? 1.0f : 0.0f;
    home_menu_selected_index_ = 0;
    home_status_message_.clear();
    login_dialog_.open = false;
    if (auth_state_.logged_in) {
        auth_overlay::start_restore(auth_controller_, login_dialog_);
    }
}

void title_scene::on_exit() {
    bgm_controller_.on_exit();
    spectrum_visualizer_.reset();
    login_dialog_.open = false;
}

// Title 上で Home 展開、Play/Create への遷移、Account 導線を扱う。
void title_scene::update(float dt) {
    ui::begin_hit_regions();
    bgm_controller_.update();
    spectrum_visualizer_.update();
    auth_overlay::poll_restore(auth_controller_, auth_state_, login_dialog_);
    auth_overlay::poll_request(auth_controller_, auth_state_, login_dialog_);
    if (intro_hold_t_ > 0.0f) {
        intro_hold_t_ = std::max(0.0f, intro_hold_t_ - dt);
    } else {
        intro_fade_.update(dt);
    }
    if (login_dialog_.open) {
        login_dialog_.open_anim = std::min(1.0f, login_dialog_.open_anim + dt * 8.0f);
    } else {
        login_dialog_.open_anim = 0.0f;
    }
    const float target_anim = home_menu_open_ ? 1.0f : 0.0f;
    home_menu_anim_ = std::clamp(home_menu_anim_ + (target_anim - home_menu_anim_) * std::min(1.0f, dt * kHomeAnimSpeed),
                                 0.0f, 1.0f);
    if (std::fabs(home_menu_anim_ - target_anim) < 0.002f) {
        home_menu_anim_ = target_anim;
    }

    if (transitioning_to_song_select_) {
        transition_fade_.update(dt);
        if (transition_fade_.complete()) {
            switch (transition_target_) {
            case transition_target::song_select:
                manager_.change_scene(std::make_unique<song_select_scene>(manager_));
                break;
            case transition_target::song_create:
                manager_.change_scene(song_select::make_song_create_scene(manager_));
                break;
            }
        }
        return;
    }

    if (quitting_) {
        quit_fade_.update(dt);
        if (quit_fade_.complete()) {
            CloseWindow();
        }
        return;
    }

    if (home_menu_anim_ >= kAccountChipInteractiveThreshold && ui::is_clicked(kAccountChipRect)) {
        if (login_dialog_.open) {
            login_dialog_.open = false;
        } else {
            song_select::open_login_dialog(login_dialog_, auth::load_session_summary());
            auth_overlay::refresh_auth_state(auth_state_);
        }
        return;
    }

    if (login_dialog_.open) {
        if ((IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) &&
            !auth_controller_.request_active) {
            login_dialog_.open = false;
        }
        return;
    }

    if (suppress_home_pointer_until_release_ &&
        !IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
        !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        suppress_home_pointer_until_release_ = false;
    }

    const bool account_hovered =
        home_menu_anim_ >= kAccountChipInteractiveThreshold && ui::is_hovered(kAccountChipRect);
    const bool left_click_for_home =
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !account_hovered;
    const bool right_click_for_home = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    if (!home_menu_open_ &&
        (IsKeyPressed(KEY_ENTER) ||
         left_click_for_home ||
         right_click_for_home)) {
        home_menu_open_ = true;
        suppress_home_pointer_until_release_ = left_click_for_home || right_click_for_home;
        home_status_message_.clear();
        return;
    }

    if (home_menu_open_) {
        if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            home_menu_open_ = false;
            suppress_home_pointer_until_release_ = false;
            home_status_message_.clear();
            return;
        }

        if (!suppress_home_pointer_until_release_) {
            for (int index = 0; index < static_cast<int>(kHomeEntries.size()); ++index) {
                const Rectangle rect = home_button_rect(index, home_menu_anim_);
                if (ui::is_hovered(rect)) {
                    home_menu_selected_index_ = index;
                }
                if (ui::is_clicked(rect)) {
                    if (kHomeEntries[static_cast<size_t>(index)].enabled) {
                        start_transition(kHomeEntries[static_cast<size_t>(index)].target);
                    } else {
                        home_status_message_ = "This route is still warming up.";
                    }
                    return;
                }
            }
        }

        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            home_menu_selected_index_ = (home_menu_selected_index_ + 1) % static_cast<int>(kHomeEntries.size());
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            home_menu_selected_index_ = (home_menu_selected_index_ - 1 + static_cast<int>(kHomeEntries.size())) %
                                        static_cast<int>(kHomeEntries.size());
        }
        if (IsKeyPressed(KEY_ENTER)) {
            const home_entry& entry = kHomeEntries[static_cast<size_t>(home_menu_selected_index_)];
            if (entry.enabled) {
                start_transition(entry.target);
            } else {
                home_status_message_ = "This route is still warming up.";
            }
            return;
        }
    }

    if (!home_menu_open_ && IsKeyDown(KEY_ESCAPE)) {
        esc_hold_t_ += dt;
        if (esc_hold_t_ >= 0.3f) {
            esc_hold_t_ = 0.0f;
            quitting_ = true;
            quit_fade_.restart(scene_fade::direction::out, 1.5f, 1.0f);
        }
    } else {
        esc_hold_t_ = 0.0f;
    }
}

// タイトルと、そこから展開する Home 導線を描画する。
void title_scene::draw() {
    const auto& t = *g_theme;
    const float menu_t = ease_out_cubic(home_menu_anim_);
    const Vector2 closed_title_pos = ui::text_position("raythm", 124,
                                                       {kTitleClosedHeaderRect.x, kTitleClosedHeaderRect.y,
                                                        kTitleClosedHeaderRect.width, 124.0f},
                                                       ui::text_align::center);
    const Vector2 open_title_pos = ui::text_position("raythm", 124,
                                                     {kTitleOpenHeaderRect.x, kTitleOpenHeaderRect.y,
                                                      kTitleOpenHeaderRect.width, 124.0f},
                                                     ui::text_align::left);
    const Vector2 title_pos = lerp_vec2(closed_title_pos, open_title_pos, menu_t);
    const Vector2 closed_subtitle_pos = ui::text_position("trace the line before the beat disappears", 30,
                                                          {kTitleClosedHeaderRect.x + 10.0f, kTitleClosedHeaderRect.y + 128.0f,
                                                           kTitleClosedHeaderRect.width - 10.0f, 30.0f},
                                                          ui::text_align::center);
    const Vector2 open_subtitle_pos = ui::text_position("trace the line before the beat disappears", 30,
                                                        {kTitleOpenHeaderRect.x + 10.0f, kTitleOpenHeaderRect.y + 128.0f,
                                                         kTitleOpenHeaderRect.width - 10.0f, 30.0f},
                                                        ui::text_align::left);
    const Vector2 subtitle_pos = lerp_vec2(closed_subtitle_pos, open_subtitle_pos, menu_t);
    virtual_screen::begin();
    ClearBackground(t.bg);
    DrawRectangleGradientV(0, 0, kScreenWidth, kScreenHeight, t.bg, t.bg_alt);
    ui::begin_draw_queue();
    spectrum_visualizer_.draw(kSpectrumRect);
    ui::draw_text_f("raythm", title_pos.x, title_pos.y, 124, t.text);
    ui::draw_text_f("trace the line before the beat disappears", subtitle_pos.x, subtitle_pos.y, 30, t.text_dim);

    if (menu_t > 0.01f) {
        const unsigned char account_alpha = static_cast<unsigned char>(255.0f * menu_t);
        DrawRectangleRec(kAccountChipRect, with_alpha(t.panel, account_alpha));
        DrawRectangleLinesEx(kAccountChipRect, 2.0f, with_alpha(t.border, account_alpha));
        const Rectangle avatar_rect = {kAccountChipRect.x + 12.0f, kAccountChipRect.y + 9.0f, 40.0f, 40.0f};
        const Vector2 avatar_center = {avatar_rect.x + avatar_rect.width * 0.5f, avatar_rect.y + avatar_rect.height * 0.5f};
        DrawCircleV(avatar_center, 20.0f, with_alpha(auth_state_.logged_in ? t.accent : t.row_selected, account_alpha));
        const std::string avatar_label = make_avatar_label(auth_state_);
        ui::draw_text_in_rect(avatar_label.c_str(), 18, avatar_rect,
                              with_alpha(auth_state_.logged_in ? t.panel : t.text, account_alpha), ui::text_align::center);
        const Rectangle account_name_rect = {
            kAccountChipRect.x + 64.0f, kAccountChipRect.y + 8.0f, kAccountChipRect.width - 88.0f, 22.0f
        };
        draw_marquee_text(account_name_for(auth_state_), account_name_rect, 18, with_alpha(t.text, account_alpha), GetTime());
        ui::draw_text_in_rect(account_status_for(auth_state_),
                              13,
                              {kAccountChipRect.x + 64.0f, kAccountChipRect.y + 30.0f, kAccountChipRect.width - 88.0f, 16.0f},
                              with_alpha(auth_state_.logged_in && !auth_state_.email_verified ? t.error : t.text_muted, account_alpha),
                              ui::text_align::left);
        ui::draw_text_in_rect(">", 18,
                              {kAccountChipRect.x + kAccountChipRect.width - 24.0f, kAccountChipRect.y + 12.0f, 12.0f, 24.0f},
                              with_alpha(t.text_muted, account_alpha), ui::text_align::center);
    }

    if (home_menu_anim_ > 0.01f) {
        for (int index = 0; index < static_cast<int>(kHomeEntries.size()); ++index) {
            const home_entry& entry = kHomeEntries[static_cast<size_t>(index)];
            const Rectangle button_rect = home_button_rect(index, home_menu_anim_);
            const bool selected = index == home_menu_selected_index_;
            const Color bg = !entry.enabled
                ? with_alpha(t.row, static_cast<unsigned char>(146.0f * menu_t))
                : (selected ? with_alpha(t.row_selected, static_cast<unsigned char>(236.0f * menu_t))
                            : with_alpha(t.row, static_cast<unsigned char>(220.0f * menu_t)));
            const Color border = !entry.enabled
                ? with_alpha(t.border_light, static_cast<unsigned char>(180.0f * menu_t))
                : (selected ? with_alpha(t.border_active, static_cast<unsigned char>(255.0f * menu_t))
                            : with_alpha(t.border, static_cast<unsigned char>(230.0f * menu_t)));
            DrawRectangleRec(button_rect, bg);
            DrawRectangleLinesEx(button_rect, 1.8f, border);
            ui::draw_text_in_rect(entry.label, 24,
                                  {button_rect.x + 14.0f, button_rect.y + 12.0f, button_rect.width - 28.0f, 24.0f},
                                  with_alpha(entry.enabled ? t.text : t.text_muted, static_cast<unsigned char>(255.0f * menu_t)),
                                  ui::text_align::center);
            ui::draw_text_in_rect(entry.detail, 13,
                                  {button_rect.x + 16.0f, button_rect.y + 42.0f, button_rect.width - 32.0f, 18.0f},
                                  with_alpha(entry.enabled ? t.text_muted : t.text_hint, static_cast<unsigned char>(220.0f * menu_t)),
                                  ui::text_align::center);
        }

        if (!home_status_message_.empty()) {
            ui::draw_text_in_rect(home_status_message_.c_str(), 16,
                                  {0.0f, kHomeButtonRowY + kHomeButtonHeight + 22.0f,
                                   static_cast<float>(kScreenWidth), 18.0f},
                                  with_alpha(t.text_muted, static_cast<unsigned char>(230.0f * menu_t)),
                                  ui::text_align::center);
        }
    }

    const Rectangle account_dialog_anchor = {
        kAccountChipRect.x,
        kAccountChipRect.y + 12.0f,
        kAccountChipRect.width,
        kAccountChipRect.height
    };
    const song_select::login_dialog_command login_command =
        song_select::draw_login_dialog(auth_state_, login_dialog_,
                                       account_dialog_anchor, kScreenRect,
                                       auth_controller_.request_active, kTitleModalLayer);
    if (login_command == song_select::login_dialog_command::close) {
        login_dialog_.open = false;
    } else if (login_command != song_select::login_dialog_command::none) {
        auth_overlay::start_request(auth_controller_, login_dialog_, login_command);
    }

    ui::flush_draw_queue();

    if (intro_hold_t_ > 0.0f) {
        ui::draw_fullscreen_overlay(BLACK);
    } else {
        intro_fade_.draw();
    }
    if (transitioning_to_song_select_) {
        transition_fade_.draw();
    }
    if (quitting_) {
        quit_fade_.draw();
    }
    virtual_screen::end();

    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
