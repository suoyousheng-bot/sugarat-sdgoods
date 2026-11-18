cmake_minimum_required(VERSION 3.16)
project(sugarat_beat_host C)
get_filename_component(FIRMWARE "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
file(GLOB_RECURSE LVGL "${FIRMWARE}/components/lvgl/src/*.c")
file(GLOB_RECURSE GAME "${FIRMWARE}/components/sugarat_game/src/*.c")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include("${FIRMWARE}/main/beat_assets.cmake")
set(beat_backdrop "${CMAKE_CURRENT_BINARY_DIR}/beat_backdrop.c")
beat_generate_asset("${beat_backdrop}" "${Python3_EXECUTABLE}" backdrop)
add_executable(beat-host test.c ${LVGL} ${GAME} "${beat_backdrop}"
    "${FIRMWARE}/main/apps/beat_view.c"
    "${FIRMWARE}/components/sdgoods_board/src/sdgoods_tap.c"
    "${FIRMWARE}/main/apps/beat_level.c" "${FIRMWARE}/main/apps/ui_helloapp.c")
target_include_directories(beat-host PRIVATE .
    "${FIRMWARE}/components/lvgl" "${FIRMWARE}/components/sugarat_game/include"
    "${FIRMWARE}/components/sdgoods_board/include" "${FIRMWARE}/main/apps")
target_compile_definitions(beat-host PRIVATE LV_CONF_INCLUDE_SIMPLE)
target_compile_options(beat-host PRIVATE -g -O1 -fsanitize=address,undefined)
target_link_options(beat-host PRIVATE -fsanitize=address,undefined)
target_link_libraries(beat-host PRIVATE m)
