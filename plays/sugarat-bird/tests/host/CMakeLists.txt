cmake_minimum_required(VERSION 3.16)
project(bird_host C)
get_filename_component(FIRMWARE "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
file(GLOB_RECURSE LVGL_SRC "${FIRMWARE}/components/lvgl/src/*.c")
file(GLOB_RECURSE GAME_SRC "${FIRMWARE}/components/sugarat_game/src/*.c")
add_executable(bird-host test.c ${LVGL_SRC} ${GAME_SRC}
    "${FIRMWARE}/main/apps/ui_helloapp.c" "${FIRMWARE}/main/bird/bird_model.c"
    "${FIRMWARE}/main/bird/bird_storage.c" "${FIRMWARE}/main/bird/bird_view.c")
target_include_directories(bird-host PRIVATE . "${FIRMWARE}/components/lvgl" "${FIRMWARE}/main"
    "${FIRMWARE}/main/apps" "${FIRMWARE}/components/sugarat_game/include")
target_compile_definitions(bird-host PRIVATE LV_CONF_INCLUDE_SIMPLE _POSIX_C_SOURCE=200809L)
target_compile_options(bird-host PRIVATE -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(bird-host PRIVATE -fsanitize=address,undefined)
target_link_libraries(bird-host PRIVATE m)
