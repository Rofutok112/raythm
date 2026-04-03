cmake_minimum_required(VERSION 4.1)
project(raythm)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
        raylib
        GIT_REPOSITORY  https://github.com/raysan5/raylib.git
        GIT_TAG         5.5
)
FetchContent_MakeAvailable(raylib)

add_executable(raythm src/main.cpp
        libs/bass/bass.h
        src/audio/audio.cpp
        src/audio/audio.h
        src/audio/audio_manager.cpp
        src/audio/audio_manager.h
        src/audio/audio_waveform.cpp
        src/audio/audio_waveform.h
        src/editor/editor_command.h
        src/editor/editor_meter_map.cpp
        src/editor/editor_meter_map.h
        src/editor/editor_state.cpp
        src/editor/editor_state.h
        src/editor/editor_timeline_view.cpp
        src/editor/editor_timeline_view.h
        src/editor/editor_timing_panel.cpp
        src/editor/editor_timing_panel.h
        src/platform/windows_input_source.cpp
        src/platform/windows_input_source.h
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/gameplay/chart_serializer.cpp
        src/gameplay/chart_serializer.h
        src/models/data_models.h
        src/gameplay/input_handler.cpp
        src/gameplay/input_handler.h
        src/gameplay/judge_system.cpp
        src/gameplay/judge_system.h
        src/gameplay/score_system.cpp
        src/gameplay/score_system.h
        src/gameplay/song_loader.cpp
        src/gameplay/song_loader.h
        src/gameplay/song_writer.cpp
        src/gameplay/song_writer.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/file_dialog.cpp
        src/core/file_dialog.h
        src/core/scene.cpp
        src/core/scene.h
        src/core/scene_manager.cpp
        src/core/scene_manager.h
        src/core/uuid_util.h
        src/gameplay/game_settings.h
        src/gameplay/key_names.h
        src/gameplay/settings_io.cpp
        src/gameplay/settings_io.h
        src/scenes/scene_common.cpp
        src/scenes/scene_common.h
        src/scenes/title_scene.cpp
        src/scenes/title_scene.h
        src/scenes/song_create_scene.cpp
        src/scenes/song_create_scene.h
        src/scenes/song_select_scene.cpp
        src/scenes/song_select_scene.h
        src/scenes/editor_scene.cpp
        src/scenes/editor_scene.h
        src/scenes/play_flow_controller.cpp
        src/scenes/play_flow_controller.h
        src/scenes/play_note_draw_queue.cpp
        src/scenes/play_note_draw_queue.h
        src/scenes/play_renderer.cpp
        src/scenes/play_renderer.h
        src/scenes/play_scene.cpp
        src/scenes/play_scene.h
        src/scenes/play_session_loader.cpp
        src/scenes/play_session_loader.h
        src/scenes/play_session_types.h
        src/scenes/result_scene.cpp
        src/scenes/result_scene.h
        src/scenes/settings_scene.cpp
        src/scenes/settings_scene.h
        src/scenes/theme.h
        src/scenes/virtual_screen.cpp
        src/scenes/virtual_screen.h
        src/game_scenes.h
        src/ui/ui_layout.h
        src/ui/ui_text.h
        src/ui/ui_hit.h
        src/ui/ui_draw.h
        src/ui/ui_clip.cpp
        src/ui/ui_clip.h)

add_executable(chart_parser_smoke
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/tests/chart_parser_smoke.cpp
        src/models/data_models.h)

add_executable(chart_serializer_smoke
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/gameplay/chart_serializer.cpp
        src/gameplay/chart_serializer.h
        src/tests/chart_serializer_smoke.cpp
        src/models/data_models.h)

add_executable(editor_state_smoke
        src/editor/editor_command.h
        src/editor/editor_meter_map.cpp
        src/editor/editor_meter_map.h
        src/editor/editor_state.cpp
        src/editor/editor_state.h
        src/models/data_models.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/tests/editor_state_smoke.cpp)

add_executable(song_loader_smoke
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/models/data_models.h
        src/gameplay/song_loader.cpp
        src/gameplay/song_loader.h
        src/tests/song_loader_smoke.cpp)

add_executable(input_handler_smoke
        src/platform/windows_input_source.cpp
        src/platform/windows_input_source.h
        src/gameplay/input_handler.cpp
        src/gameplay/input_handler.h
        src/tests/input_handler_smoke.cpp)

