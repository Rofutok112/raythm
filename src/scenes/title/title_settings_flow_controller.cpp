#include "title/title_settings_flow_controller.h"

namespace title {

settings_flow_result update_settings_flow(title_settings_overlay& settings_overlay,
                                          common_mode return_mode,
                                          float dt) {
    if (settings_overlay.closing()) {
        if (settings_overlay.closed()) {
            return {
                .return_mode = return_mode,
                .refresh_auth_state = true,
            };
        }
        return {};
    }

    settings_overlay.update(dt);
    return {};
}

}  // namespace title
