idf_component_register(
    SRCS "src/companion_app.c" "src/companion_model.c" "src/companion_copy_zh.c" "src/companion_copy_en.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl sugarat_watch sugarat_pixel
)
