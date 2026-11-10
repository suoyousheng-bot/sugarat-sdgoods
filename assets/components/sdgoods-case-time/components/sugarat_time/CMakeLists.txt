idf_component_register(
    SRCS "src/time_app.c" "src/time_model.c" "src/time_copy_zh.c" "src/time_copy_en.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl sugarat_watch
)