add_executable(timing_engine_smoke
        src/models/data_models.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/tests/timing_engine_smoke.cpp)

add_executable(judge_system_smoke
        src/models/data_models.h
        src/platform/windows_input_source.cpp
        src/platform/windows_input_source.h
        src/gameplay/input_handler.cpp
        src/gameplay/input_handler.h
        src/gameplay/judge_system.cpp
        src/gameplay/judge_system.h
        src/tests/judge_system_smoke.cpp
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h)

add_executable(score_system_smoke
        src/models/data_models.h
        src/gameplay/score_system.cpp
        src/gameplay/score_system.h
        src/tests/score_system_smoke.cpp)

add_executable(play_note_draw_queue_smoke
        src/models/data_models.h
        src/scenes/play_note_draw_queue.cpp
        src/scenes/play_note_draw_queue.h
        src/tests/play_note_draw_queue_smoke.cpp)

add_executable(play_flow_controller_smoke
        src/models/data_models.h
        src/platform/windows_input_source.cpp
        src/platform/windows_input_source.h
        src/gameplay/input_handler.cpp
        src/gameplay/input_handler.h
        src/gameplay/judge_system.cpp
        src/gameplay/judge_system.h
        src/gameplay/score_system.cpp
        src/gameplay/score_system.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/scenes/play_flow_controller.cpp
        src/scenes/play_flow_controller.h
        src/scenes/play_note_draw_queue.cpp
        src/scenes/play_note_draw_queue.h
        src/scenes/play_session_types.h
        src/scenes/editor_scene.h
        src/tests/play_flow_controller_smoke.cpp)

add_executable(audio_manager_smoke
        libs/bass/bass.h
        src/audio/audio.cpp
        src/audio/audio.h
        src/audio/audio_manager.cpp
        src/audio/audio_manager.h
        src/tests/audio_manager_smoke.cpp)

set(RAYTHM_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/audio
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/editor
        ${CMAKE_SOURCE_DIR}/src/gameplay
        ${CMAKE_SOURCE_DIR}/src/models
        ${CMAKE_SOURCE_DIR}/src/scenes
        ${CMAKE_SOURCE_DIR}/src/ui
        ${CMAKE_SOURCE_DIR}/src/tests
)

# BASSライブラリ
target_include_directories(raythm PRIVATE ${CMAKE_SOURCE_DIR}/libs/bass ${RAYTHM_INCLUDE_DIRS})
target_include_directories(chart_parser_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(chart_serializer_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(editor_state_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(song_loader_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(input_handler_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(timing_engine_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(judge_system_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(score_system_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(play_note_draw_queue_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(play_flow_controller_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(audio_manager_smoke PRIVATE ${CMAKE_SOURCE_DIR}/libs/bass ${RAYTHM_INCLUDE_DIRS})
target_link_libraries(raythm PRIVATE raylib ${CMAKE_SOURCE_DIR}/libs/bass/bass.lib comdlg32)
target_link_libraries(input_handler_smoke PRIVATE raylib)
target_link_libraries(judge_system_smoke PRIVATE raylib)
target_link_libraries(play_flow_controller_smoke PRIVATE raylib)
target_link_libraries(audio_manager_smoke PRIVATE ${CMAKE_SOURCE_DIR}/libs/bass/bass.lib)

set(RAYTHM_RUNTIME_DLLS
        ${CMAKE_SOURCE_DIR}/libs/bass/bass.dll
)

if(MINGW)
    get_filename_component(RAYTHM_COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(runtime_name IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        set(runtime_path "${RAYTHM_COMPILER_BIN_DIR}/${runtime_name}")
        if(EXISTS "${runtime_path}")
            list(APPEND RAYTHM_RUNTIME_DLLS "${runtime_path}")
        endif()
    endforeach()
endif()

# bass.dll を実行ファイルと同じディレクトリにコピー
add_custom_command(TARGET raythm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${RAYTHM_RUNTIME_DLLS}
        $<TARGET_FILE_DIR:raythm>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:raythm>/assets
        COMMAND_EXPAND_LISTS
)

add_custom_command(TARGET audio_manager_smoke POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/libs/bass/bass.dll
        $<TARGET_FILE_DIR:audio_manager_smoke>/bass.dll
)

install(TARGETS raythm RUNTIME DESTINATION .)
install(FILES ${RAYTHM_RUNTIME_DLLS} DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION .)
