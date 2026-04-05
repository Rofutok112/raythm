cmake_minimum_required(VERSION 4.1)
project(raythm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)
set(RAYTHM_APP_ICON_PNG
        "${CMAKE_SOURCE_DIR}/icon/icon_256.png"
        CACHE FILEPATH "Source PNG used for the Windows app icon")
set(RAYTHM_PACKAGE_VERSION "0.1.0" CACHE STRING "Version string used for Windows release packages")
set(RAYTHM_PACKAGE_FILE_NAME
        "raythm-${RAYTHM_PACKAGE_VERSION}-windows-x64"
        CACHE STRING "Package file name without extension")
set(RAYTHM_BASS_DIR
        "${CMAKE_SOURCE_DIR}/libs/bass"
        CACHE PATH "Directory containing bass.h, bass.lib, and bass.dll")

set(RAYTHM_BASS_HEADER "${RAYTHM_BASS_DIR}/bass.h")
set(RAYTHM_BASS_IMPORT_LIB "${RAYTHM_BASS_DIR}/bass.lib")
set(RAYTHM_BASS_RUNTIME_DLL "${RAYTHM_BASS_DIR}/bass.dll")

foreach(required_bass_file IN ITEMS
        "${RAYTHM_BASS_HEADER}"
        "${RAYTHM_BASS_IMPORT_LIB}"
        "${RAYTHM_BASS_RUNTIME_DLL}")
    if(NOT EXISTS "${required_bass_file}")
        message(FATAL_ERROR
                "Missing required BASS dependency: ${required_bass_file}\n"
                "Ensure libs/bass is checked into the repository or set RAYTHM_BASS_DIR to a directory containing bass.h, bass.lib, and bass.dll.")
    endif()
endforeach()

include(FetchContent)
FetchContent_Declare(
        raylib
        GIT_REPOSITORY  https://github.com/raysan5/raylib.git
        GIT_TAG         5.5
)
FetchContent_MakeAvailable(raylib)

