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
        src/ui/button.cpp
        src/ui/button.h
        src/ui/ui_element.h
        src/ui/ui_render_queue.cpp
        src/ui/ui_render_queue.h
        src/ui/ui_renderer.cpp
        src/ui/ui_renderer.h)

# BASSライブラリ
target_include_directories(raythm PRIVATE ${CMAKE_SOURCE_DIR}/libs/bass)
target_link_libraries(raythm PRIVATE raylib ${CMAKE_SOURCE_DIR}/libs/bass/bass.lib)

# bass.dll を実行ファイルと同じディレクトリにコピー
add_custom_command(TARGET raythm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/libs/bass/bass.dll
        $<TARGET_FILE_DIR:raythm>/bass.dll
)
