#include "multiplayer_result_scene.h"

#include <algorithm>
#include <chrono>
#include <filesystem>

#include "app_paths.h"
#include "audio_manager.h"
#include "managed_content_storage.h"
#include "path_utils.h"
#include "raylib_file_io.h"
#include "scene_manager.h"
#include "multiplayer_result/multiplayer_result_detail_view.h"
#include "multiplayer_result/multiplayer_result_score_list_view.h"
#include "multiplayer_result/multiplayer_result_screen_view.h"
#include "multiplayer_result/multiplayer_result_summary_view.h"
#include "song_select/song_select_navigation.h"
#include "virtual_screen.h"

namespace {

namespace detail_view = multiplayer_result::detail_view;
namespace score_list_view = multiplayer_result::score_list;
namespace screen_view = multiplayer_result::screen_view;
namespace summary_view = multiplayer_result::summary_view;

constexpr float kRefreshSeconds = 1.5f;

std::string result_bgm_path(bool failed) {
    const std::filesystem::path path =
        app_paths::audio_root() / (failed ? "result_failed.mp3" : "result_success.mp3");
    return std::filesystem::exists(path) ? path_utils::to_utf8(path) : "";
}

void play_result_bgm(bool failed) {
    const std::string path = result_bgm_path(failed);
    if (!path.empty() && audio_manager::instance().load_bgm(path)) {
        audio_manager::instance().play_bgm(true);
    }
}

rank parse_rank_label(const std::string& value) {
    if (value == "ss") return rank::ss;
    if (value == "s") return rank::s;
    if (value == "aa") return rank::aa;
    if (value == "a") return rank::a;
    if (value == "b") return rank::b;
    if (value == "c") return rank::c;
    return rank::f;
}

int find_self_placement(const std::vector<play_multiplayer_score_row>& scores, const std::string& self_user_id) {
    for (int i = 0; i < static_cast<int>(scores.size()); ++i) {
        if (!self_user_id.empty() && scores[static_cast<size_t>(i)].user_id == self_user_id) {
            return i + 1;
        }
    }
    return 0;
}

const play_multiplayer_score_row* find_score(const std::vector<play_multiplayer_score_row>& scores,
                                             const std::string& key) {
    const auto it = std::find_if(scores.begin(), scores.end(), [&key](const play_multiplayer_score_row& score) {
        return score_list_view::score_key(score) == key;
    });
    return it == scores.end() ? nullptr : &*it;
}

}  // namespace

multiplayer_result_scene::multiplayer_result_scene(scene_manager& manager,
                                                   result_data result,
                                                   song_data song,
                                                   chart_meta chart,
                                                   int key_count,
                                                   std::string room_id,
                                                   std::string match_id,
                                                   std::string self_user_id,
                                                   std::vector<play_multiplayer_score_row> scores)
    : scene(manager),
      result_(result),
      song_(std::move(song)),
      chart_(std::move(chart)),
      key_count_(key_count),
      room_id_(std::move(room_id)),
      match_id_(std::move(match_id)),
      self_user_id_(std::move(self_user_id)),
      scores_(std::move(scores)) {
    upsert_self_score();
    sort_scores();
    if (!self_user_id_.empty()) {
        selected_score_key_ = self_user_id_;
    }
}

multiplayer_result_scene::~multiplayer_result_scene() {
    unload_jacket_texture();
}

void multiplayer_result_scene::on_enter() {
    reveal_t_ = 0.0f;
    refresh_t_ = kRefreshSeconds;
    status_message_ = "Match result.";
    load_jacket_texture();
    play_result_bgm(result_.failed);
}

void multiplayer_result_scene::on_exit() {
    unload_jacket_texture();
}

void multiplayer_result_scene::on_app_exit() {
    if (room_id_.empty()) {
        return;
    }
    const auth::session_summary session = auth::load_session_summary();
    if (!match_id_.empty()) {
        (void)multiplayer::client::complete_match(session, match_id_);
    }
    (void)multiplayer::client::leave_room(session, room_id_);
}

