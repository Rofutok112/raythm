cmake_minimum_required(VERSION 4.1)
project(raythm)

include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)
set(RAYTHM_APP_ICON_PNG
        "${CMAKE_SOURCE_DIR}/icon/icon_256.png"
        CACHE FILEPATH "Source PNG used for the Windows app icon")
set(RAYTHM_PACKAGE_VERSION "0.1.0" CACHE STRING "Version string used for Windows release packages")
set(RAYTHM_PACKAGE_FILE_NAME
        "raythm-${RAYTHM_PACKAGE_VERSION}-windows-x64"
        CACHE STRING "Package file name without extension")
set(RAYTHM_SERVER_ENV "development" CACHE STRING "Server environment: production or development")
set_property(CACHE RAYTHM_SERVER_ENV PROPERTY STRINGS production development)
set(RAYTHM_API_BASE_URL "" CACHE STRING "Optional API base URL override for local builds")
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

add_compile_definitions(RAYTHM_SERVER_ENV="${RAYTHM_SERVER_ENV}")
if(NOT RAYTHM_API_BASE_URL STREQUAL "")
    add_compile_definitions(RAYTHM_API_BASE_URL="${RAYTHM_API_BASE_URL}")
endif()

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip
        URL_HASH SHA256=00e8a7c85f7a5221e56ee8e4e1c6642b773eae73d29379a91c8ac5149e803f42
)
FetchContent_MakeAvailable(raylib)

FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
        URL_HASH SHA256=04022b05d806eb5ff73023c280b68697d12b93e1b7267a0b22a1a39ec7578069
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
        lua
        URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
        URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
)
if(POLICY CMP0169)
    cmake_policy(PUSH)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
endif()
if(POLICY CMP0169)
    cmake_policy(POP)
endif()
add_library(lua STATIC
        "${lua_SOURCE_DIR}/src/lapi.c"
        "${lua_SOURCE_DIR}/src/lauxlib.c"
        "${lua_SOURCE_DIR}/src/lbaselib.c"
        "${lua_SOURCE_DIR}/src/lcode.c"
        "${lua_SOURCE_DIR}/src/lcorolib.c"
        "${lua_SOURCE_DIR}/src/lctype.c"
        "${lua_SOURCE_DIR}/src/ldblib.c"
        "${lua_SOURCE_DIR}/src/ldebug.c"
        "${lua_SOURCE_DIR}/src/ldo.c"
        "${lua_SOURCE_DIR}/src/ldump.c"
        "${lua_SOURCE_DIR}/src/lfunc.c"
        "${lua_SOURCE_DIR}/src/lgc.c"
        "${lua_SOURCE_DIR}/src/linit.c"
        "${lua_SOURCE_DIR}/src/liolib.c"
        "${lua_SOURCE_DIR}/src/llex.c"
        "${lua_SOURCE_DIR}/src/lmathlib.c"
        "${lua_SOURCE_DIR}/src/lmem.c"
        "${lua_SOURCE_DIR}/src/loadlib.c"
        "${lua_SOURCE_DIR}/src/lobject.c"
        "${lua_SOURCE_DIR}/src/lopcodes.c"
        "${lua_SOURCE_DIR}/src/loslib.c"
        "${lua_SOURCE_DIR}/src/lparser.c"
        "${lua_SOURCE_DIR}/src/lstate.c"
        "${lua_SOURCE_DIR}/src/lstring.c"
        "${lua_SOURCE_DIR}/src/lstrlib.c"
        "${lua_SOURCE_DIR}/src/ltable.c"
        "${lua_SOURCE_DIR}/src/ltablib.c"
        "${lua_SOURCE_DIR}/src/ltm.c"
        "${lua_SOURCE_DIR}/src/lundump.c"
        "${lua_SOURCE_DIR}/src/lutf8lib.c"
        "${lua_SOURCE_DIR}/src/lvm.c"
        "${lua_SOURCE_DIR}/src/lzio.c")
target_include_directories(lua PUBLIC "${lua_SOURCE_DIR}/src")

FetchContent_Declare(
        sqlite3_amalgamation
        URL https://www.sqlite.org/2026/sqlite-amalgamation-3530000.zip
        URL_HASH SHA3_256=c2325c53b3b41761469f91cfb078e96882ac5d85bac10c11b0bd8f253b031e5b
)
if(POLICY CMP0169)
    cmake_policy(PUSH)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_GetProperties(sqlite3_amalgamation)
if(NOT sqlite3_amalgamation_POPULATED)
    FetchContent_Populate(sqlite3_amalgamation)
endif()
if(POLICY CMP0169)
    cmake_policy(POP)
endif()
add_library(sqlite3 STATIC "${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c")
target_include_directories(sqlite3 PUBLIC "${sqlite3_amalgamation_SOURCE_DIR}")

