idf_component_register(
    SRCS "src/sugarat_battery.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_usb_serial_jtag esp_timer freertos log sdgoods_board
)