void multiplayer_result_scene::update(float dt) {
    reveal_t_ = std::min(1.0f, reveal_t_ + dt * 1.8f);

    const screen_view::action_result screen_interaction = screen_view::handle_input(returning_);
    if (screen_interaction.back_requested) {
        request_return_to_room();
    }

    const score_list_view::interaction_result list_interaction =
        score_list_view::handle_input(scores_, scroll_y_, scrollbar_dragging_, scrollbar_drag_offset_);
    if (list_interaction.scroll_changed) {
        scroll_y_ = list_interaction.scroll_y;
    }
    if (list_interaction.scrollbar_drag_state_changed) {
        scrollbar_dragging_ = list_interaction.scrollbar_dragging;
        scrollbar_drag_offset_ = list_interaction.scrollbar_drag_offset;
    }
    if (list_interaction.selected_score_key.has_value()) {
        selected_score_key_ = *list_interaction.selected_score_key;
    }

    poll_room_refresh(dt);

    if (complete_future_.has_value() &&
        complete_future_->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const multiplayer::room_operation_result result = complete_future_->get();
        complete_future_.reset();
        status_message_ = result.success ? "Returning to room..." : result.message;
        manager_.change_scene(song_select::make_multiplayer_title_scene(
            manager_, room_id_, song_.meta.song_id, chart_.chart_id));
    }
}

void multiplayer_result_scene::request_return_to_room() {
    returning_ = true;
    status_message_ = "Returning to room...";
    if (match_id_.empty()) {
        manager_.change_scene(song_select::make_multiplayer_title_scene(
            manager_, room_id_, song_.meta.song_id, chart_.chart_id));
        return;
    }
    if (!complete_future_.has_value()) {
        const auth::session_summary session = auth::load_session_summary();
        const std::string match_id = match_id_;
        complete_future_ = std::async(std::launch::async, [session, match_id]() {
            return multiplayer::client::complete_match(session, match_id);
        });
    }
}

void multiplayer_result_scene::poll_room_refresh(float dt) {
    if (returning_) {
        return;
    }

    if (room_future_.has_value() &&
        room_future_->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const multiplayer::room_operation_result result = room_future_->get();
        room_future_.reset();
        if (result.room.has_value()) {
            apply_live_scores(result.room->live_scores);
        } else if (!result.live_scores.empty()) {
            apply_live_scores(result.live_scores);
        }
    }

    refresh_t_ += dt;
    if (refresh_t_ >= kRefreshSeconds && !room_future_.has_value()) {
        refresh_t_ = 0.0f;
        const auth::session_summary session = auth::load_session_summary();
        const std::string room_id = room_id_;
        room_future_ = std::async(std::launch::async, [session, room_id]() {
            return multiplayer::client::fetch_room(session, room_id);
        });
    }
}

void multiplayer_result_scene::apply_live_scores(const std::vector<multiplayer::live_score>& scores) {
    scores_.clear();
    scores_.reserve(scores.size() + 1);
    for (const multiplayer::live_score& score : scores) {
        scores_.push_back({
            .user_id = score.user_id,
            .display_name = score.display_name,
            .avatar_url = score.avatar_url,
            .score = score.score,
            .combo = score.combo,
            .accuracy = score.accuracy,
            .failed = score.failed,
            .has_result_details = score.has_result_details,
            .judge_counts = score.judge_counts,
            .rc_value = score.rc_value,
            .avg_offset = score.avg_offset,
            .fast_count = score.fast_count,
            .slow_count = score.slow_count,
            .clear_rank = score.clear_rank.empty() ? rank::f : parse_rank_label(score.clear_rank),
            .is_full_combo = score.is_full_combo,
            .is_all_perfect = score.is_all_perfect,
        });
    }
    upsert_self_score();
    sort_scores();
    if (find_score(scores_, selected_score_key_) == nullptr) {
        selected_score_key_ = !self_user_id_.empty() ? self_user_id_ :
            (scores_.empty() ? std::string{} : score_list_view::score_key(scores_.front()));
    }
}

void multiplayer_result_scene::upsert_self_score() {
    auto self = std::find_if(scores_.begin(), scores_.end(), [this](const play_multiplayer_score_row& score) {
        return !self_user_id_.empty() && score.user_id == self_user_id_;
    });

    const auth::session_summary session = auth::load_session_summary();
    const play_multiplayer_score_row row{
        .user_id = self_user_id_,
        .display_name = session.display_name.empty() ? "You" : session.display_name,
        .avatar_url = session.avatar_url,
        .score = result_.score,
        .combo = result_.max_combo,
        .accuracy = result_.accuracy,
        .failed = result_.failed,
        .has_result_details = true,
        .judge_counts = result_.judge_counts,
        .rc_value = result_.rc_value,
        .avg_offset = result_.avg_offset,
        .fast_count = result_.fast_count,
        .slow_count = result_.slow_count,
        .clear_rank = result_.clear_rank,
        .is_full_combo = result_.is_full_combo,
        .is_all_perfect = result_.is_all_perfect,
    };
    if (self == scores_.end()) {
        scores_.push_back(row);
    } else {
        *self = row;
    }
    if (selected_score_key_.empty()) {
        selected_score_key_ = score_list_view::score_key(row);
    }
}

