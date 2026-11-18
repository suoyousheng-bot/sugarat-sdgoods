file(GLOB SUGARAT_WATCH_FONTS "${CMAKE_CURRENT_LIST_DIR}/src/fonts/*.c")
idf_component_register(
    SRCS "src/sw_core.c" "src/sw_controls.c" "src/sw_display.c" "src/sw_icon_glyphs.c" ${SUGARAT_WATCH_FONTS}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES lvgl
)
