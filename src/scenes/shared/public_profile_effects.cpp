#include "shared/public_profile_effects.h"

namespace public_profile {

void apply_notice(const notice_request& notice) {
    ui::notify(notice.message, notice.tone, notice.seconds);
}

void apply_effects(const effects& effects) {
    for (const notice_request& notice : effects.notices) {
        apply_notice(notice);
    }
}

}  // namespace public_profile
