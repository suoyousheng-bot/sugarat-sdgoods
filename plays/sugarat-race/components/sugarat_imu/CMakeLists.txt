idf_component_register(
    SRCS "src/sugarat_imu.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer freertos log sdgoods_board
)
