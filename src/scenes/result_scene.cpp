#include "result_scene.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <thread>

#include "app_paths.h"
#include "audio_manager.h"
#include "path_utils.h"
#include "play_scene.h"
#include "raylib.h"
#include "result/result_scene_view.h"
#include "scene_common.h"
#include "scene_manager.h"
#include "song_select/song_select_navigation.h"
#include "ui_notice.h"
#include "virtual_screen.h"

namespace {

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

}  // namespace

result_scene::result_scene(scene_manager& manager, result_data result, bool ranking_enabled,
                           song_data song, std::string chart_path, chart_meta chart, int key_count)
    : scene(manager), result_(result), ranking_enabled_(ranking_enabled),
      song_(std::move(song)), chart_path_(std::move(chart_path)), chart_(std::move(chart)), key_count_(key_count) {
}

result_scene::~result_scene() {
    unload_jacket_texture();
}

void result_scene::on_enter() {
    reveal_t_ = 0.0f;
    load_jacket_texture();
    play_result_bgm(result_.failed);

    local_submit_result_ = ranking_service::submit_local_result_detailed(chart_, result_);

    if (local_submit_result_.success && local_submit_result_.best_updated) {
        ui::notify("Local best updated.", ui::notice_tone::success, 2.0f);
    }

    if (ranking_enabled_ && ranking_service::should_attempt_online_submit(local_submit_result_)) {
        online_submit_status_message_ = "Submitting online ranking...";
        online_submit_status_is_error_ = false;

        online_submit_task_ = std::make_shared<online_submit_task_state>();
        std::shared_ptr<online_submit_task_state> task = online_submit_task_;
        const song_data song = song_;
        const std::string chart_path = chart_path_;
        const chart_meta chart = chart_;
        const std::string recorded_at = local_submit_result_.submitted_entry->recorded_at;
        const result_data result_payload = result_;
        std::thread([task, song, chart_path, chart, result_payload, recorded_at]() {
            ranking_service::online_submit_result submit_result =
                ranking_service::submit_online_result(song, chart_path, chart, result_payload, recorded_at);
            {
                std::scoped_lock lock(task->mutex);
                task->result = std::move(submit_result);
            }
            task->done.store(true);
        }).detach();
    } else if (!ranking_enabled_) {
        online_submit_status_message_ = "Online ranking disabled for this play.";
        online_submit_status_is_error_ = false;
    } else if (result_.failed) {
        online_submit_status_message_ = "Failed play is not ranked.";
        online_submit_status_is_error_ = false;
    } else if (!local_submit_result_.success) {
        online_submit_status_message_ = "This result is not ranking eligible.";
        online_submit_status_is_error_ = false;
    }
}

void result_scene::on_exit() {
    unload_jacket_texture();
}

void result_scene::return_to_song_select() const {
    song_select::recent_result_offset recent_result;
    recent_result.song_id = song_.meta.song_id;
    recent_result.chart_id = chart_.chart_id;
    recent_result.avg_offset_ms = result_.avg_offset;
    manager_.change_scene(song_select::make_seamless_song_select_scene(
        manager_, song_.meta.song_id, chart_.chart_id, recent_result));
}

void result_scene::retry_chart() {
    manager_.change_scene(std::make_unique<play_scene>(manager_, song_, chart_path_, key_count_, chart_.level));
}

void result_scene::update(float dt) {
    reveal_t_ = std::min(1.0f, reveal_t_ + dt * 1.7f);
    fade_in_.update(dt);
    poll_online_submit();

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
        return_to_song_select();
        return;
    }
    if (IsKeyPressed(KEY_R)) {
        retry_chart();
        return;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        switch (result_scene_view::hit_test_action(virtual_screen::get_virtual_mouse())) {
            case result_scene_view::action::retry:
            case result_scene_view::action::replay:
                retry_chart();
                return;
            case result_scene_view::action::song_select:
                return_to_song_select();
                return;
            case result_scene_view::action::none:
                break;
        }
    }
}

void result_scene::poll_online_submit() {
    if (!online_submit_task_ || online_submit_consumed_ || !online_submit_task_->done.load()) {
        return;
    }

    online_submit_consumed_ = true;
    {
        std::scoped_lock lock(online_submit_task_->mutex);
        online_submit_result_ = online_submit_task_->result;
    }

    if (!online_submit_result_.message.empty()) {
        online_submit_status_message_ = online_submit_result_.message;
        online_submit_status_is_error_ = !online_submit_result_.success;
    } else if (online_submit_result_.success && online_submit_result_.updated) {
        online_submit_status_message_ = "Online ranking updated.";
        online_submit_status_is_error_ = false;
    } else if (online_submit_result_.success) {
        online_submit_status_message_ = "Submitted score did not beat your online best.";
        online_submit_status_is_error_ = false;
    } else {
        online_submit_status_message_.clear();
        online_submit_status_is_error_ = false;
    }

    if (!online_submit_status_message_.empty()) {
        ui::notify(online_submit_status_message_,
                   online_submit_status_is_error_ ? ui::notice_tone::error : ui::notice_tone::success,
                   online_submit_status_is_error_ ? 3.0f : 2.0f);
    }
}

void result_scene::load_jacket_texture() {
    unload_jacket_texture();
    if (song_.meta.jacket_file.empty()) {
        return;
    }

    const std::filesystem::path jacket_path =
        path_utils::join_utf8(song_.directory, song_.meta.jacket_file);
    if (!std::filesystem::exists(jacket_path) || !std::filesystem::is_regular_file(jacket_path)) {
        return;
    }

    const std::string jacket_path_utf8 = path_utils::to_utf8(jacket_path);
    jacket_texture_ = LoadTexture(jacket_path_utf8.c_str());
    jacket_texture_loaded_ = jacket_texture_.id != 0;
    if (jacket_texture_loaded_) {
        SetTextureFilter(jacket_texture_, TEXTURE_FILTER_BILINEAR);
    }
}

void result_scene::unload_jacket_texture() {
    if (!jacket_texture_loaded_) {
        return;
    }

    UnloadTexture(jacket_texture_);
    jacket_texture_ = {};
    jacket_texture_loaded_ = false;
}

void result_scene::draw() {
    virtual_screen::begin_ui();
    result_scene_view::draw({
        .result = result_,
        .song = song_,
        .chart = chart_,
        .key_count = key_count_,
        .jacket_texture = jacket_texture_loaded_ ? &jacket_texture_ : nullptr,
        .local_submit = local_submit_result_.success ? &local_submit_result_ : nullptr,
        .online_submit = online_submit_consumed_ ? &online_submit_result_ : nullptr,
        .online_status_message = online_submit_status_message_,
        .online_status_is_error = online_submit_status_is_error_,
        .reveal_t = reveal_t_,
        .now = GetTime(),
    });

    fade_in_.draw();

    virtual_screen::end();
    ClearBackground(BLACK);
    virtual_screen::draw_to_screen();
}