void multiplayer_result_scene::sort_scores() {
    std::stable_sort(scores_.begin(), scores_.end(), [](const play_multiplayer_score_row& left,
                                                        const play_multiplayer_score_row& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        if (left.accuracy != right.accuracy) {
            return left.accuracy > right.accuracy;
        }
        return left.combo > right.combo;
    });
}

void multiplayer_result_scene::load_jacket_texture() {
    unload_jacket_texture();
    if (song_.directory.empty() || song_.meta.jacket_file.empty()) {
        return;
    }
    const std::filesystem::path jacket_path = path_utils::join_utf8(song_.directory, song_.meta.jacket_file);
    const managed_content_storage::managed_file_read_result managed =
        managed_content_storage::read_managed_file(jacket_path);
    if (managed.managed) {
        if (!managed.success || managed.bytes.empty()) {
            return;
        }
        Image image = LoadImageFromMemory(jacket_path.extension().string().c_str(),
                                          managed.bytes.data(),
                                          static_cast<int>(managed.bytes.size()));
        if (image.data == nullptr) {
            return;
        }
        jacket_texture_ = LoadTextureFromImage(image);
        UnloadImage(image);
    } else {
        if (!std::filesystem::exists(jacket_path) || !std::filesystem::is_regular_file(jacket_path)) {
            return;
        }
        jacket_texture_ = raylib_file_io::load_texture(jacket_path);
    }
    jacket_texture_loaded_ = jacket_texture_.id != 0;
}

void multiplayer_result_scene::unload_jacket_texture() {
    if (jacket_texture_loaded_) {
        UnloadTexture(jacket_texture_);
        jacket_texture_ = {};
        jacket_texture_loaded_ = false;
    }
}

void multiplayer_result_scene::draw() {
    virtual_screen::begin_ui();
    bool return_to_room_requested = false;

    screen_view::draw_frame();

    const play_multiplayer_score_row fallback_score{
        .user_id = self_user_id_,
        .display_name = "You",
        .avatar_url = auth::load_session_summary().avatar_url,
        .score = result_.score,
        .combo = result_.max_combo,
        .accuracy = result_.accuracy,
        .failed = result_.failed,
        .has_result_details = true,
        .judge_counts = result_.judge_counts,
        .rc_value = result_.rc_value,
        .avg_offset = result_.avg_offset,
        .fast_count = result_.fast_count,
        .slow_count = result_.slow_count,
        .clear_rank = result_.clear_rank,
        .is_full_combo = result_.is_full_combo,
        .is_all_perfect = result_.is_all_perfect,
    };
    const play_multiplayer_score_row* selected = find_score(scores_, selected_score_key_);
    if (selected == nullptr) {
        selected = &fallback_score;
    }
    const std::string self_key = !self_user_id_.empty() ? self_user_id_ : score_list_view::score_key(fallback_score);
    const bool selected_self = score_list_view::score_key(*selected) == self_key;
    const bool selected_has_details = selected_self || selected->has_result_details;
    const std::string avatar_base_url = auth::load_session_summary().server_url;

    const int self_place = find_self_placement(scores_, self_user_id_);
    summary_view::draw(song_, chart_, key_count_, fallback_score, self_place,
                       jacket_texture_, jacket_texture_loaded_, avatar_base_url);

    detail_view::draw(*selected, selected_has_details);

    screen_view::draw_ranking_header(static_cast<int>(scores_.size()));
    score_list_view::draw(scores_, self_user_id_, selected_score_key_, avatar_base_url, scroll_y_);

    const screen_view::action_result back_button = screen_view::draw_back_button(returning_);
    if (back_button.back_requested) {
        return_to_room_requested = true;
    }

    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
    if (return_to_room_requested) {
        request_return_to_room();
    }
}


