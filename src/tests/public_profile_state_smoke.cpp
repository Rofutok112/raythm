#include <cstdlib>
#include <iostream>
#include <string>

#include "shared/public_profile_state.h"

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

auth::public_profile profile(std::string relationship_status, std::string request_id = "") {
    return {
        .id = "user-a",
        .display_name = "Aki",
        .relationship_status = std::move(relationship_status),
        .relationship_request_id = std::move(request_id),
    };
}

void expect_action(const auth::public_profile& profile,
                   public_profile_state::relationship_action action,
                   const char* label,
                   bool enabled,
                   bool& ok) {
    const public_profile_state::relationship_action_view view =
        public_profile_state::relationship_action_for(profile, false);
    expect(view.action == action, std::string(label) + " action should map correctly.", ok);
    expect(std::string(view.label) == label, std::string(label) + " label should map correctly.", ok);
    expect(view.enabled == enabled, std::string(label) + " enabled state should map correctly.", ok);
}

}  // namespace

int main() {
    bool ok = true;

    expect_action(profile("none"),
                  public_profile_state::relationship_action::send_request,
                  "ADD",
                  true,
                  ok);
    expect_action(profile("pending_incoming", "request-1"),
                  public_profile_state::relationship_action::accept_request,
                  "ACCEPT",
                  true,
                  ok);
    expect_action(profile("pending_incoming"),
                  public_profile_state::relationship_action::none,
                  "ACCEPT",
                  false,
                  ok);
    expect_action(profile("pending_outgoing"),
                  public_profile_state::relationship_action::none,
                  "PENDING",
                  false,
                  ok);
    expect_action(profile("accepted"),
                  public_profile_state::relationship_action::remove_friend,
                  "REMOVE",
                  true,
                  ok);
    expect_action(profile("blocked"),
                  public_profile_state::relationship_action::unblock,
                  "UNBLOCK",
                  true,
                  ok);
    expect_action(profile("unavailable"),
                  public_profile_state::relationship_action::none,
                  "UNAVAILABLE",
                  false,
                  ok);
    expect_action(profile("self"),
                  public_profile_state::relationship_action::none,
                  "SELF",
                  false,
                  ok);

    const public_profile_state::relationship_action_view active_view =
        public_profile_state::relationship_action_for(profile("accepted"), true);
    expect(active_view.action == public_profile_state::relationship_action::none,
           "Active operation should not expose a new action.", ok);
    expect(std::string(active_view.label) == "WORKING",
           "Active operation should show WORKING.", ok);
    expect(!active_view.enabled,
           "Active operation should disable the button.", ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "public_profile_state smoke test passed\n";
    return EXIT_SUCCESS;
}
