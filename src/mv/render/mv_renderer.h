#pragma once

#include "../api/mv_scene.h"

namespace mv {

// Render a scene using raylib 2D draw calls.
// Assumes virtual_screen coordinate system (1920x1080) is already active.
void render_scene(const scene& sc);

} // namespace mv
