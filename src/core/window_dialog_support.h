#pragma once

namespace window_dialog_support {

bool is_fullscreen();
void toggle_fullscreen();
void apply_windowed_layout(int client_width, int client_height);
void set_fullscreen(bool fullscreen, int windowed_client_width, int windowed_client_height);
void* native_window_handle();

}  // namespace window_dialog_support
