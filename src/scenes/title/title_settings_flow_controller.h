#pragma once

#include <optional>

#include "title/title_common_update_controller.h"
#include "title/title_settings_overlay.h"

namespace title {

struct settings_flow_result {
    std::optional<common_mode> return_mode;
    bool refresh_auth_state = false;
};

[[nodiscard]] settings_flow_result update_settings_flow(title_settings_overlay& settings_overlay,
                                                        common_mode return_mode,
                                                        float dt);

}  // namespace title
