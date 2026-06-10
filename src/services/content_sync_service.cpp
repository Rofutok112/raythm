#include "services/content_sync_service.h"

namespace content_sync_service {

content_source source_from_status(content_status status) {
    switch (status) {
    case content_status::local:
        return content_source::local;
    case content_status::official:
        return content_source::official;
    case content_status::community:
        return content_source::community;
    case content_status::update:
    case content_status::modified:
    case content_status::checking:
        return content_source::unknown;
    }
    return content_source::unknown;
}

content_sync_state sync_from_status(content_status status) {
    switch (status) {
    case content_status::update:
        return content_sync_state::update_available;
    case content_status::modified:
        return content_sync_state::modified;
    case content_status::checking:
        return content_sync_state::checking;
    case content_status::local:
    case content_status::official:
    case content_status::community:
        return content_sync_state::clean;
    }
    return content_sync_state::unknown;
}

state state_from_legacy_status(content_status source_status, content_status display_status) {
    state result;
    result.source = source_from_status(source_status);
    result.sync = sync_from_status(display_status);
    if (result.sync != content_sync_state::modified &&
        result.sync != content_sync_state::update_available &&
        result.sync != content_sync_state::checking) {
        result.sync = content_sync_state::clean;
    }
    return result;
}

content_status legacy_status_for_display(state value) {
    if (value.sync == content_sync_state::checking) {
        return content_status::checking;
    }
    if (value.sync == content_sync_state::modified) {
        return content_status::modified;
    }
    if (value.sync == content_sync_state::update_available) {
        return content_status::update;
    }

    switch (value.source) {
    case content_source::official:
        return content_status::official;
    case content_source::community:
        return content_status::community;
    case content_source::local:
    case content_source::unknown:
        return content_status::local;
    }
    return content_status::local;
}

bool is_modified(content_sync_state sync) {
    return sync == content_sync_state::modified;
}

bool is_update_available(content_sync_state sync) {
    return sync == content_sync_state::update_available;
}

}  // namespace content_sync_service
