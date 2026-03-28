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
        src/audio.cpp
        src/audio.h
        src/chart_parser.cpp
        src/chart_parser.h
        src/data_models.h
        src/input_handler.cpp
        src/input_handler.h
        src/judge_system.cpp
        src/judge_system.h
        src/song_loader.cpp
        src/song_loader.h
        src/timing_engine.cpp
        src/timing_engine.h
        src/scene.cpp
        src/scene.h
        src/scene_manager.cpp
        src/scene_manager.h
        src/game_scenes.cpp
        src/game_scenes.h)

add_executable(chart_parser_smoke
        src/chart_parser.cpp
        src/chart_parser.h
        src/chart_parser_smoke.cpp
        src/data_models.h)

add_executable(song_loader_smoke
        src/chart_parser.cpp
        src/chart_parser.h
        src/data_models.h
        src/song_loader.cpp
        src/song_loader.h
        src/song_loader_smoke.cpp)

add_executable(input_handler_smoke
        src/input_handler.cpp
        src/input_handler.h
        src/input_handler_smoke.cpp)

add_executable(timing_engine_smoke
        src/data_models.h
        src/timing_engine.cpp
        src/timing_engine.h
        src/timing_engine_smoke.cpp)

add_executable(judge_system_smoke
        src/data_models.h
        src/input_handler.cpp
        src/input_handler.h
        src/judge_system.cpp
        src/judge_system.h
        src/judge_system_smoke.cpp
        src/timing_engine.cpp
        src/timing_engine.h)

# BASSライブラリ
target_include_directories(raythm PRIVATE ${CMAKE_SOURCE_DIR}/libs/bass)
target_link_libraries(raythm PRIVATE raylib ${CMAKE_SOURCE_DIR}/libs/bass/bass.lib)
target_link_libraries(input_handler_smoke PRIVATE raylib)
target_link_libraries(judge_system_smoke PRIVATE raylib)

# bass.dll を実行ファイルと同じディレクトリにコピー
add_custom_command(TARGET raythm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/libs/bass/bass.dll
        $<TARGET_FILE_DIR:raythm>/bass.dll
)
