idf_component_register(
    SRCS "src/sugarat_audio.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2s esp_timer freertos heap log sdgoods_board
)
