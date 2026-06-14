#include "title/title_shell_effects.h"

namespace title {

shell_effect_result apply_friends_effects(const title_friends_effects::feature_effects& effects,
                                          const shell_effect_context& context) {
    shell_effect_result result;
    title_friends_effects::apply_effects(effects);
    if (effects.room_join.has_value()) {
        if (context.start_room_join) {
            context.start_room_join(*effects.room_join);
        }
        result.scene_flow_changed = true;
        return result;
    }
    if (effects.profile_user_id.has_value()) {
        if (context.open_public_profile) {
            context.open_public_profile(*effects.profile_user_id);
        }
        result.scene_flow_changed = true;
    }
    return result;
}

shell_effect_result apply_public_profile_effects(const public_profile::effects& effects,
                                                 const shell_effect_context& context) {
    public_profile::apply_effects(effects);
    if (effects.reload_friends && context.reload_friends) {
        context.reload_friends();
    }
    return {};
}

}  // namespace title
