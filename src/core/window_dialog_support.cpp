#include "window_dialog_support.h"

#include "raylib.h"

namespace window_dialog_support {

bool is_fullscreen() {
    return IsWindowFullscreen();
}

void toggle_fullscreen() {
    ToggleFullscreen();
}

void* native_window_handle() {
    return GetWindowHandle();
}

}  // namespace window_dialog_support
