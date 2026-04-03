#pragma once

#include <array>

#include "play_note_draw_queue.h"
#include "play_session_types.h"
#include "raylib.h"

namespace play_renderer {

Rectangle pause_panel_rect();
std::array<Rectangle, 3> pause_button_rects();

void draw_status(const play_session_state& state);
void draw_world_background();
void draw_world(const play_session_state& state, const play_note_draw_queue& draw_queue,
                float lane_start_z, float judgement_z, float lane_end_z, double visual_ms);
void draw_overlay(const play_session_state& state);

}  // namespace play_renderer
