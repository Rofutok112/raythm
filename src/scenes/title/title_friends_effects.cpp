#include "title/title_friends_effects.h"

namespace title_friends_effects {

void apply_notice(const notice_request& notice) {
    ui::notify(notice.message, notice.tone, notice.seconds);
}

void apply_effects(const feature_effects& effects) {
    for (const notice_request& notice : effects.notices) {
        apply_notice(notice);
    }
}

}  // namespace title_friends_effects
