idf_component_register(
    SRCS "src/control_app.c" "src/control_copy_zh.c" "src/control_copy_en.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl sugarat_watch
)
