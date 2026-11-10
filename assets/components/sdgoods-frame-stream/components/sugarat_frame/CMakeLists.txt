idf_component_register(
    SRCS "src/sugarat_frame.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer freertos heap log lvgl sdgoods_board
)