if(WIN32)
    if(NOT EXISTS "${RAYTHM_APP_ICON_PNG}")
        message(FATAL_ERROR "Missing required Windows app icon: ${RAYTHM_APP_ICON_PNG}")
    endif()

    find_program(RAYTHM_POWERSHELL_EXECUTABLE NAMES powershell pwsh REQUIRED)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RAYTHM_APP_ICON_PNG}")

    set(RAYTHM_APP_ICON_ICO "${CMAKE_CURRENT_BINARY_DIR}/raythm.ico")
    set(RAYTHM_APP_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/raythm_app_icon.rc")

    execute_process(
            COMMAND "${RAYTHM_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_SOURCE_DIR}/cmake/convert_png_to_ico.ps1"
                    -InputPng "${RAYTHM_APP_ICON_PNG}"
                    -OutputIco "${RAYTHM_APP_ICON_ICO}"
            COMMAND_ERROR_IS_FATAL ANY
    )

    configure_file("${CMAKE_SOURCE_DIR}/src/platform/windows_app_icon.rc.in"
            "${RAYTHM_APP_ICON_RC}" @ONLY)
endif()

add_executable(raythm WIN32 src/main.cpp
        src/audio/audio.cpp
        src/audio/audio.h
        src/audio/audio_manager.cpp
        src/audio/audio_manager.h
        src/audio/audio_waveform.cpp
        src/audio/audio_waveform.h
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_view.cpp
        src/scenes/editor/editor_timeline_view.h
        src/scenes/editor/editor_timing_panel.cpp
        src/scenes/editor/editor_timing_panel.h
        src/platform/windows_input_source.cpp
        src/platform/windows_input_source.h
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_serializer.cpp
        src/gameplay/chart_serializer.h
        src/models/data_models.h
        src/gameplay/input_handler.cpp
        src/gameplay/input_handler.h
        src/gameplay/judge_system.cpp
        src/gameplay/judge_system.h
        src/gameplay/player_note_offsets.cpp
        src/gameplay/player_note_offsets.h
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
        src/core/official_content_sync.cpp
        src/core/official_content_sync.h
        src/core/window_dialog_support.cpp
        src/core/window_dialog_support.h
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
        src/scenes/title/title_bgm_controller.cpp
        src/scenes/title/title_bgm_controller.h
        src/scenes/title/title_spectrum_visualizer.cpp
        src/scenes/title/title_spectrum_visualizer.h
        src/scenes/song_create_scene.cpp
        src/scenes/song_create_scene.h
        src/scenes/song_select/song_catalog_service.cpp
        src/scenes/song_select/song_catalog_service.h
        src/scenes/song_select/song_select_confirmation_dialog.cpp
        src/scenes/song_select/song_select_confirmation_dialog.h
        src/scenes/song_select/song_import_export_service.cpp
        src/scenes/song_select/song_import_export_service.h
        src/scenes/song_select/song_preview_controller.cpp
        src/scenes/song_select/song_preview_controller.h
        src/scenes/song_select/song_select_detail_view.cpp
        src/scenes/song_select/song_select_detail_view.h
        src/scenes/song_select/song_select_layout.h
        src/scenes/song_select/song_select_list_view.cpp
        src/scenes/song_select/song_select_list_view.h
        src/scenes/song_select/song_select_navigation.cpp
        src/scenes/song_select/song_select_navigation.h
        src/scenes/song_select/song_select_overlay_view.cpp
        src/scenes/song_select/song_select_overlay_view.h
        src/scenes/song_select/song_select_state.cpp
        src/scenes/song_select/song_select_state.h
        src/scenes/song_select_scene.cpp
        src/scenes/song_select_scene.h
        src/scenes/editor/controller/editor_runtime_controller.cpp
        src/scenes/editor/controller/editor_runtime_controller.h
        src/scenes/editor/editor_flow_controller.cpp
        src/scenes/editor/editor_flow_controller.h
        src/scenes/editor/view/editor_cursor_hud_view.cpp
        src/scenes/editor/view/editor_cursor_hud_view.h
        src/scenes/editor/view/editor_header_view.cpp
        src/scenes/editor/view/editor_header_view.h
        src/scenes/editor/view/editor_layout.h
        src/scenes/editor/view/editor_left_panel_view.cpp
        src/scenes/editor/view/editor_left_panel_view.h
        src/scenes/editor/view/editor_modal_view.cpp
        src/scenes/editor/view/editor_modal_view.h
        src/scenes/editor/editor_panel_controller.cpp
        src/scenes/editor/editor_panel_controller.h
        src/scenes/editor/view/editor_right_panel_view.cpp
        src/scenes/editor/view/editor_right_panel_view.h
        src/scenes/editor/editor_scene_sync.cpp
        src/scenes/editor/editor_scene_sync.h
        src/scenes/editor/editor_scene_types.h
        src/scenes/editor/editor_session_loader.cpp
        src/scenes/editor/editor_session_loader.h
        src/scenes/editor/service/editor_chart_identity_service.cpp
        src/scenes/editor/service/editor_chart_identity_service.h
        src/scenes/editor/service/editor_metadata_service.cpp
        src/scenes/editor/service/editor_metadata_service.h
        src/scenes/editor/service/editor_transport_service.cpp
        src/scenes/editor/service/editor_transport_service.h
        src/scenes/editor/service/editor_timing_edit_service.cpp
        src/scenes/editor/service/editor_timing_edit_service.h
        src/scenes/editor/service/editor_timing_selection_service.cpp
        src/scenes/editor/service/editor_timing_selection_service.h
        src/scenes/editor/editor_timeline_controller.cpp
        src/scenes/editor/editor_timeline_controller.h
        src/scenes/editor/editor_transport_controller.cpp
        src/scenes/editor/editor_transport_controller.h
        src/scenes/editor/view/editor_timeline_presenter.cpp
        src/scenes/editor/view/editor_timeline_presenter.h
        src/scenes/editor/viewport/editor_timeline_viewport.cpp
        src/scenes/editor/viewport/editor_timeline_viewport.h
        src/scenes/editor_scene.cpp
        src/scenes/editor_scene.h
        src/scenes/play/play_flow_controller.cpp
        src/scenes/play/play_flow_controller.h
        src/scenes/play/play_chart_filter.cpp
        src/scenes/play/play_chart_filter.h
        src/scenes/play/play_note_draw_queue.cpp
        src/scenes/play/play_note_draw_queue.h
        src/scenes/play/play_renderer.cpp
        src/scenes/play/play_renderer.h
        src/scenes/play_scene.cpp
        src/scenes/play_scene.h
        src/scenes/play/play_session_loader.cpp
        src/scenes/play/play_session_loader.h
        src/scenes/play/play_session_types.h
        src/scenes/result_scene.cpp
        src/scenes/result_scene.h
        src/scenes/shared/scene_fade.cpp
        src/scenes/shared/scene_fade.h
        src/scenes/settings/settings_key_config_state.cpp
        src/scenes/settings/settings_key_config_state.h
        src/scenes/settings/settings_layout.h
        src/scenes/settings/settings_pages.cpp
        src/scenes/settings/settings_pages.h
        src/scenes/settings/settings_runtime_applier.cpp
        src/scenes/settings/settings_runtime_applier.h
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

add_executable(raythm_launcher
        src/launcher_main.cpp
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/updater/update_log.cpp
        src/updater/update_log.h
        src/updater/update_release.cpp
        src/updater/update_release.h
        src/updater/update_paths.cpp
        src/updater/update_paths.h
        src/updater/update_version.cpp
        src/updater/update_version.h
        src/updater/update_workflow.cpp
        src/updater/update_workflow.h)

add_executable(raythm_updater
        src/updater_main.cpp
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/updater/update_log.cpp
        src/updater/update_log.h
        src/updater/update_apply.cpp
        src/updater/update_apply.h
        src/updater/update_download.cpp
        src/updater/update_download.h
        src/updater/update_extract.cpp
        src/updater/update_extract.h
        src/updater/update_process.cpp
        src/updater/update_process.h
        src/updater/update_release.cpp
        src/updater/update_release.h
        src/updater/update_paths.cpp
        src/updater/update_paths.h
        src/updater/update_version.cpp
        src/updater/update_version.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/updater/update_workflow.cpp
        src/updater/update_workflow.h)

set_target_properties(raythm_launcher PROPERTIES OUTPUT_NAME "Launcher")
set_target_properties(raythm_updater PROPERTIES OUTPUT_NAME "Updater")

if(WIN32)
    target_sources(raythm PRIVATE "${RAYTHM_APP_ICON_RC}")
    target_sources(raythm_launcher PRIVATE "${RAYTHM_APP_ICON_RC}")
    target_sources(raythm_updater PRIVATE "${RAYTHM_APP_ICON_RC}")
endif()

add_executable(chart_parser_smoke
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/tests/chart_parser_smoke.cpp
        src/models/data_models.h)

add_executable(chart_serializer_smoke
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_serializer.cpp
        src/gameplay/chart_serializer.h
        src/tests/chart_serializer_smoke.cpp
        src/models/data_models.h)

add_executable(chart_difficulty_smoke
        src/models/data_models.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/tests/chart_difficulty_smoke.cpp)

add_executable(editor_state_smoke
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/models/data_models.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/tests/editor_state_smoke.cpp)

add_executable(editor_meter_map_smoke
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/models/data_models.h
        src/tests/editor_meter_map_smoke.cpp)

add_executable(song_loader_smoke
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/models/data_models.h
        src/gameplay/song_loader.cpp
        src/gameplay/song_loader.h
        src/tests/song_loader_smoke.cpp)

add_executable(song_select_state_smoke
        src/models/data_models.h
        src/scenes/song_select/song_select_layout.h
        src/scenes/song_select/song_select_state.cpp
        src/scenes/song_select/song_select_state.h
        src/tests/song_select_state_smoke.cpp)

add_executable(settings_io_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/gameplay/settings_io.cpp
        src/gameplay/settings_io.h
        src/tests/settings_io_smoke.cpp)

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
        src/scenes/play/play_note_draw_queue.cpp
        src/scenes/play/play_note_draw_queue.h
        src/tests/play_note_draw_queue_smoke.cpp)

add_executable(play_chart_filter_smoke
        src/models/data_models.h
        src/scenes/play/play_chart_filter.cpp
        src/scenes/play/play_chart_filter.h
        src/tests/play_chart_filter_smoke.cpp)

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
        src/scenes/play/play_flow_controller.cpp
        src/scenes/play/play_flow_controller.h
        src/scenes/play/play_note_draw_queue.cpp
        src/scenes/play/play_note_draw_queue.h
        src/scenes/play/play_session_types.h
        src/scenes/editor/editor_scene_types.h
        src/tests/play_flow_controller_smoke.cpp)

