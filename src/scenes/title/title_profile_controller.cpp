#include "title/title_profile_controller.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <thread>
#include <utility>

#include "app_paths.h"
#include "file_dialog.h"
#include "path_utils.h"
#include "shared/avatar_texture_cache.h"
#include "tween.h"
#include "ui_notice.h"

namespace {

title_profile_view::activity_item to_activity_item(const auth::profile_ranking_record& record) {
    return {
        .song_title = record.song_title,
        .artist = record.artist,
        .genre = record.genre,
        .difficulty_name = record.difficulty_name,
        .local_summary = "Score " + std::to_string(record.score),
        .online_summary = "Online #" + std::to_string(record.placement) + " / " + std::to_string(record.score),
    };
}

}  // namespace

void title_profile_controller::reset() {
    state_ = {};
    avatar_picker_.close();
}

void title_profile_controller::open() {
    title_profile_view::open(state_);
    request_reload();
}

void title_profile_controller::close() {
    title_profile_view::close(state_);
}

void title_profile_controller::close_if_logged_out(bool logged_in) {
    if (state_.open && !logged_in) {
        close();
    }
}

void title_profile_controller::tick(float dt) {
    if (state_.open && state_.closing) {
        state_.open_anim = tween::retreat(state_.open_anim, dt, 8.0f);
        if (state_.open_anim <= 0.0f) {
            state_.open = false;
            state_.closing = false;
        }
    } else if (state_.open) {
        state_.open_anim = tween::advance(state_.open_anim, dt, 8.0f);
    } else {
        state_.open_anim = 0.0f;
    }
}

title_profile_controller::poll_result title_profile_controller::poll() {
    poll_result result;

    if (state_.loading && load_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        title_profile_view::load_result loaded;
        try {
            loaded = load_future_.get();
        } catch (const std::exception& ex) {
            loaded.uploads = {
                .success = false,
                .message = ex.what(),
            };
        } catch (...) {
            loaded.uploads = {
                .success = false,
                .message = "Failed to load profile.",
            };
        }

        state_.uploads = std::move(loaded.uploads);
        state_.rankings = std::move(loaded.rankings);
        state_.activity = std::move(loaded.activity);
        state_.first_place_records = std::move(loaded.first_place_records);
        state_.loading = false;
        state_.loaded_once = true;
        title_profile_view::clamp_scroll(state_);
        if (!state_.uploads.success) {
            ui::notify(state_.uploads.message, ui::notice_tone::error, 3.0f);
        } else if (!state_.rankings.success) {
            ui::notify(state_.rankings.message, ui::notice_tone::error, 3.0f);
        }
    }

    if (!state_.deleting || delete_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        if (!state_.saving_avatar ||
            save_avatar_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            if (!state_.saving_links ||
                save_links_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
                return result;
            }

            auth::operation_result save_result;
            try {
                save_result = save_links_future_.get();
            } catch (const std::exception& ex) {
                save_result.success = false;
                save_result.message = ex.what();
            } catch (...) {
                save_result.success = false;
                save_result.message = "Saving profile links failed.";
            }

            state_.saving_links = false;
            ui::notify(save_result.message,
                       save_result.success ? ui::notice_tone::success : ui::notice_tone::error,
                       3.0f);
            if (save_result.success) {
                state_.settings_links_initialized = false;
                result.content_changed = true;
            }

            return result;
        }

        auth::operation_result save_result;
        try {
            save_result = save_avatar_future_.get();
        } catch (const std::exception& ex) {
            save_result.success = false;
            save_result.message = ex.what();
        } catch (...) {
            save_result.success = false;
            save_result.message = "Saving profile image failed.";
        }

        state_.saving_avatar = false;
        ui::notify(save_result.message,
                   save_result.success ? ui::notice_tone::success : ui::notice_tone::error,
                   3.0f);
        if (save_result.success) {
            avatar_texture_cache::shared().clear();
            result.content_changed = true;
        }

        return result;
    }

    auth::operation_result delete_result;
    try {
        delete_result = delete_future_.get();
    } catch (const std::exception& ex) {
        delete_result.success = false;
        delete_result.message = ex.what();
    } catch (...) {
        delete_result.success = false;
        delete_result.message = "Delete failed.";
    }

    state_.deleting = false;
    state_.pending_delete = title_profile_view::delete_target::none;
    state_.pending_id.clear();
    state_.pending_label.clear();
    ui::notify(delete_result.message,
               delete_result.success ? ui::notice_tone::success : ui::notice_tone::error,
               3.0f);
    if (delete_result.success) {
        result.content_changed = true;
        request_reload();
    }

    return result;
}

