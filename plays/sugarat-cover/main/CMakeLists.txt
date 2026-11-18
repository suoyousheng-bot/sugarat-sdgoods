# One relocatable application image includes the local management site and default cover.
set(COVER_SOURCES cover/cv_copy.c cover/cv_crypto.c cover/cv_device.c cover/cv_gesture.c
    cover/cv_http.c cover/cv_jpeg.c cover/cv_link.c cover/cv_store.c cover/cv_view.c)
idf_component_register(
    SRCS "main.c" "apps/apps_registry.c" "apps/ui_helloapp.c" ${COVER_SOURCES}
    INCLUDE_DIRS "." "apps" "cover"
    REQUIRES sugarat_watch sdgoods_board sdgoods_launcher sugarat_portal_dns esp_wifi esp_netif esp_http_server
             esp_event esp_timer nvs_flash freertos mbedtls json driver
    EMBED_FILES "../assets/sample.jpg" "web/index.html" "web/app.js" "web/style.css" "web/font.woff2"
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wno-misleading-indentation)
