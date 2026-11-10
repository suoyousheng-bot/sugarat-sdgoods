idf_component_register(
    SRCS "src/data_app.c" "src/data_model.c" "src/data_copy_zh.c" "src/data_copy_en.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl sugarat_watch
)
