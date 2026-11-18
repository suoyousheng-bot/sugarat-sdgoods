cmake_minimum_required(VERSION 3.16)
project(squad_host C)
get_filename_component(FIRMWARE "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
file(GLOB_RECURSE LVGL_SRC "${FIRMWARE}/components/lvgl/src/*.c")
file(GLOB_RECURSE GAME_SRC "${FIRMWARE}/components/sugarat_game/src/*.c")
file(GLOB_RECURSE PIXEL_SRC "${FIRMWARE}/components/sugarat_pixel/src/*.c")
add_executable(squad-host test.c ${LVGL_SRC} ${GAME_SRC} ${PIXEL_SRC}
    "${FIRMWARE}/main/apps/ui_helloapp.c" "${FIRMWARE}/main/talk/talk_core.c" "${FIRMWARE}/main/talk/talk_view.c")
target_include_directories(squad-host PRIVATE . "${FIRMWARE}/main/apps" "${FIRMWARE}/components/lvgl" "${FIRMWARE}/main"
    "${FIRMWARE}/components/sugarat_game/include" "${FIRMWARE}/components/sugarat_pixel/include")
target_compile_definitions(squad-host PRIVATE LV_CONF_INCLUDE_SIMPLE _POSIX_C_SOURCE=200809L)
target_compile_options(squad-host PRIVATE -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(squad-host PRIVATE -fsanitize=address,undefined)
target_link_libraries(squad-host PRIVATE m)
