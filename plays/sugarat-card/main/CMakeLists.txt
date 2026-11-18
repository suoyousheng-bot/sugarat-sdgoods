idf_component_register(
    SRCS "main.c" "apps/apps_registry.c" "apps/ui_helloapp.c"
        "card/card_language.c" "card/card_data.c" "card/card_store.c" "card/card_crypto.c"
        "card/card_exchange.c" "card/card_network.c" "card/card_http.c" "card/card_view.c"
    INCLUDE_DIRS "." "apps" "card"
    REQUIRES sdgoods_board sdgoods_launcher sugarat_watch lvgl driver esp_timer heap log
        sugarat_portal_dns nvs_flash esp_wifi esp_event esp_netif esp_http_server json mbedtls
)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=sdgoods_launcher_return_to_launcher")
