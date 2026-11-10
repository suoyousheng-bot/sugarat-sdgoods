idf_component_register(
    SRCS "src/media_app.c" "src/media_transport.c" "src/media_copy_zh.c" "src/media_copy_en.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl sugarat_watch
)
