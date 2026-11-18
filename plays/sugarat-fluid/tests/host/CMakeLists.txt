cmake_minimum_required(VERSION 3.16)
project(fluid_host C)
get_filename_component(FIRMWARE "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(LVGL "${FIRMWARE}/components/lvgl")
file(GLOB_RECURSE LVGL_SRC "${LVGL}/src/*.c")
file(GLOB_RECURSE WATCH_SRC "${FIRMWARE}/components/sugarat_watch/src/*.c")
add_executable(fluid-host preview.c ${LVGL_SRC} ${WATCH_SRC}
    "${FIRMWARE}/main/fluid/sim.c" "${FIRMWARE}/main/fluid/render.c" "${FIRMWARE}/main/fluid/liquid.c" "${FIRMWARE}/main/fluid/link.c"
    "${FIRMWARE}/main/apps/bottle_view.c")
target_include_directories(fluid-host PRIVATE . "${LVGL}" "${FIRMWARE}/main"
    "${FIRMWARE}/main/apps" "${FIRMWARE}/main/fluid"
    "${FIRMWARE}/components/sugarat_watch/include")
target_compile_definitions(fluid-host PRIVATE LV_CONF_INCLUDE_SIMPLE _POSIX_C_SOURCE=200809L)
target_compile_options(fluid-host PRIVATE -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(fluid-host PRIVATE -fsanitize=address,undefined)
target_link_libraries(fluid-host PRIVATE m)

add_executable(fluid-frames frames_test.c)
target_include_directories(fluid-frames PRIVATE "${FIRMWARE}/main" "${FIRMWARE}/components/sugarat_frame/include")
target_compile_options(fluid-frames PRIVATE -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(fluid-frames PRIVATE -fsanitize=address,undefined)

add_executable(fluid-link link_test.c "${FIRMWARE}/main/fluid/liquid.c" "${FIRMWARE}/main/fluid/link.c")
target_include_directories(fluid-link PRIVATE "${FIRMWARE}/main")
target_compile_options(fluid-link PRIVATE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(fluid-link PRIVATE -fsanitize=address,undefined)
target_link_libraries(fluid-link PRIVATE m)
