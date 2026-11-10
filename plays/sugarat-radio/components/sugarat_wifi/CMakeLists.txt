idf_component_register(SRCS "sugarat_wifi.c" "sugarat_wifi_dns.c" INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event esp_timer esp_http_server nvs_flash json
    PRIV_REQUIRES lwip
    EMBED_TXTFILES "portal.html")
