#pragma once

class scene_manager;

namespace window_chrome {

bool initialize(void* native_window_handle);
void shutdown();

void update(scene_manager& manager);
void draw();

int titlebar_height_px();
bool is_maximized();
bool is_state_transitioning();
bool is_pointer_over_chrome();
void minimize();
void maximize();
void toggle_maximize_restore();

}  // namespace window_chrome