add_executable(editor_transport_controller_smoke
        src/audio/audio_manager.h
        src/audio/audio_waveform.h
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_view.h
        src/scenes/editor/editor_timing_panel.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/models/data_models.h
        src/scenes/editor/editor_scene_types.h
        src/scenes/editor/editor_transport_controller.cpp
        src/scenes/editor/editor_transport_controller.h
        src/tests/editor_transport_controller_smoke.cpp)

add_executable(editor_flow_controller_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/path_utils.h
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_view.h
        src/scenes/editor/editor_timing_panel.h
        src/gameplay/chart_serializer.cpp
        src/gameplay/chart_serializer.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/models/data_models.h
        src/scenes/editor/editor_flow_controller.cpp
        src/scenes/editor/editor_flow_controller.h
        src/scenes/editor/editor_scene_types.h
        src/tests/editor_flow_controller_smoke.cpp)

add_executable(editor_timeline_controller_smoke
        src/audio/audio_manager.h
        src/audio/audio_waveform.h
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_view.cpp
        src/scenes/editor/editor_timeline_view.h
        src/scenes/editor/editor_timing_panel.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/models/data_models.h
        src/scenes/editor/editor_scene_types.h
        src/scenes/editor/editor_timeline_controller.cpp
        src/scenes/editor/editor_timeline_controller.h
        src/ui/ui_clip.cpp
        src/ui/ui_clip.h
        src/tests/editor_timeline_controller_smoke.cpp)

