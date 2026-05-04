#include "shared/auth_overlay_controller.h"

#include <chrono>
#include <exception>
#include <future>
#include <thread>
#include <utility>

#include "game_settings.h"
#include "network/server_environment.h"
#include "ui_notice.h"

namespace {

template <typename Fn>
std::future<auth::operation_result> start_auth_task(Fn task) {
    std::promise<auth::operation_result> promise;
    std::future<auth::operation_result> future = promise.get_future();
    std::thread([promise = std::move(promise), task = std::move(task)]() mutable {
        auth::operation_result result;
        try {
            result = task();
        } catch (const std::exception& ex) {
            result.success = false;
            result.message = ex.what();
        } catch (...) {
            result.success = false;
            result.message = "Authentication request failed.";
        }
        promise.set_value(std::move(result));
    }).detach();
    return future;
}

void notify_auth(std::string message, ui::notice_tone tone) {
    if (message.empty()) {
        return;
    }
    ui::notify(std::move(message), tone, tone == ui::notice_tone::error ? 3.2f : 2.2f);
}

}  // namespace

namespace auth_overlay {

void refresh_auth_state(song_select::auth_state& auth_state) {
    const auth::session_summary summary = auth::load_session_summary();
    auth_state.logged_in = summary.logged_in;
    auth_state.email = summary.email;
    auth_state.display_name = summary.display_name;
    auth_state.email_verified = summary.email_verified;
}

void start_restore(controller& controller_state, song_select::login_dialog_state&) {
    controller_state.restore_active = true;
    controller_state.restore_future = start_auth_task([]() {
        return auth::restore_saved_session();
    });
}

void start_request(controller& controller_state,
                   song_select::login_dialog_state& dialog_state,
                   song_select::login_dialog_command command) {
    if (controller_state.request_active) {
        return;
    }

    controller_state.request_active = true;
    const std::string server_url = auth::normalize_server_url(
        server_environment::configured_url(g_settings.server_env, g_settings.custom_server_url));
    const std::string display_name = dialog_state.display_name_input.value;
    const std::string email = dialog_state.email_input.value;
    const std::string password = dialog_state.password_input.value;
    const std::string password_confirmation = dialog_state.password_confirmation_input.value;
    const std::string verification_email = dialog_state.verification_email;
    const std::string verification_code = dialog_state.verification_code_input.value;
    const auth::verification_purpose verification = dialog_state.verification;

    switch (command) {
    case song_select::login_dialog_command::request_restore:
        notify_auth("Restoring session...", ui::notice_tone::info);
        controller_state.request_future = start_auth_task([]() {
            return auth::restore_saved_session();
        });
        break;
    case song_select::login_dialog_command::request_login:
        notify_auth("Connecting to raythm-Server...", ui::notice_tone::info);
        controller_state.request_future = start_auth_task([server_url, email, password]() {
            return auth::login_user(server_url, email, password);
        });
        break;
    case song_select::login_dialog_command::request_register:
        if (display_name.empty() || email.empty() || password.empty()) {
            controller_state.request_active = false;
            notify_auth("Name, email, and password are required.", ui::notice_tone::error);
            break;
        }
        if (password != password_confirmation) {
            controller_state.request_active = false;
            notify_auth("Passwords do not match.", ui::notice_tone::error);
            break;
        }
        notify_auth("Creating account...", ui::notice_tone::info);
        controller_state.request_future = start_auth_task([server_url, email, display_name, password]() {
            return auth::register_user(server_url, email, display_name, password);
        });
        break;
    case song_select::login_dialog_command::request_verify:
        if (verification_email.empty() || verification_code.empty()) {
            controller_state.request_active = false;
            notify_auth("Verification code is required.", ui::notice_tone::error);
            break;
        }
        notify_auth("Verifying code...", ui::notice_tone::info);
        controller_state.request_future = start_auth_task([server_url, verification_email, verification_code, verification]() {
            if (verification == auth::verification_purpose::login_verification) {
                return auth::verify_login_code(server_url, verification_email, verification_code);
            }
            return auth::verify_email_code(server_url, verification_email, verification_code);
        });
        break;
    case song_select::login_dialog_command::request_resend_code:
        if (verification_email.empty() || verification == auth::verification_purpose::none) {
            controller_state.request_active = false;
            notify_auth("No verification request is active.", ui::notice_tone::error);
            break;
        }
        notify_auth("Resending code...", ui::notice_tone::info);
        controller_state.request_future = start_auth_task([server_url, verification_email, verification]() {
            return auth::resend_verification_code(server_url, verification_email, verification);
        });
        break;
    case song_select::login_dialog_command::request_logout:
        notify_auth("Logging out...", ui::notice_tone::info);
        controller_state.request_future = start_auth_task([]() {
            return auth::logout_saved_session();
        });
        break;
    case song_select::login_dialog_command::request_delete_account:
        if (password.empty()) {
            controller_state.request_active = false;
            notify_auth("Password is required to delete the account.", ui::notice_tone::error);
            break;
        }
        notify_auth("Deleting account...", ui::notice_tone::info);
        controller_state.request_future = start_auth_task([password]() {
            return auth::delete_saved_account(password);
        });
        break;
    case song_select::login_dialog_command::none:
    case song_select::login_dialog_command::close:
        controller_state.request_active = false;
        break;
    }
}

void poll_restore(controller& controller_state,
                  song_select::auth_state& auth_state,
                  song_select::login_dialog_state&) {
    if (!controller_state.restore_active) {
        return;
    }

    if (controller_state.restore_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    controller_state.restore_active = false;
    const auth::operation_result result = controller_state.restore_future.get();
    refresh_auth_state(auth_state);

    if (!result.success) {
        notify_auth(result.message, ui::notice_tone::error);
    }
}

void poll_request(controller& controller_state,
                  song_select::auth_state& auth_state,
                  song_select::login_dialog_state& dialog_state) {
    if (!controller_state.request_active) {
        return;
    }

    if (controller_state.request_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    controller_state.request_active = false;
    const auth::operation_result result = controller_state.request_future.get();
    refresh_auth_state(auth_state);
    dialog_state.password_input.value.clear();
    dialog_state.password_confirmation_input.value.clear();
    if (result.verification_required) {
        dialog_state.mode = song_select::login_dialog_mode::verify;
        dialog_state.verification = result.verification;
        dialog_state.verification_email = result.verification_email.empty()
            ? dialog_state.email_input.value
            : result.verification_email;
        dialog_state.verification_code_input.value.clear();
        notify_auth(result.message, ui::notice_tone::info);
        return;
    }
    if (result.success) {
        dialog_state.mode = song_select::login_dialog_mode::login;
        dialog_state.verification = auth::verification_purpose::none;
        dialog_state.verification_email.clear();
        dialog_state.verification_code_input.value.clear();
    }
    notify_auth(result.message, result.success ? ui::notice_tone::success : ui::notice_tone::error);

    const auth::session_summary summary = auth::load_session_summary();
    dialog_state.email_input.value = summary.email;
}

}  // namespace auth_overlay
