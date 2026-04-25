#include "shared/auth_overlay_controller.h"

#include <chrono>
#include <exception>
#include <future>
#include <thread>
#include <utility>

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
    dialog_state.status_message_is_error = false;
    const std::string server_url = auth::kDefaultServerUrl;
    const std::string display_name = dialog_state.display_name_input.value;
    const std::string email = dialog_state.email_input.value;
    const std::string password = dialog_state.password_input.value;
    const std::string password_confirmation = dialog_state.password_confirmation_input.value;
    const std::string verification_email = dialog_state.verification_email;
    const std::string verification_code = dialog_state.verification_code_input.value;
    const auth::verification_purpose verification = dialog_state.verification;

    switch (command) {
    case song_select::login_dialog_command::request_restore:
        dialog_state.status_message = "Restoring session...";
        controller_state.request_future = start_auth_task([]() {
            return auth::restore_saved_session();
        });
        break;
    case song_select::login_dialog_command::request_login:
        dialog_state.status_message = "Connecting to raythm-Server...";
        controller_state.request_future = start_auth_task([server_url, email, password]() {
            return auth::login_user(server_url, email, password);
        });
        break;
    case song_select::login_dialog_command::request_register:
        if (display_name.empty() || email.empty() || password.empty()) {
            controller_state.request_active = false;
            dialog_state.status_message = "Name, email, and password are required.";
            dialog_state.status_message_is_error = true;
            break;
        }
        if (password != password_confirmation) {
            controller_state.request_active = false;
            dialog_state.status_message = "Passwords do not match.";
            dialog_state.status_message_is_error = true;
            break;
        }
        dialog_state.status_message = "Creating account...";
        controller_state.request_future = start_auth_task([server_url, email, display_name, password]() {
            return auth::register_user(server_url, email, display_name, password);
        });
        break;
    case song_select::login_dialog_command::request_verify:
        if (verification_email.empty() || verification_code.empty()) {
            controller_state.request_active = false;
            dialog_state.status_message = "Verification code is required.";
            dialog_state.status_message_is_error = true;
            break;
        }
        dialog_state.status_message = "Verifying code...";
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
            dialog_state.status_message = "No verification request is active.";
            dialog_state.status_message_is_error = true;
            break;
        }
        dialog_state.status_message = "Resending code...";
        controller_state.request_future = start_auth_task([server_url, verification_email, verification]() {
            return auth::resend_verification_code(server_url, verification_email, verification);
        });
        break;
    case song_select::login_dialog_command::request_logout:
        dialog_state.status_message = "Logging out...";
        controller_state.request_future = start_auth_task([]() {
            return auth::logout_saved_session();
        });
        break;
    case song_select::login_dialog_command::none:
    case song_select::login_dialog_command::close:
        controller_state.request_active = false;
        break;
    }
}

poll_result poll_restore(controller& controller_state,
                         song_select::auth_state& auth_state,
                         song_select::login_dialog_state& dialog_state) {
    if (!controller_state.restore_active) {
        return {};
    }

    if (controller_state.restore_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return {};
    }

    controller_state.restore_active = false;
    const auth::operation_result result = controller_state.restore_future.get();
    refresh_auth_state(auth_state);

    if (!result.success) {
        if (dialog_state.open) {
            dialog_state.status_message = result.message;
            dialog_state.status_message_is_error = true;
        } else {
            return {
                true,
                true,
                result.message,
            };
        }
    }

    return {};
}

poll_result poll_request(controller& controller_state,
                         song_select::auth_state& auth_state,
                         song_select::login_dialog_state& dialog_state) {
    if (!controller_state.request_active) {
        return {};
    }

    if (controller_state.request_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return {};
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
        dialog_state.status_message = result.message;
        dialog_state.status_message_is_error = false;
        return {};
    }
    if (result.success) {
        dialog_state.mode = song_select::login_dialog_mode::login;
        dialog_state.verification = auth::verification_purpose::none;
        dialog_state.verification_email.clear();
        dialog_state.verification_code_input.value.clear();
    }
    dialog_state.status_message = result.message;
    dialog_state.status_message_is_error = !result.success;

    const auth::session_summary summary = auth::load_session_summary();
    dialog_state.email_input.value = summary.email;
    return {};
}

}  // namespace auth_overlay
