#pragma once

#include <string_view>

namespace window_dialog_support {

bool is_fullscreen();
void toggle_fullscreen();
void minimize_window();
void apply_windowed_layout(int client_width, int client_height);
void set_fullscreen(bool fullscreen, int windowed_client_width, int windowed_client_height);
int current_monitor_width();
int current_monitor_height();
void* native_window_handle();
bool open_url(std::string_view url);

}  // namespace window_dialog_support
