file(GLOB SUGARAT_GAME_FONTS "${CMAKE_CURRENT_LIST_DIR}/src/fonts/*.c")
idf_component_register(
    SRCS "src/sg_core.c" "src/sg_controls.c" "src/sg_display.c" "src/sg_game.c" "src/sg_icon_glyphs.c" ${SUGARAT_GAME_FONTS}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES lvgl
)
