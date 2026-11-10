# Modified for Sugarat Radio by Deotaland, 2026.
idf_component_register(
    SRCS
        "main.c"
        "wifi_prov.c"
        "radio_region.c"
        "radio_player.c"
        "radio_catalog.c"
        "radio_station_identity.c"
        "radio_settings.c"
        "apps/apps_registry.c"
        "apps/ui_radio_kit.c"
        "apps/ui_radio_pages.c"
        "apps/ui_radio.c"
        "apps/ui_radio_skin.c"
        "apps/ui_radio_assets.c"
        "apps/ui_radio_redraw_assets.c"
        "apps/ui_radio_title_font.c"
    EMBED_TXTFILES "stations-us.json"
    INCLUDE_DIRS "apps" "."
    REQUIRES sdgoods_board sdgoods_launcher driver esp_event esp_http_client
        esp_audio_codec json sugarat_wifi sugarat_wifi_ui esp_netif esp_timer
        esp_wifi freertos hal heap log lvgl lwip nvs_flash
)

# ---- 自动编译版本号 (MMDDHHMM)，每次编译刷新 ----
# 生成 build_version.h 到组件构建目录，并让组件库在编译前依赖它，
# 同时把构建目录加入头文件搜索路径，使 apps/ui_home.c 能 #include "build_version.h"。
set(_bv_dir ${CMAKE_CURRENT_BINARY_DIR})
add_custom_target(gen_build_version ALL
    COMMAND ${CMAKE_COMMAND}
        -D BIN_DIR=${_bv_dir}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/gen_build_version.cmake
    BYPRODUCTS ${_bv_dir}/build_version.h
    COMMENT "Generating build version stamp (MMDDHHMM)"
)
add_dependencies(${COMPONENT_LIB} gen_build_version)
target_include_directories(${COMPONENT_LIB} PRIVATE ${_bv_dir})

# ---- 应用第三方组件补丁（LVGL 的 GIF canvas 走 PSRAM 等）----
# managed_components/ 不入库、由组件管理器下载，所以补丁放在 main/patches/ 随源码提交，
# 每次 configure 幂等地覆盖过去 —— 保证「clone 后直接编译」得到与发布版一致的固件。
# 详见 main/patches/README.md。该步失败会让 configure 直接报错（有意为之）。
idf_build_get_property(_patch_python PYTHON)
execute_process(
    COMMAND ${_patch_python} ${CMAKE_CURRENT_SOURCE_DIR}/patches/apply_lvgl_patches.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    RESULT_VARIABLE _lvgl_patch_rc
    OUTPUT_VARIABLE _lvgl_patch_out
    ERROR_VARIABLE  _lvgl_patch_err
)
if(_lvgl_patch_out)
    message(STATUS "${_lvgl_patch_out}")
endif()
if(NOT _lvgl_patch_rc EQUAL 0)
    message(FATAL_ERROR "LVGL 补丁应用失败: ${_lvgl_patch_err}")
endif()
