# Repository build only: shared components this play uses. `python3 scripts/kit.py export-play` copies them
# into components/ and omits this file, producing the standalone project of the Release attachment.
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../shared/components/lvgl"
    "${CMAKE_CURRENT_LIST_DIR}/../../assets/components/sdgoods-wifi-ui/components/sugarat_wifi_ui")
