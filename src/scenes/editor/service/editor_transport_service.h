#pragma once

#include <optional>
#include <string>

#include "editor/editor_scene_types.h"

namespace editor_transport_service {

void sync(editor_transport_state& transport,
          const editor_state* state,
          const std::string& hitsound_path,
          const editor_hitsound_paths* hitsounds = nullptr,
          bool suppress_hitsounds = false,
          double dt = 0.0);

std::optional<int> toggle_playback(editor_transport_state& transport,
                                   const editor_state* state,
                                   std::optional<int>& space_playback_start_tick,
                                   const std::string& hitsound_path,
                                   const editor_hitsound_paths* hitsounds = nullptr);

void pause_for_seek(editor_transport_state& transport,
                    const editor_state* state,
                    std::optional<int>& space_playback_start_tick,
                    const std::string& hitsound_path,
                    const editor_hitsound_paths* hitsounds = nullptr);

void seek_to_tick(editor_transport_state& transport,
                  const editor_state* state,
                  int tick,
                  const std::string& hitsound_path,
                  const editor_hitsound_paths* hitsounds = nullptr);

std::string playback_status_text(const editor_transport_state& transport);

}  // namespace editor_transport_service
