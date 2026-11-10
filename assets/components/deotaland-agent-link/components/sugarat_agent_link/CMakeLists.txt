idf_component_register(
    SRCS "src/agent_cloud.c" "src/agent_voice.c" "src/agent_uplink.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    PRIV_REQUIRES sugarat_https sugarat_audio json nvs_flash esp_wifi esp_timer esp_app_format mbedtls
                  esp_netif espressif__esp_websocket_client espressif__esp_audio_codec
)
