#pragma once

#include "data_models.h"

namespace content_sync_service {

struct state {
    content_source source = content_source::unknown;
    content_sync_state sync = content_sync_state::unknown;
};

content_source source_from_status(content_status status);
content_sync_state sync_from_status(content_status status);
state state_from_legacy_status(content_status source_status, content_status display_status);
content_status legacy_status_for_display(state value);
bool is_modified(content_sync_state sync);
bool is_update_available(content_sync_state sync);

}  // namespace content_sync_service