if(WIN32)
    if(NOT EXISTS "${RAYTHM_APP_ICON_PNG}")
        message(FATAL_ERROR "Missing required Windows app icon: ${RAYTHM_APP_ICON_PNG}")
    endif()

    find_program(RAYTHM_POWERSHELL_EXECUTABLE NAMES powershell pwsh REQUIRED)
    set(RAYTHM_APP_ICON_ICO "${CMAKE_CURRENT_BINARY_DIR}/raythm.ico")
    set(RAYTHM_APP_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/raythm_app_icon.rc")
    set(RAYTHM_APP_ICON_RC_TEMPLATE "${CMAKE_SOURCE_DIR}/src/platform/windows_app_icon.rc.in")
    set(RAYTHM_APP_ICON_RC_GENERATOR "${CMAKE_SOURCE_DIR}/cmake/generate_windows_app_icon_rc.cmake")

    add_custom_command(
            OUTPUT "${RAYTHM_APP_ICON_ICO}" "${RAYTHM_APP_ICON_RC}"
            COMMAND "${RAYTHM_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_SOURCE_DIR}/cmake/convert_png_to_ico.ps1"
                    -InputPng "${RAYTHM_APP_ICON_PNG}"
                    -OutputIco "${RAYTHM_APP_ICON_ICO}"
            COMMAND "${CMAKE_COMMAND}"
                    -DINPUT_FILE=${RAYTHM_APP_ICON_RC_TEMPLATE}
                    -DOUTPUT_FILE=${RAYTHM_APP_ICON_RC}
                    -DRAYTHM_APP_ICON_ICO=${RAYTHM_APP_ICON_ICO}
                    -P "${RAYTHM_APP_ICON_RC_GENERATOR}"
            DEPENDS
                    "${RAYTHM_APP_ICON_PNG}"
                    "${CMAKE_SOURCE_DIR}/cmake/convert_png_to_ico.ps1"
                    "${RAYTHM_APP_ICON_RC_TEMPLATE}"
                    "${RAYTHM_APP_ICON_RC_GENERATOR}"
            COMMENT "Generating Windows app icon resources"
            VERBATIM
    )
    set_source_files_properties("${RAYTHM_APP_ICON_ICO}" "${RAYTHM_APP_ICON_RC}" PROPERTIES GENERATED TRUE)
endif()

set(RAYTHM_ICON_MANIFEST "${CMAKE_SOURCE_DIR}/assets/icons/raythm_icons.json")
set(RAYTHM_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(RAYTHM_GENERATED_ICON_CPP "${RAYTHM_GENERATED_INCLUDE_DIR}/ui/icons/raythm_icons.cpp")
set(RAYTHM_GENERATED_ICON_H "${RAYTHM_GENERATED_INCLUDE_DIR}/ui/icons/raythm_icons.h")

add_custom_command(
        OUTPUT "${RAYTHM_GENERATED_ICON_CPP}" "${RAYTHM_GENERATED_ICON_H}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/svg2raylib.py"
                --manifest "${RAYTHM_ICON_MANIFEST}"
                --out "${RAYTHM_GENERATED_ICON_CPP}"
                --header-out "${RAYTHM_GENERATED_ICON_H}"
                --include "ui/icons/raythm_icons.h"
                --namespace raythm_icons
                --sdf-atlas
                --sdf-cell-size 80
        DEPENDS
                "${CMAKE_SOURCE_DIR}/tools/svg2raylib.py"
                "${RAYTHM_ICON_MANIFEST}"
        COMMENT "Generating raylib icon drawing code"
        VERBATIM
)

set_source_files_properties("${RAYTHM_GENERATED_ICON_CPP}" "${RAYTHM_GENERATED_ICON_H}" PROPERTIES GENERATED TRUE)

add_executable(raythm WIN32 src/main.cpp
        src/audio/audio.cpp
        src/audio/audio.h
        src/audio/audio_loudness.cpp
        src/audio/audio_loudness.h
        src/audio/audio_manager.cpp
        src/audio/audio_manager.h
        src/mv/mv_storage.cpp
        src/mv/mv_storage.h
        src/audio/audio_waveform.cpp
        src/audio/audio_waveform.h
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_note_geometry.cpp
        src/scenes/editor/editor_timeline_note_geometry.h
        src/scenes/editor/editor_timeline_types.cpp
        src/scenes/editor/editor_timeline_types.h
        src/scenes/editor/editor_timing_panel.cpp
        src/scenes/editor/editor_timing_panel.h
        src/platform/windows_input_source.cpp
        src/platform/windows_input_source.h
        src/platform/windows_app_icon.cpp
        src/platform/windows_app_icon.h
        src/platform/window_chrome.cpp
        src/platform/window_chrome.h
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/gameplay/chart_file_storage.cpp
        src/gameplay/chart_file_storage.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/chart_fingerprint.cpp
        src/gameplay/chart_fingerprint.h
        src/gameplay/chart_level_memory_cache.cpp
        src/gameplay/chart_level_memory_cache.h
        src/gameplay/chart_serializer.cpp
        src/gameplay/chart_serializer.h
        src/models/content_lifecycle.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/gameplay/lane_input_tracker.cpp
        src/gameplay/lane_input_tracker.h
        src/gameplay/input_handler.cpp
        src/gameplay/input_handler.h
        src/gameplay/judge_system.cpp
        src/gameplay/judge_system.h
        src/gameplay/player_note_offsets.cpp
        src/gameplay/player_note_offsets.h
        src/gameplay/chart_rc_calculator.cpp
        src/gameplay/chart_rc_calculator.h
        src/gameplay/performance_system.cpp
        src/gameplay/performance_system.h
        src/gameplay/play_mods.h
        src/services/ranking_service.cpp
        src/services/ranking_service.h
        src/services/online_content_availability.cpp
        src/services/online_content_availability.h
        src/services/content_authorization_service.cpp
        src/services/content_authorization_service.h
        src/services/content_sync_service.cpp
        src/services/content_sync_service.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/mv/mv_managed_storage.cpp
        src/mv/mv_managed_storage.h
        src/gameplay/scoring_ruleset_runtime.cpp
        src/gameplay/scoring_ruleset_runtime.h
        src/gameplay/score_system.cpp
        src/gameplay/score_system.h
        src/gameplay/song_loader.cpp
        src/gameplay/song_loader.h
        src/gameplay/song_fingerprint.cpp
        src/gameplay/song_fingerprint.h
        src/gameplay/song_writer.cpp
        src/gameplay/song_writer.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/network/auth_client.cpp
        src/network/auth_client.h
        src/network/friend_client.cpp
        src/network/friend_client.h
        src/network/friend_client_parser.cpp
        src/network/friend_client_parser.h
        src/network/http_client.cpp
        src/network/http_client.h
        src/network/websocket_client.cpp
        src/network/websocket_client.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/network/network_error.cpp
        src/network/network_error.h
        src/network/ranking_client.cpp
        src/network/ranking_client.h
        src/network/unlock_rule_client.cpp
        src/network/unlock_rule_client.h
        src/network/server_environment.cpp
        src/network/server_environment.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/core/local_catalog_signature.cpp
        src/core/local_catalog_signature.h
        src/core/local_sqlite.cpp
        src/core/local_sqlite.h
        src/core/file_dialog.cpp
        src/core/file_dialog.h
        src/core/window_dialog_support.cpp
        src/core/window_dialog_support.h
        src/core/scene.cpp
        src/core/scene.h
        src/core/scene_manager.cpp
        src/core/scene_manager.h
        src/core/load_progress.h
        src/core/uuid_util.h
        src/localization/localization.cpp
        src/localization/localization.h
        src/gameplay/game_settings.h
        src/gameplay/key_names.h
        src/gameplay/settings_io.cpp
        src/gameplay/settings_io.h
        src/scenes/scene_common.cpp
        src/scenes/scene_common.h
        src/scenes/shared/auth_overlay_controller.cpp
        src/scenes/shared/auth_overlay_controller.h
        src/scenes/shared/avatar_texture_cache.cpp
        src/scenes/shared/avatar_texture_cache.h
        src/scenes/shared/loading_screen_view.cpp
        src/scenes/shared/loading_screen_view.h
        src/scenes/shared/public_profile_controller.cpp
        src/scenes/shared/public_profile_controller.h
        src/scenes/shared/public_profile_effects.cpp
        src/scenes/shared/public_profile_effects.h
        src/scenes/shared/public_profile_service.cpp
        src/scenes/shared/public_profile_service.h
        src/scenes/shared/public_profile_state.cpp
        src/scenes/shared/public_profile_state.h
        src/scenes/shared/public_profile_view.cpp
        src/scenes/shared/public_profile_view.h
        src/scenes/shared/square_image_picker.cpp
        src/scenes/shared/square_image_picker.h
        src/scenes/shared/content_status_badge.h
        src/scenes/title_scene.cpp
        src/scenes/title_scene.h
        src/scenes/title/home_menu_view.cpp
        src/scenes/title/home_menu_view.h
        src/scenes/title/title_home_input_controller.cpp
        src/scenes/title/title_home_input_controller.h
        src/scenes/title/title_hub_view.cpp
        src/scenes/title/title_hub_view.h
        src/scenes/title/title_audio_controller.cpp
        src/scenes/title/title_audio_controller.h
        src/scenes/title/title_audio_policy.cpp
        src/scenes/title/title_audio_policy.h
        src/scenes/title/catalog_reload_policy.h
        src/scenes/title/catalog_reload_coordinator.cpp
        src/scenes/title/catalog_reload_coordinator.h
        src/scenes/title/title_layout.cpp
        src/scenes/title/title_layout.h
        src/scenes/title/play_session_controller.cpp
        src/scenes/title/play_session_controller.h
        src/scenes/title/profile_view.cpp
        src/scenes/title/profile_view.h
        src/scenes/title/title_create_mode_controller.cpp
        src/scenes/title/title_create_mode_controller.h
        src/scenes/title/title_browse_feature.cpp
        src/scenes/title/title_browse_feature.h
        src/scenes/title/title_common_update_controller.cpp
        src/scenes/title/title_common_update_controller.h
        src/scenes/title/title_frame_input_controller.cpp
        src/scenes/title/title_frame_input_controller.h
        src/scenes/title/title_friends_controller.cpp
        src/scenes/title/title_friends_controller.h
        src/scenes/title/title_friends_effects.cpp
        src/scenes/title/title_friends_effects.h
        src/scenes/title/title_friends_reducer.cpp
        src/scenes/title/title_friends_reducer.h
        src/scenes/title/title_friends_service.cpp
        src/scenes/title/title_friends_service.h
        src/scenes/title/title_friends_state.cpp
        src/scenes/title/title_friends_state.h
        src/scenes/title/title_friends_view.cpp
        src/scenes/title/title_friends_view.h
        src/scenes/title/title_online_mode_controller.cpp
        src/scenes/title/title_online_mode_controller.h
        src/scenes/title/title_mode_update_controller.cpp
        src/scenes/title/title_mode_update_controller.h
        src/scenes/title/title_multiplayer_audio_controller.cpp
        src/scenes/title/title_multiplayer_audio_controller.h
        src/scenes/title/title_multiplayer_content_resolver.cpp
        src/scenes/title/title_multiplayer_content_resolver.h
        src/scenes/title/title_multiplayer_flow_controller.cpp
        src/scenes/title/title_multiplayer_flow_controller.h
        src/scenes/title/title_play_create_feature.cpp
        src/scenes/title/title_play_create_feature.h
        src/scenes/title/title_play_data_controller.cpp
        src/scenes/title/title_play_data_controller.h
        src/scenes/title/title_selection_media_coordinator.cpp
        src/scenes/title/title_selection_media_coordinator.h
        src/scenes/title/title_play_mode_controller.cpp
        src/scenes/title/title_play_mode_controller.h
        src/scenes/title/title_startup_controller.cpp
        src/scenes/title/title_startup_controller.h
        src/scenes/title/title_shell_effects.cpp
        src/scenes/title/title_shell_effects.h
        src/scenes/title/title_play_transfer_controller.cpp
        src/scenes/title/title_play_transfer_controller.h
        src/scenes/title/title_profile_controller.cpp
        src/scenes/title/title_profile_controller.h
        src/scenes/title/title_header_view.cpp
        src/scenes/title/title_header_view.h
        src/scenes/title/title_local_song_select_controller.cpp
        src/scenes/title/title_local_song_select_controller.h
        src/scenes/title/create_tools_model.cpp
        src/scenes/title/create_tools_model.h
        src/scenes/title/create_tools_view.cpp
        src/scenes/title/create_tools_view.h
        src/scenes/title/create_unlock_rules_controller.cpp
        src/scenes/title/create_unlock_rules_controller.h
        src/scenes/title/create_upload_permissions.cpp
        src/scenes/title/create_upload_permissions.h
        src/scenes/title/create_upload_client.cpp
        src/scenes/title/create_upload_client.h
        src/scenes/title/online_catalog_data_controller.cpp
        src/scenes/title/online_catalog_data_controller.h
        src/scenes/title/online_catalog_ranking_loader.cpp
        src/scenes/title/online_catalog_ranking_loader.h
        src/scenes/title/local_content_database.cpp
        src/scenes/title/local_content_database.h
        src/scenes/title/local_content_binding.h
        src/scenes/title/local_content_index.cpp
        src/scenes/title/local_content_index.h
        src/scenes/title/online_download_internal.h
        src/scenes/title/online_download_catalog.cpp
        src/scenes/title/online_download_remote_client.cpp
        src/scenes/title/online_download_remote_client.h
        src/scenes/title/online_download_transfer.cpp
        src/scenes/title/online_download_update.cpp
        src/scenes/title/online_download_preview_controller.cpp
        src/scenes/title/online_download_preview_controller.h
        src/scenes/title/online_download_render.cpp
        src/scenes/title/online_download_layout.cpp
        src/scenes/title/online_download_state.cpp
        src/scenes/title/online_download_view.h
        src/scenes/title/ranking_panel_view.cpp
        src/scenes/title/ranking_panel_view.h
        src/scenes/title/center_panel_view.cpp
        src/scenes/title/center_panel_view.h
        src/scenes/title/song_list_view.cpp
        src/scenes/title/song_list_view.h
        src/scenes/title/title_bgm_controller.cpp
        src/scenes/title/title_bgm_controller.h
        src/scenes/title/title_command.h
        src/scenes/title/title_command_dispatcher.cpp
        src/scenes/title/title_command_dispatcher.h
        src/scenes/title/seamless_song_select_view.cpp
        src/scenes/title/seamless_song_select_view.h
        src/scenes/title/title_spectrum_visualizer.cpp
        src/scenes/title/title_spectrum_visualizer.h
        src/scenes/title/title_settings_overlay.cpp
        src/scenes/title/title_settings_overlay.h
        src/scenes/title/title_settings_flow_controller.cpp
        src/scenes/title/title_settings_flow_controller.h
        src/scenes/multiplayer/multiplayer_client.cpp
        src/scenes/multiplayer/multiplayer_client.h
        src/scenes/multiplayer/multiplayer_controller.cpp
        src/scenes/multiplayer/multiplayer_controller.h
        src/scenes/multiplayer/multiplayer_state.cpp
        src/scenes/multiplayer/multiplayer_state.h
        src/scenes/multiplayer/multiplayer_view.cpp
        src/scenes/multiplayer/multiplayer_view.h
        src/scenes/multiplayer_result_scene.cpp
        src/scenes/multiplayer_result_scene.h
        src/mv/composition/mv_composition.cpp
        src/mv/composition/mv_composition.h
        src/mv/composition/mv_component_registry.cpp
        src/mv/composition/mv_component_registry.h
        src/mv/composition/mv_lua_runtime.cpp
        src/mv/composition/mv_lua_runtime.h
        src/mv/composition/mv_composition_edit_history.cpp
        src/mv/composition/mv_composition_edit_history.h
        src/mv/composition/mv_composition_evaluator.cpp
        src/mv/composition/mv_composition_evaluator.h
        src/mv/composition/mv_composition_serializer.cpp
        src/mv/composition/mv_composition_serializer.h
        src/scenes/mv_editor_scene.cpp
        src/scenes/mv_editor_scene.h
         src/scenes/song_create_scene.cpp
         src/scenes/song_create_scene.h
         src/scenes/song_create/song_create_form_panel.cpp
         src/scenes/song_create/song_create_form_panel.h
         src/scenes/song_create/song_create_midi_importer.cpp
         src/scenes/song_create/song_create_midi_importer.h
         src/scenes/song_create/song_create_saved_view.cpp
         src/scenes/song_create/song_create_saved_view.h
         src/scenes/song_create/song_create_service.cpp
         src/scenes/song_create/song_create_service.h
         src/scenes/song_create/song_create_tag_editor.cpp
         src/scenes/song_create/song_create_tag_editor.h
         src/scenes/song_create/song_create_timing_controller.cpp
         src/scenes/song_create/song_create_timing_controller.h
         src/scenes/song_create/song_create_timing_panel.cpp
         src/scenes/song_create/song_create_timing_panel.h
         src/scenes/song_create/song_create_timing_service.cpp
         src/scenes/song_create/song_create_timing_service.h
         src/scenes/song_select/song_catalog_service.cpp
        src/scenes/song_select/song_catalog_service.h
        src/scenes/song_select/local_catalog_cache_service.cpp
        src/scenes/song_select/local_catalog_cache_service.h
        src/scenes/song_select/local_catalog_database.cpp
        src/scenes/song_select/local_catalog_database.h
        src/scenes/song_select/song_select_confirmation_dialog.cpp
        src/scenes/song_select/song_select_confirmation_dialog.h
        src/scenes/song_select/song_export_identity.cpp
        src/scenes/song_select/song_export_identity.h
        src/scenes/song_select/song_import_export_service.cpp
        src/scenes/song_select/song_import_export_service.h
        src/scenes/song_select/song_select_command_controller.cpp
        src/scenes/song_select/song_select_command_controller.h
        src/scenes/song_select/ranking_source_policy.cpp
        src/scenes/song_select/ranking_source_policy.h
        src/scenes/song_select/selection_key.cpp
        src/scenes/song_select/selection_key.h
        src/scenes/song_select/song_select_data_controller.cpp
        src/scenes/song_select/song_select_data_controller.h
        src/scenes/song_select/song_select_ranking_loader.cpp
        src/scenes/song_select/song_select_ranking_loader.h
        src/scenes/song_select/song_transfer_controller.cpp
        src/scenes/song_select/song_transfer_controller.h
        src/scenes/song_select/song_select_last_played.cpp
        src/scenes/song_select/song_select_last_played.h
        src/scenes/song_select/song_jacket_loader.cpp
        src/scenes/song_select/song_jacket_loader.h
        src/scenes/song_select/song_preview_audio_loader.cpp
        src/scenes/song_select/song_preview_audio_loader.h
        src/scenes/song_select/song_preview_controller.cpp
        src/scenes/song_select/song_preview_controller.h
        src/scenes/song_select/song_select_layout.h
        src/scenes/song_select/song_select_login_dialog.cpp
        src/scenes/song_select/song_select_login_dialog.h
        src/scenes/song_select/song_select_navigation.cpp
        src/scenes/song_select/song_select_navigation.h
        src/scenes/song_select/song_select_overlay_view.cpp
        src/scenes/song_select/song_select_overlay_view.h
        src/scenes/song_select/song_select_state.cpp
        src/scenes/song_select/song_select_state.h
        src/scenes/editor/controller/editor_runtime_controller.cpp
        src/scenes/editor/controller/editor_runtime_controller.h
        src/scenes/editor/controller/editor_screen_controller.cpp
        src/scenes/editor/controller/editor_screen_controller.h
        src/scenes/editor/controller/editor_timeline_screen_controller.cpp
        src/scenes/editor/controller/editor_timeline_screen_controller.h
        src/scenes/editor/controller/editor_timing_action_controller.cpp
        src/scenes/editor/controller/editor_timing_action_controller.h
        src/scenes/editor/daw/editor_daw_view.cpp
        src/scenes/editor/daw/editor_daw_view.h
        src/scenes/editor/daw/editor_daw_types.h
        src/scenes/editor/editor_flow_controller.cpp
        src/scenes/editor/editor_flow_controller.h
        src/scenes/editor/view/editor_cursor_hud_view.cpp
        src/scenes/editor/view/editor_cursor_hud_view.h
        src/scenes/editor/view/editor_layout.h
        src/scenes/editor/view/editor_modal_view.cpp
        src/scenes/editor/view/editor_modal_view.h
        src/scenes/editor/editor_panel_controller.cpp
        src/scenes/editor/editor_panel_controller.h
        src/scenes/editor/view/editor_screen_view.cpp
        src/scenes/editor/view/editor_screen_view.h
        src/scenes/editor/editor_scene_sync.cpp
        src/scenes/editor/editor_scene_sync.h
        src/scenes/editor/editor_scene_types.h
        src/scenes/editor/editor_session_loader.cpp
        src/scenes/editor/editor_session_loader.h
        src/scenes/editor/service/editor_chart_load_service.cpp
        src/scenes/editor/service/editor_chart_load_service.h
        src/scenes/editor/service/editor_chart_identity_service.cpp
        src/scenes/editor/service/editor_chart_identity_service.h
        src/scenes/editor/service/editor_metadata_service.cpp
        src/scenes/editor/service/editor_metadata_service.h
        src/scenes/editor/service/editor_note_edit_service.cpp
        src/scenes/editor/service/editor_note_edit_service.h
        src/scenes/editor/service/editor_note_placement_rules.cpp
        src/scenes/editor/service/editor_note_placement_rules.h
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
        src/scenes/editor/viewport/editor_timeline_viewport.cpp
        src/scenes/editor/viewport/editor_timeline_viewport.h
        src/scenes/editor_scene.cpp
        src/scenes/editor_scene.h
        src/scenes/play/play_flow_controller.cpp
        src/scenes/play/play_flow_controller.h
        src/scenes/play/play_hitsound_service.cpp
        src/scenes/play/play_hitsound_service.h
        src/scenes/play/play_chart_filter.cpp
        src/scenes/play/play_chart_filter.h
        src/scenes/play/play_note_draw_queue.cpp
        src/scenes/play/play_note_draw_queue.h
        src/scenes/play/play_mv_controller.cpp
        src/scenes/play/play_mv_controller.h
        src/scenes/play/play_renderer.cpp
        src/scenes/play/play_renderer.h
        src/scenes/play/play_view_geometry.cpp
        src/scenes/play/play_view_geometry.h
        src/scenes/play_scene.cpp
        src/scenes/play_scene.h
        src/scenes/play/play_session_loader.cpp
        src/scenes/play/play_session_loader.h
        src/scenes/play/play_session_types.h
        src/scenes/result/result_scene_view.cpp
        src/scenes/result/result_scene_view.h
        src/scenes/result_scene.cpp
        src/scenes/result_scene.h
        src/scenes/shared/scene_fade.cpp
        src/scenes/shared/scene_fade.h
        src/scenes/settings/settings_key_config_state.cpp
        src/scenes/settings/settings_key_config_state.h
        src/scenes/settings/settings_gameplay_preview.cpp
        src/scenes/settings/settings_gameplay_preview.h
        src/scenes/settings/settings_layout.h
        src/scenes/settings/settings_page_stack.cpp
        src/scenes/settings/settings_page_stack.h
        src/scenes/settings/settings_pages.cpp
        src/scenes/settings/settings_pages.h
        src/scenes/settings/settings_runtime_applier.cpp
        src/scenes/settings/settings_runtime_applier.h
        src/scenes/settings/settings_shell_view.h
        src/scenes/theme.h
        src/scenes/virtual_screen.cpp
        src/scenes/virtual_screen.h
        src/ui/ui_layout.h
        src/ui/ui_font.cpp
        src/ui/ui_font.h
        src/ui/ui_text.h
        src/ui/ui_hit.h
        src/ui/ui_draw.h
        src/ui/ui_inspector.h
        src/ui/ui_clip.cpp
        src/ui/ui_clip.h
        src/ui/ui_text_editor.cpp
        src/ui/ui_text_editor.h)

target_sources(raythm PRIVATE "${RAYTHM_GENERATED_ICON_CPP}" "${RAYTHM_GENERATED_ICON_H}")

add_executable(raythm_launcher
        src/launcher_main.cpp
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/updater/update_log.cpp
        src/updater/update_log.h
        src/updater/update_apply.cpp
        src/updater/update_apply.h
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
        src/updater/update_progress_window.cpp
        src/updater/update_progress_window.h
        src/updater/update_version.cpp
        src/updater/update_version.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/updater/update_workflow.cpp
        src/updater/update_workflow.h)

set_target_properties(raythm_launcher PROPERTIES OUTPUT_NAME "Launcher")
set_target_properties(raythm_updater PROPERTIES OUTPUT_NAME "Updater")

add_executable(font_atlas_generator
        src/font_atlas/font_atlas_generator.cpp)
add_custom_target(generate_ui_font_atlas
        COMMAND font_atlas_generator
                "${CMAKE_SOURCE_DIR}/assets/fonts/MPLUS1p-Medium.ttf"
                "${CMAKE_SOURCE_DIR}/assets/fonts/japanese_full.txt"
                "${CMAKE_SOURCE_DIR}/assets/fonts/ui_sdf"
        DEPENDS
                font_atlas_generator
                "${CMAKE_SOURCE_DIR}/assets/fonts/MPLUS1p-Medium.ttf"
                "${CMAKE_SOURCE_DIR}/assets/fonts/japanese_full.txt"
        VERBATIM)

if(WIN32)
    set_target_properties(raythm_launcher PROPERTIES WIN32_EXECUTABLE TRUE)
    set_target_properties(raythm_updater PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(raythm PRIVATE "${RAYTHM_APP_ICON_RC}")
    target_sources(raythm_launcher PRIVATE "${RAYTHM_APP_ICON_RC}")
    target_sources(raythm_updater PRIVATE "${RAYTHM_APP_ICON_RC}")
endif()

add_executable(chart_parser_smoke
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/tests/chart_parser_smoke.cpp
        src/models/data_models.h)

add_executable(chart_file_storage_smoke
        src/gameplay/chart_file_storage.cpp
        src/gameplay/chart_file_storage.h
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/gameplay/chart_serializer.cpp
        src/gameplay/chart_serializer.h
        src/tests/chart_file_storage_smoke.cpp
        src/models/data_models.h)

add_executable(chart_serializer_smoke
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/gameplay/chart_fingerprint.cpp
        src/gameplay/chart_fingerprint.h
        src/gameplay/chart_serializer.cpp
        src/gameplay/chart_serializer.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/tests/chart_serializer_smoke.cpp
        src/models/data_models.h)

add_executable(chart_difficulty_smoke
        src/models/data_models.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/tests/chart_difficulty_smoke.cpp)

add_executable(editor_state_smoke
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/service/editor_note_placement_rules.cpp
        src/scenes/editor/service/editor_note_placement_rules.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
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
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/local_sqlite.cpp
        src/core/local_sqlite.h
        src/core/path_utils.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/gameplay/song_fingerprint.cpp
        src/gameplay/song_fingerprint.h
        src/gameplay/song_loader.cpp
        src/gameplay/song_loader.h
        src/gameplay/song_writer.cpp
        src/gameplay/song_writer.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/tests/song_loader_smoke.cpp)

add_executable(song_select_state_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/scenes/song_select/song_select_layout.h
        src/scenes/shared/scene_fade.cpp
        src/scenes/shared/scene_fade.h
        src/scenes/song_select/song_select_state.cpp
        src/scenes/song_select/song_select_state.h
        src/tests/song_select_state_smoke.cpp)

add_executable(song_select_data_controller_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/scenes/shared/scene_fade.cpp
        src/scenes/shared/scene_fade.h
        src/scenes/song_select/ranking_source_policy.h
        src/scenes/song_select/selection_key.cpp
        src/scenes/song_select/selection_key.h
        src/scenes/song_select/song_select_data_controller.cpp
        src/scenes/song_select/song_select_data_controller.h
        src/scenes/song_select/song_select_ranking_loader.cpp
        src/scenes/song_select/song_select_ranking_loader.h
        src/scenes/title/online_catalog_ranking_loader.cpp
        src/scenes/title/online_catalog_ranking_loader.h
        src/scenes/song_select/song_select_layout.h
        src/scenes/song_select/song_select_state.cpp
        src/scenes/song_select/song_select_state.h
        src/tests/song_select_data_controller_smoke.cpp)

add_executable(title_startup_controller_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/localization/localization.cpp
        src/localization/localization.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/scenes/virtual_screen.cpp
        src/scenes/virtual_screen.h
        src/scenes/shared/loading_screen_view.cpp
        src/scenes/shared/loading_screen_view.h
        src/scenes/shared/scene_fade.cpp
        src/scenes/shared/scene_fade.h
        src/scenes/song_select/song_select_layout.h
        src/scenes/song_select/song_select_state.cpp
        src/scenes/song_select/song_select_state.h
        src/scenes/title/title_startup_controller.cpp
        src/scenes/title/title_startup_controller.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/ui/ui_font.cpp
        src/ui/ui_font.h
        src/tests/title_startup_controller_smoke.cpp)

add_executable(song_export_identity_smoke
        src/models/data_models.h
        src/models/online_content_identity.h
        src/scenes/song_select/song_export_identity.cpp
        src/scenes/song_select/song_export_identity.h
        src/tests/song_export_identity_smoke.cpp)

add_executable(online_content_identity_smoke
        src/models/content_lifecycle.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/tests/online_content_identity_smoke.cpp)

add_executable(online_content_availability_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/services/online_content_availability.cpp
        src/services/online_content_availability.h
        src/services/content_sync_service.cpp
        src/services/content_sync_service.h
        src/scenes/title/local_content_binding.h
        src/scenes/title/local_content_index.h
        src/scenes/song_select/song_select_state.cpp
        src/scenes/song_select/song_select_state.h
        src/services/content_sync_service.cpp
        src/services/content_sync_service.h
        src/scenes/shared/scene_fade.cpp
        src/scenes/shared/scene_fade.h
        src/tests/online_content_availability_smoke.cpp)

add_executable(create_upload_permissions_smoke
        src/scenes/title/create_upload_permissions.cpp
        src/scenes/title/create_upload_permissions.h
        src/scenes/title/local_content_binding.h
        src/scenes/title/local_content_index.h
        src/tests/create_upload_permissions_smoke.cpp)

add_executable(create_tools_model_smoke
        src/network/server_environment.cpp
        src/network/server_environment.h
        src/scenes/title/create_tools_model.cpp
        src/scenes/title/create_tools_model.h
        src/scenes/title/create_upload_permissions.cpp
        src/scenes/title/create_upload_permissions.h
        src/scenes/title/local_content_binding.h
        src/scenes/title/local_content_index.h
        src/scenes/song_select/song_select_state.h
        src/tests/create_tools_model_smoke.cpp)

add_executable(content_authorization_service_smoke
        src/services/content_authorization_service.cpp
        src/services/content_authorization_service.h
        src/tests/content_authorization_service_smoke.cpp)

add_executable(content_sync_service_smoke
        src/models/data_models.h
        src/services/content_sync_service.cpp
        src/services/content_sync_service.h
        src/tests/content_sync_service_smoke.cpp)

add_executable(content_cache_paths_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/tests/content_cache_paths_smoke.cpp)

add_executable(managed_content_storage_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/core/local_catalog_signature.cpp
        src/core/local_catalog_signature.h
        src/core/local_sqlite.cpp
        src/core/local_sqlite.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_file_storage.cpp
        src/gameplay/chart_file_storage.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/chart_level_memory_cache.cpp
        src/gameplay/chart_level_memory_cache.h
        src/gameplay/chart_parser.cpp
        src/gameplay/chart_parser.h
        src/gameplay/chart_serializer.cpp
        src/gameplay/chart_serializer.h
        src/gameplay/song_loader.cpp
        src/gameplay/song_loader.h
        src/gameplay/song_writer.cpp
        src/gameplay/song_writer.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/scenes/shared/scene_fade.cpp
        src/scenes/shared/scene_fade.h
        src/scenes/editor/service/editor_chart_load_service.cpp
        src/scenes/editor/service/editor_chart_load_service.h
        src/scenes/song_select/local_catalog_cache_service.cpp
        src/scenes/song_select/local_catalog_cache_service.h
        src/scenes/song_select/local_catalog_database.cpp
        src/scenes/song_select/local_catalog_database.h
        src/scenes/song_select/song_catalog_service.cpp
        src/scenes/song_select/song_catalog_service.h
        src/scenes/song_select/song_select_state.cpp
        src/scenes/song_select/song_select_state.h
        src/services/content_sync_service.cpp
        src/services/content_sync_service.h
        src/scenes/title/local_content_binding.h
        src/scenes/title/local_content_database.cpp
        src/scenes/title/local_content_database.h
        src/scenes/title/local_content_index.cpp
        src/scenes/title/local_content_index.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/tests/managed_content_storage_smoke.cpp)

add_executable(settings_io_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/localization/localization.cpp
        src/localization/localization.h
        src/gameplay/settings_io.cpp
        src/gameplay/settings_io.h
        src/network/server_environment.cpp
        src/network/server_environment.h
        src/tests/settings_io_smoke.cpp)

add_executable(localization_smoke
        src/localization/localization.cpp
        src/localization/localization.h
        src/tests/localization_smoke.cpp)

add_executable(local_catalog_database_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/core/local_catalog_signature.cpp
        src/core/local_catalog_signature.h
        src/core/local_sqlite.cpp
        src/core/local_sqlite.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/chart_level_memory_cache.cpp
        src/gameplay/chart_level_memory_cache.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/models/online_content_identity.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/scenes/song_select/local_catalog_database.cpp
        src/scenes/song_select/local_catalog_database.h
        src/scenes/song_select/local_catalog_cache_service.cpp
        src/scenes/song_select/local_catalog_cache_service.h
        src/scenes/song_select/song_select_state.cpp
        src/scenes/song_select/song_select_state.h
        src/gameplay/player_note_offsets.cpp
        src/gameplay/player_note_offsets.h
        src/services/content_sync_service.cpp
        src/services/content_sync_service.h
        src/scenes/shared/scene_fade.cpp
        src/scenes/shared/scene_fade.h
        src/scenes/title/local_content_binding.h
        src/scenes/title/local_content_database.cpp
        src/scenes/title/local_content_database.h
        src/scenes/title/local_content_index.cpp
        src/scenes/title/local_content_index.h
        src/models/data_models.h
        src/tests/local_catalog_database_smoke.cpp)

add_executable(local_content_index_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/local_catalog_signature.cpp
        src/core/local_catalog_signature.h
        src/core/local_sqlite.cpp
        src/core/local_sqlite.h
        src/scenes/title/local_content_binding.h
        src/scenes/title/local_content_database.cpp
        src/scenes/title/local_content_database.h
        src/scenes/title/local_content_index.cpp
        src/scenes/title/local_content_index.h
        src/tests/local_content_index_smoke.cpp)

add_executable(mv_composition_model_smoke
        src/mv/composition/mv_composition.cpp
        src/mv/composition/mv_composition.h
        src/mv/composition/mv_component_registry.cpp
        src/mv/composition/mv_component_registry.h
        src/mv/composition/mv_lua_runtime.cpp
        src/mv/composition/mv_lua_runtime.h
        src/mv/composition/mv_composition_evaluator.cpp
        src/mv/composition/mv_composition_evaluator.h
        src/mv/composition/mv_composition_serializer.cpp
        src/mv/composition/mv_composition_serializer.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/tests/mv_composition_model_smoke.cpp)

add_executable(mv_composition_edit_history_smoke
        src/mv/composition/mv_composition.cpp
        src/mv/composition/mv_composition.h
        src/mv/composition/mv_component_registry.cpp
        src/mv/composition/mv_component_registry.h
        src/mv/composition/mv_lua_runtime.cpp
        src/mv/composition/mv_lua_runtime.h
        src/mv/composition/mv_composition_edit_history.cpp
        src/mv/composition/mv_composition_edit_history.h
        src/mv/composition/mv_composition_serializer.cpp
        src/mv/composition/mv_composition_serializer.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/tests/mv_composition_edit_history_smoke.cpp)

add_executable(mv_storage_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/mv/composition/mv_composition.cpp
        src/mv/composition/mv_composition.h
        src/mv/composition/mv_component_registry.cpp
        src/mv/composition/mv_component_registry.h
        src/mv/composition/mv_lua_runtime.cpp
        src/mv/composition/mv_lua_runtime.h
        src/mv/composition/mv_composition_evaluator.cpp
        src/mv/composition/mv_composition_evaluator.h
        src/mv/composition/mv_composition_serializer.cpp
        src/mv/composition/mv_composition_serializer.h
        src/mv/mv_managed_storage.cpp
        src/mv/mv_managed_storage.h
        src/mv/mv_storage.cpp
        src/mv/mv_storage.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/tests/mv_storage_smoke.cpp)

add_executable(mv_managed_storage_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/models/data_models.h
        src/models/online_content_identity.h
        src/mv/composition/mv_composition.cpp
        src/mv/composition/mv_composition.h
        src/mv/composition/mv_component_registry.cpp
        src/mv/composition/mv_component_registry.h
        src/mv/composition/mv_lua_runtime.cpp
        src/mv/composition/mv_lua_runtime.h
        src/mv/composition/mv_composition_evaluator.cpp
        src/mv/composition/mv_composition_evaluator.h
        src/mv/composition/mv_composition_serializer.cpp
        src/mv/composition/mv_composition_serializer.h
        src/mv/mv_managed_storage.cpp
        src/mv/mv_managed_storage.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/tests/mv_managed_storage_smoke.cpp)

add_executable(input_handler_smoke
        src/platform/windows_input_source.cpp
        src/platform/windows_input_source.h
        src/gameplay/lane_input_tracker.cpp
        src/gameplay/lane_input_tracker.h
        src/gameplay/input_handler.cpp
        src/gameplay/input_handler.h
        src/tests/input_handler_smoke.cpp)

add_executable(lane_input_tracker_smoke
        src/models/data_models.h
        src/gameplay/lane_input_tracker.cpp
        src/gameplay/lane_input_tracker.h
        src/tests/lane_input_tracker_smoke.cpp)

add_executable(timing_engine_smoke
        src/models/data_models.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/tests/timing_engine_smoke.cpp)

add_executable(scene_manager_smoke
        src/core/scene.cpp
        src/core/scene.h
        src/core/scene_manager.cpp
        src/core/scene_manager.h
        src/tests/scene_manager_smoke.cpp)

add_executable(judge_system_smoke
        src/models/data_models.h
        src/platform/windows_input_source.cpp
        src/platform/windows_input_source.h
        src/gameplay/lane_input_tracker.cpp
        src/gameplay/lane_input_tracker.h
        src/gameplay/input_handler.cpp
        src/gameplay/input_handler.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/judge_system.cpp
        src/gameplay/judge_system.h
        src/tests/judge_system_smoke.cpp
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h)

add_executable(chart_judge_events_smoke
        src/models/data_models.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/tests/chart_judge_events_smoke.cpp)

add_executable(score_system_smoke
        src/models/data_models.h
        src/gameplay/scoring_ruleset_runtime.cpp
        src/gameplay/scoring_ruleset_runtime.h
        src/gameplay/score_system.cpp
        src/gameplay/score_system.h
        src/tests/score_system_smoke.cpp)

add_executable(performance_system_smoke
        src/models/data_models.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/chart_rc_calculator.cpp
        src/gameplay/chart_rc_calculator.h
        src/gameplay/performance_system.cpp
        src/gameplay/performance_system.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/tests/performance_system_smoke.cpp)

add_executable(chart_rc_calculator_smoke
        src/models/data_models.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/chart_rc_calculator.cpp
        src/gameplay/chart_rc_calculator.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/tests/chart_rc_calculator_smoke.cpp)

add_executable(ranking_service_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/core/local_catalog_signature.cpp
        src/core/local_catalog_signature.h
        src/core/local_sqlite.cpp
        src/core/local_sqlite.h
        src/scenes/title/local_content_database.cpp
        src/scenes/title/local_content_database.h
        src/scenes/title/local_content_index.cpp
        src/scenes/title/local_content_index.h
        src/models/data_models.h
        src/gameplay/chart_fingerprint.cpp
        src/gameplay/chart_fingerprint.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/services/ranking_service.cpp
        src/services/ranking_service.h
        src/gameplay/scoring_ruleset_runtime.cpp
        src/gameplay/scoring_ruleset_runtime.h
        src/gameplay/song_fingerprint.cpp
        src/gameplay/song_fingerprint.h
        src/network/auth_client.cpp
        src/network/auth_client.h
        src/network/http_client.cpp
        src/network/http_client.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/network/network_error.cpp
        src/network/network_error.h
        src/network/ranking_client.cpp
        src/network/ranking_client.h
        src/network/server_environment.cpp
        src/network/server_environment.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/tests/ranking_service_smoke.cpp)

add_executable(network_error_smoke
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/network/network_error.cpp
        src/network/network_error.h
        src/tests/network_error_smoke.cpp)

add_executable(json_helpers_smoke
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/tests/json_helpers_smoke.cpp)

add_executable(friend_client_parser_smoke
        src/network/friend_client_parser.cpp
        src/network/friend_client_parser.h
        src/network/friend_client.h
        src/network/http_client.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/network/ranking_client.h
        src/services/ranking_service.h
        src/models/data_models.h
        src/gameplay/scoring_ruleset_runtime.cpp
        src/gameplay/scoring_ruleset_runtime.h
        src/tests/friend_client_parser_smoke.cpp)

add_executable(friend_client_refresh_smoke
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/models/data_models.h
        src/network/auth_client.cpp
        src/network/auth_client.h
        src/network/friend_client.cpp
        src/network/friend_client.h
        src/network/friend_client_parser.cpp
        src/network/friend_client_parser.h
        src/network/http_client.cpp
        src/network/http_client.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/network/network_error.cpp
        src/network/network_error.h
        src/network/ranking_client.cpp
        src/network/ranking_client.h
        src/network/server_environment.cpp
        src/network/server_environment.h
        src/network/websocket_client.cpp
        src/network/websocket_client.h
        src/services/ranking_service.h
        src/gameplay/scoring_ruleset_runtime.cpp
        src/gameplay/scoring_ruleset_runtime.h
        src/tests/friend_client_refresh_smoke.cpp)

add_executable(title_friends_state_smoke
        src/network/friend_client.h
        src/scenes/title/title_friends_reducer.cpp
        src/scenes/title/title_friends_reducer.h
        src/scenes/title/title_friends_state.cpp
        src/scenes/title/title_friends_state.h
        src/tests/title_friends_state_smoke.cpp)

add_executable(public_profile_state_smoke
        src/network/auth_client.h
        src/scenes/shared/public_profile_state.cpp
        src/scenes/shared/public_profile_state.h
        src/tests/public_profile_state_smoke.cpp)

add_executable(multiplayer_state_smoke
        src/network/auth_client.h
        src/network/friend_client.h
        src/scenes/multiplayer/multiplayer_state.h
        src/tests/multiplayer_state_smoke.cpp)

add_executable(ranking_panel_view_smoke
        src/scenes/title/ranking_panel_view.h
        src/tests/ranking_panel_view_smoke.cpp)

add_executable(play_note_draw_queue_smoke
        src/models/data_models.h
        src/scenes/play/play_note_draw_queue.cpp
        src/scenes/play/play_note_draw_queue.h
        src/scenes/play/play_scroll_map.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
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
        src/gameplay/lane_input_tracker.cpp
        src/gameplay/lane_input_tracker.h
        src/gameplay/input_handler.cpp
        src/gameplay/input_handler.h
        src/gameplay/judge_system.cpp
        src/gameplay/judge_system.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/chart_rc_calculator.cpp
        src/gameplay/chart_rc_calculator.h
        src/gameplay/performance_system.cpp
        src/gameplay/performance_system.h
        src/gameplay/scoring_ruleset_runtime.cpp
        src/gameplay/scoring_ruleset_runtime.h
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

add_executable(play_hitsound_service_smoke
        src/audio/audio_loudness.cpp
        src/audio/audio_loudness.h
        src/audio/audio_manager.cpp
        src/audio/audio_manager.h
        src/platform/windows_input_source.cpp
        src/platform/windows_input_source.h
        src/models/data_models.h
        src/scenes/play/play_hitsound_service.cpp
        src/scenes/play/play_hitsound_service.h
        src/scenes/play/play_session_types.h
        src/scenes/play/play_scroll_map.h
        src/scenes/editor/editor_scene_types.h
        src/gameplay/input_handler.cpp
        src/gameplay/input_handler.h
        src/gameplay/lane_input_tracker.cpp
        src/gameplay/lane_input_tracker.h
        src/gameplay/judge_system.cpp
        src/gameplay/judge_system.h
        src/gameplay/performance_system.cpp
        src/gameplay/performance_system.h
        src/gameplay/score_system.cpp
        src/gameplay/score_system.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/chart_rc_calculator.cpp
        src/gameplay/chart_rc_calculator.h
        src/gameplay/scoring_ruleset_runtime.cpp
        src/gameplay/scoring_ruleset_runtime.h
        src/tests/play_hitsound_service_smoke.cpp)

add_executable(editor_transport_controller_smoke
        src/audio/audio_manager.h
        src/audio/audio_waveform.h
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_types.h
        src/scenes/editor/editor_timing_panel.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
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
        src/core/content_cache_paths.cpp
        src/core/content_cache_paths.h
        src/core/local_sqlite.cpp
        src/core/local_sqlite.h
        src/core/path_utils.h
        src/network/json_helpers.cpp
        src/network/json_helpers.h
        src/services/managed_content_storage.cpp
        src/services/managed_content_storage.h
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_types.h
        src/scenes/editor/editor_timing_panel.h
        src/gameplay/chart_serializer.cpp
        src/gameplay/chart_serializer.h
        src/gameplay/chart_fingerprint.cpp
        src/gameplay/chart_fingerprint.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/models/data_models.h
        src/scenes/editor/editor_flow_controller.cpp
        src/scenes/editor/editor_flow_controller.h
        src/scenes/editor/editor_scene_types.h
        src/updater/update_verify.cpp
        src/updater/update_verify.h
        src/tests/editor_flow_controller_smoke.cpp)

add_executable(editor_timeline_controller_smoke
        src/audio/audio_manager.h
        src/audio/audio_waveform.h
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_note_geometry.cpp
        src/scenes/editor/editor_timeline_note_geometry.h
        src/scenes/editor/editor_timeline_types.cpp
        src/scenes/editor/editor_timeline_types.h
        src/scenes/editor/editor_timing_panel.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/models/data_models.h
        src/scenes/editor/editor_scene_types.h
        src/scenes/editor/editor_timeline_controller.cpp
        src/scenes/editor/editor_timeline_controller.h
        src/ui/ui_clip.cpp
        src/ui/ui_clip.h
        src/ui/ui_font.cpp
        src/ui/ui_font.h
        src/localization/localization.cpp
        src/localization/localization.h
        src/core/app_paths.cpp
        src/core/app_paths.h
        src/scenes/virtual_screen.cpp
        src/scenes/virtual_screen.h
        src/tests/editor_timeline_controller_smoke.cpp)

add_executable(editor_note_edit_service_smoke
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_types.h
        src/scenes/editor/editor_timing_panel.h
        src/scenes/editor/service/editor_note_edit_service.cpp
        src/scenes/editor/service/editor_note_edit_service.h
        src/scenes/editor/service/editor_note_placement_rules.cpp
        src/scenes/editor/service/editor_note_placement_rules.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/models/data_models.h
        src/scenes/editor/editor_scene_types.h
        src/tests/editor_note_edit_service_smoke.cpp)

add_executable(editor_timing_edit_service_smoke
        src/scenes/editor/editor_command.h
        src/scenes/editor/editor_meter_map.cpp
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.cpp
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_types.h
        src/scenes/editor/editor_timing_panel.h
        src/scenes/editor/service/editor_note_placement_rules.cpp
        src/scenes/editor/service/editor_note_placement_rules.h
        src/scenes/editor/service/editor_timing_edit_service.cpp
        src/scenes/editor/service/editor_timing_edit_service.h
        src/gameplay/chart_difficulty.cpp
        src/gameplay/chart_difficulty.h
        src/gameplay/chart_judge_events.cpp
        src/gameplay/chart_judge_events.h
        src/gameplay/timing_engine.cpp
        src/gameplay/timing_engine.h
        src/models/data_models.h
        src/tests/editor_timing_edit_service_smoke.cpp)

add_executable(editor_panel_controller_smoke
        src/audio/audio_manager.h
        src/audio/audio_waveform.h
        src/scenes/editor/editor_meter_map.h
        src/scenes/editor/editor_state.h
        src/scenes/editor/editor_timeline_types.h
        src/scenes/editor/editor_timing_panel.h
        src/models/data_models.h
        src/scenes/editor/editor_panel_controller.cpp
        src/scenes/editor/editor_panel_controller.h
        src/scenes/editor/editor_scene_types.h
        src/tests/editor_panel_controller_smoke.cpp)

add_executable(audio_manager_smoke
        src/audio/audio.cpp
        src/audio/audio.h
        src/audio/audio_loudness.cpp
        src/audio/audio_loudness.h
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
        src/updater/update_log.cpp
        src/updater/update_log.h
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
        ${CMAKE_SOURCE_DIR}/src/gameplay
        ${CMAKE_SOURCE_DIR}/src/models
        ${CMAKE_SOURCE_DIR}/src/scenes
        ${CMAKE_SOURCE_DIR}/src/services
        ${CMAKE_SOURCE_DIR}/src/ui
)
set(RAYTHM_TEST_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/tests
)

set(RAYTHM_APP_TARGETS
        raythm
        raythm_launcher
        raythm_updater
)
set(RAYTHM_SMOKE_TARGETS
        chart_parser_smoke
        chart_file_storage_smoke
        chart_serializer_smoke
        chart_difficulty_smoke
        editor_state_smoke
        editor_meter_map_smoke
        song_loader_smoke
        song_select_state_smoke
        song_select_data_controller_smoke
        title_startup_controller_smoke
        song_export_identity_smoke
        online_content_identity_smoke
        online_content_availability_smoke
        create_upload_permissions_smoke
        create_tools_model_smoke
        content_authorization_service_smoke
        content_sync_service_smoke
        content_cache_paths_smoke
        managed_content_storage_smoke
        settings_io_smoke
        localization_smoke
        local_catalog_database_smoke
        local_content_index_smoke
        mv_composition_model_smoke
        mv_composition_edit_history_smoke
        mv_storage_smoke
        mv_managed_storage_smoke
        lane_input_tracker_smoke
        input_handler_smoke
        timing_engine_smoke
        scene_manager_smoke
        chart_judge_events_smoke
        judge_system_smoke
        score_system_smoke
        chart_rc_calculator_smoke
        performance_system_smoke
        ranking_service_smoke
        network_error_smoke
        json_helpers_smoke
        friend_client_parser_smoke
        friend_client_refresh_smoke
        title_friends_state_smoke
        public_profile_state_smoke
        multiplayer_state_smoke
        ranking_panel_view_smoke
        play_note_draw_queue_smoke
        play_chart_filter_smoke
        play_flow_controller_smoke
        play_hitsound_service_smoke
        editor_transport_controller_smoke
        editor_flow_controller_smoke
        editor_timeline_controller_smoke
        editor_note_edit_service_smoke
        editor_timing_edit_service_smoke
        editor_panel_controller_smoke
        audio_manager_smoke
        update_version_smoke
)

foreach(raythm_target IN LISTS RAYTHM_APP_TARGETS RAYTHM_SMOKE_TARGETS)
    target_include_directories(${raythm_target} PRIVATE ${RAYTHM_INCLUDE_DIRS})
    target_link_libraries(${raythm_target} PRIVATE nlohmann_json::nlohmann_json)
endforeach()
target_include_directories(raythm BEFORE PRIVATE "${RAYTHM_GENERATED_INCLUDE_DIR}")

foreach(raythm_target IN LISTS RAYTHM_SMOKE_TARGETS)
    target_include_directories(${raythm_target} PRIVATE ${RAYTHM_TEST_INCLUDE_DIRS})
endforeach()

if(BUILD_TESTING)
    foreach(raythm_target IN LISTS RAYTHM_SMOKE_TARGETS)
        if(WIN32 AND raythm_target STREQUAL "update_version_smoke")
            # CTest's direct spawn can hit Windows access-denied behavior for updater-named executables.
            add_test(NAME ${raythm_target}
                    COMMAND "${RAYTHM_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                    -Command "& '$<SHELL_PATH:$<TARGET_FILE:${raythm_target}>>'")
        else()
            add_test(NAME ${raythm_target} COMMAND ${raythm_target})
        endif()
    endforeach()
    set_tests_properties(audio_manager_smoke PROPERTIES SKIP_RETURN_CODE 77)
endif()

target_include_directories(raythm PRIVATE ${RAYTHM_BASS_DIR})
target_include_directories(audio_manager_smoke PRIVATE ${RAYTHM_BASS_DIR})
target_link_libraries(raythm PRIVATE raylib sqlite3 lua ${RAYTHM_BASS_IMPORT_LIB} comdlg32 crypt32 winhttp)
target_link_libraries(mv_composition_model_smoke PRIVATE lua)
target_link_libraries(mv_composition_edit_history_smoke PRIVATE lua)
target_link_libraries(mv_storage_smoke PRIVATE lua)
target_link_libraries(mv_managed_storage_smoke PRIVATE lua)
target_link_libraries(raythm_launcher PRIVATE winhttp)
target_link_libraries(raythm_updater PRIVATE winhttp)
target_link_libraries(font_atlas_generator PRIVATE raylib)
target_link_libraries(update_version_smoke PRIVATE winhttp)
target_link_libraries(friend_client_refresh_smoke PRIVATE winhttp ws2_32)
target_link_libraries(multiplayer_state_smoke PRIVATE raylib)
target_link_libraries(ranking_panel_view_smoke PRIVATE raylib)
target_link_libraries(song_loader_smoke PRIVATE sqlite3)
target_link_libraries(local_catalog_database_smoke PRIVATE sqlite3 raylib)
target_link_libraries(local_content_index_smoke PRIVATE sqlite3)
target_link_libraries(managed_content_storage_smoke PRIVATE sqlite3 raylib)
target_link_libraries(online_content_availability_smoke PRIVATE raylib)
target_link_libraries(song_select_state_smoke PRIVATE raylib)
target_link_libraries(song_select_data_controller_smoke PRIVATE raylib)
target_link_libraries(title_startup_controller_smoke PRIVATE raylib)
target_link_libraries(create_tools_model_smoke PRIVATE raylib)
target_link_libraries(settings_io_smoke PRIVATE raylib)
target_link_libraries(input_handler_smoke PRIVATE raylib)
target_link_libraries(judge_system_smoke PRIVATE raylib)
target_link_libraries(ranking_service_smoke PRIVATE sqlite3 crypt32 winhttp)
target_link_libraries(play_flow_controller_smoke PRIVATE raylib)
target_include_directories(play_hitsound_service_smoke PRIVATE ${RAYTHM_BASS_DIR})
target_link_libraries(play_hitsound_service_smoke PRIVATE raylib ${RAYTHM_BASS_IMPORT_LIB})
target_link_libraries(editor_transport_controller_smoke PRIVATE raylib)
target_link_libraries(editor_flow_controller_smoke PRIVATE raylib sqlite3)
target_link_libraries(editor_timeline_controller_smoke PRIVATE raylib)
target_link_libraries(editor_note_edit_service_smoke PRIVATE raylib)
target_link_libraries(editor_timing_edit_service_smoke PRIVATE raylib)
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

file(GLOB_RECURSE RAYTHM_ASSET_FILES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/assets/*")
set(RAYTHM_RUNTIME_FILES_STAMP
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/raythm_runtime_files.stamp")

add_custom_command(OUTPUT "${RAYTHM_RUNTIME_FILES_STAMP}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:raythm>
        $<TARGET_FILE_DIR:raythm>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${RAYTHM_RUNTIME_DLLS}
        $<TARGET_FILE_DIR:raythm>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:raythm>/assets
        COMMAND ${CMAKE_COMMAND} -E touch
        "${RAYTHM_RUNTIME_FILES_STAMP}"
        DEPENDS
        ${RAYTHM_RUNTIME_DLLS}
        ${RAYTHM_ASSET_FILES}
        COMMAND_EXPAND_LISTS
)

add_custom_target(raythm_runtime_files
        DEPENDS "${RAYTHM_RUNTIME_FILES_STAMP}"
)
add_dependencies(raythm raythm_runtime_files)

add_custom_command(TARGET audio_manager_smoke POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${RAYTHM_BASS_RUNTIME_DLL}
        $<TARGET_FILE_DIR:audio_manager_smoke>/bass.dll
)

add_custom_command(TARGET play_hitsound_service_smoke POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${RAYTHM_BASS_RUNTIME_DLL}
        $<TARGET_FILE_DIR:play_hitsound_service_smoke>/bass.dll
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
