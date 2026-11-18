idf_component_register(
    SRCS "main.c" "apps/apps_registry.c" "apps/ui_helloapp.c"
        "pet/pet_model.c" "pet/pet_character.c" "pet/pet_save.c" "pet/pet_storage.c" "pet/pet_view.c"
        "pet/pet_family.c" "pet/pet_family_record.c" "pet/pet_family_store.c" "pet/pet_family_nvs.c"
        "pet/pet_crypto.c" "pet/pet_social.c" "pet/pet_radio.c" "pet/pet_animation.c" "pet/pet_art.c" "pet/pet_controls.c" "pet/pet_gestures.c"
    INCLUDE_DIRS "." "apps" "pet"
    REQUIRES sdgoods_board sdgoods_launcher sugarat_watch epet_core driver esp_timer heap log lvgl nvs_flash esp_wifi esp_event mbedtls
)
# Both SDK and power-key routes flush the application before a launcher restart.
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=sdgoods_launcher_return_to_launcher")
