#include "title/title_command_dispatcher.h"

namespace title {

bool dispatch_command(const command_dispatch_context& context, const command& command) {
    switch (command.type) {
        case command_type::none:
            return false;
        case command_type::enter_home:
            if (context.enter_home) {
                context.enter_home();
            }
            return true;
        case command_type::open_update_catalog:
            if (context.open_update_catalog) {
                context.open_update_catalog(command.song_id, command.chart_id);
            }
            return true;
        case command_type::add_selected_to_multiplayer:
            return context.add_selected_to_multiplayer ? context.add_selected_to_multiplayer() : false;
        case command_type::open_self_profile:
            if (context.open_self_profile) {
                context.open_self_profile();
            }
            return true;
        case command_type::open_public_profile:
            if (context.open_public_profile) {
                context.open_public_profile(command.user_id);
            }
            return true;
        case command_type::open_multiplayer_song_select:
            if (context.open_multiplayer_song_select) {
                context.open_multiplayer_song_select();
            }
            return true;
    }
    return false;
}

}  // namespace title
