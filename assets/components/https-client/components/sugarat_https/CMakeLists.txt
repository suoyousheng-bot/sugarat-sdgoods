idf_component_register(
    SRCS "src/sugarat_https.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp-tls mbedtls freertos heap log
)