title_profile_controller::input_result title_profile_controller::handle_input(bool auth_request_active) {
    if (avatar_picker_.is_open()) {
        avatar_picker_.update();
        if (avatar_picker_.consume_accept()) {
            const std::filesystem::path avatar_path = app_paths::app_data_root() / "profile_avatar_upload.png";
            const square_image_picker::export_result exported =
                avatar_picker_.export_png(path_utils::to_utf8(avatar_path), {.output_size = 128});
            if (!exported.success) {
                ui::notify(exported.message.empty() ? "Failed to crop profile image." : exported.message,
                           ui::notice_tone::error, 3.0f);
            } else {
                start_save_avatar(path_utils::to_utf8(avatar_path));
            }
        } else if (avatar_picker_.consume_cancel()) {
            return {.consumed = true};
        }
        return {.consumed = true};
    }

    const title_profile_view::command command = title_profile_view::update(state_, auth_request_active);
    switch (command.type) {
    case title_profile_view::command_type::delete_account:
        if (command.password.empty()) {
            ui::notify("Password is required to delete the account.", ui::notice_tone::error, 2.8f);
            return {.consumed = true};
        }
        title_profile_view::close(state_);
        return {
            .consumed = true,
            .delete_account_password = command.password,
        };
    case title_profile_view::command_type::delete_song:
        start_delete_song(command.id);
        return {.consumed = true};
    case title_profile_view::command_type::delete_chart:
        start_delete_chart(command.id);
        return {.consumed = true};
    case title_profile_view::command_type::save_external_links:
        start_save_external_links(command.external_links);
        return {.consumed = true};
    case title_profile_view::command_type::change_avatar: {
        const std::string path = file_dialog::open_image_file();
        if (!path.empty()) {
            std::string error;
            if (!avatar_picker_.open(path, error)) {
                ui::notify(error.empty() ? "Failed to open image." : error, ui::notice_tone::error, 3.0f);
            }
        }
        return {.consumed = true};
    }
    case title_profile_view::command_type::remove_avatar:
        start_delete_avatar();
        return {.consumed = true};
    case title_profile_view::command_type::close:
        return {.consumed = true};
    case title_profile_view::command_type::none:
        return {.consumed = state_.open};
    }

    return {.consumed = state_.open};
}

void title_profile_controller::draw(const song_select::auth_state& auth_state,
                                    bool auth_request_active,
                                    ui::draw_layer layer) {
    title_profile_view::draw(state_, auth_state, avatar_picker_, auth_request_active, layer);
}

bool title_profile_controller::is_open() const {
    return state_.open;
}

Rectangle title_profile_controller::bounds() const {
    return title_profile_view::bounds();
}

void title_profile_controller::request_reload() {
    if (state_.loading) {
        return;
    }

    state_.loading = true;
    std::promise<title_profile_view::load_result> promise;
    load_future_ = promise.get_future();
    std::thread([promise = std::move(promise)]() mutable {
        try {
            title_profile_view::load_result loaded;
            loaded.uploads = auth::fetch_my_community_uploads();
            loaded.rankings = auth::fetch_my_profile_rankings();

            if (loaded.rankings.success) {
                for (const auth::profile_ranking_record& record : loaded.rankings.recent_records) {
                    loaded.activity.push_back(to_activity_item(record));
                }
                for (const auth::profile_ranking_record& record : loaded.rankings.first_place_records) {
                    loaded.first_place_records.push_back(to_activity_item(record));
                }
            }

            promise.set_value(std::move(loaded));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_profile_controller::start_save_external_links(std::vector<auth::external_link> links) {
    if (state_.saving_links) {
        return;
    }

    state_.saving_links = true;
    std::promise<auth::operation_result> promise;
    save_links_future_ = promise.get_future();
    std::thread([promise = std::move(promise), links = std::move(links)]() mutable {
        try {
            promise.set_value(auth::update_profile_external_links(links));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_profile_controller::start_save_avatar(std::string image_path) {
    if (state_.saving_avatar) {
        return;
    }

    state_.saving_avatar = true;
    std::promise<auth::operation_result> promise;
    save_avatar_future_ = promise.get_future();
    std::thread([promise = std::move(promise), image_path = std::move(image_path)]() mutable {
        try {
            promise.set_value(auth::update_profile_avatar(image_path));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_profile_controller::start_delete_avatar() {
    if (state_.saving_avatar) {
        return;
    }

    state_.saving_avatar = true;
    std::promise<auth::operation_result> promise;
    save_avatar_future_ = promise.get_future();
    std::thread([promise = std::move(promise)]() mutable {
        try {
            promise.set_value(auth::delete_profile_avatar());
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_profile_controller::start_delete_song(std::string song_id) {
    if (state_.deleting) {
        return;
    }

    state_.deleting = true;
    std::promise<auth::operation_result> promise;
    delete_future_ = promise.get_future();
    std::thread([promise = std::move(promise), song_id = std::move(song_id)]() mutable {
        try {
            promise.set_value(auth::delete_community_song_upload(song_id));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}

void title_profile_controller::start_delete_chart(std::string chart_id) {
    if (state_.deleting) {
        return;
    }

    state_.deleting = true;
    std::promise<auth::operation_result> promise;
    delete_future_ = promise.get_future();
    std::thread([promise = std::move(promise), chart_id = std::move(chart_id)]() mutable {
        try {
            promise.set_value(auth::delete_community_chart_upload(chart_id));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
}
