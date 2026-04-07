#pragma once

#include "../api/mv_scene.h"

namespace mv {

// Render a scene using raylib 2D draw calls.
// Assumes virtual_screen coordinate system (1280x720) is already active.
void render_scene(const scene& sc);

} // namespace mv