add_executable(editor_panel_controller_smoke
        src/audio/audio_manager.h
        src/audio/audio_waveform.h
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_view.h
        src/scenes/editor/editor_timing_panel.h
        src/models/data_models.h
        src/scenes/editor/editor_panel_controller.cpp
        src/scenes/editor/editor_panel_controller.h
        src/scenes/editor/editor_scene_types.h
        src/tests/editor_panel_controller_smoke.cpp)

add_executable(audio_manager_smoke
        src/audio/audio.cpp
        src/audio/audio.h
        src/audio/audio_manager.cpp
        src/audio/audio_manager.h
        src/tests/audio_manager_smoke.cpp)

add_executable(update_version_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/updater/update_apply.cpp
        src/updater/update_apply.h
        src/updater/update_download.cpp
        src/updater/update_download.h
        src/updater/update_extract.cpp
        src/updater/update_extract.h
        src/updater/update_process.cpp
        src/updater/update_process.h
        src/updater/update_release.cpp
        src/updater/update_release.h
        src/updater/update_paths.cpp
        src/updater/update_paths.h
        src/updater/update_version.cpp
        src/updater/update_version.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/updater/update_workflow.cpp
        src/updater/update_workflow.h
        src/tests/update_version_smoke.cpp)

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
target_include_directories(raythm PRIVATE ${RAYTHM_BASS_DIR} ${RAYTHM_INCLUDE_DIRS})
target_include_directories(raythm_launcher PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(raythm_updater PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(chart_parser_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(chart_serializer_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(chart_difficulty_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(editor_state_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(editor_meter_map_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(song_loader_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(song_select_state_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(settings_io_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(input_handler_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(timing_engine_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(judge_system_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(score_system_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(play_note_draw_queue_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(play_chart_filter_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(play_flow_controller_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(editor_transport_controller_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(editor_flow_controller_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(editor_timeline_controller_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(editor_panel_controller_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_include_directories(audio_manager_smoke PRIVATE ${RAYTHM_BASS_DIR} ${RAYTHM_INCLUDE_DIRS})
target_include_directories(update_version_smoke PRIVATE ${RAYTHM_INCLUDE_DIRS})
target_link_libraries(raythm PRIVATE raylib ${RAYTHM_BASS_IMPORT_LIB} comdlg32)
target_link_libraries(raythm_launcher PRIVATE winhttp)
target_link_libraries(raythm_updater PRIVATE winhttp)
target_link_libraries(update_version_smoke PRIVATE winhttp)
target_link_libraries(song_select_state_smoke PRIVATE raylib)
target_link_libraries(settings_io_smoke PRIVATE raylib)
target_link_libraries(input_handler_smoke PRIVATE raylib)
target_link_libraries(judge_system_smoke PRIVATE raylib)
target_link_libraries(play_flow_controller_smoke PRIVATE raylib)
target_link_libraries(editor_timeline_controller_smoke PRIVATE raylib)
target_link_libraries(editor_panel_controller_smoke PRIVATE raylib)
target_link_libraries(audio_manager_smoke PRIVATE ${RAYTHM_BASS_IMPORT_LIB})
target_compile_definitions(raythm_launcher PRIVATE RAYTHM_PACKAGE_VERSION_STRING="${RAYTHM_PACKAGE_VERSION}")

set(RAYTHM_RUNTIME_DLLS
        ${RAYTHM_BASS_RUNTIME_DLL}
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
        ${RAYTHM_BASS_RUNTIME_DLL}
        $<TARGET_FILE_DIR:audio_manager_smoke>/bass.dll
)

install(TARGETS raythm raythm_launcher raythm_updater RUNTIME DESTINATION .)
install(FILES ${RAYTHM_RUNTIME_DLLS} DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION .)

if(WIN32)
    set(CPACK_PACKAGE_NAME "raythm")
    set(CPACK_PACKAGE_VENDOR "Rofutok112")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "beatmania-style rhythm game")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "raythm")
    set(CPACK_PACKAGE_VERSION "${RAYTHM_PACKAGE_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "${RAYTHM_PACKAGE_FILE_NAME}")
    set(CPACK_PACKAGE_EXECUTABLES "Launcher" "raythm")
    set(CPACK_CREATE_DESKTOP_LINKS "raythm")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
    set(CPACK_NSIS_DISPLAY_NAME "raythm")
    set(CPACK_NSIS_PACKAGE_NAME "raythm")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "raythm")
    set(CPACK_NSIS_MUI_ICON "${RAYTHM_APP_ICON_ICO}")
    set(CPACK_NSIS_MUI_UNIICON "${RAYTHM_APP_ICON_ICO}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
    set(CPACK_MONOLITHIC_INSTALL ON)
    include(CPack)
endif()
