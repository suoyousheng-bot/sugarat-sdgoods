# main/ —— 应用层（二次开发主要在这里）
#
# 平台层（屏 / 触摸 / 音频 / LVGL 移植 / 应用框架 / 字体）是独立组件：
#   components/sdgoods_board/
# 应用层通过 REQUIRES sdgoods_board 使用它，一行 #include "sdgoods_board.h" 即可。

idf_component_register(
    SRCS
        "main.c"

        # --- 应用层小工具（不注册成「应用」，只被 main.c / 页面调用）---
        # app_data_store：持久化数据的参考实现（appdata 优先，拿不到退 NVS）
        "app_data_store.c"

        # --- 应用注册表（新增应用要在这里接线） ---
        "apps/apps_registry.c"

        # --- 应用骨架（开发者二次开发模板，始终编译以便参考） ---
        "apps/app_template.c"

        # --- helloapp 应用（本工程唯一应用：开机直入） ---
        "apps/ui_helloapp.c"

        # --- lz 实时 3D：渲染器与导出的模型数据（tools/export_model.py 生成）---
        "render/lz3d.c"
        "model/lz_model_data.c"

        # --- 开机动画解码（数据由 tools/encode_bootanim.py 生成）---
        "bootanim/bootanim.c"

    INCLUDE_DIRS
        "apps"                  # 让 #include "ui_xxx.h" 直接可用
        "render"
        "model"
        "bootanim"
        "."                     # 让 #include "app_data_store.h" 直接可用

    EMBED_FILES
        "bootanim/bootanim.bin"

    REQUIRES
        sdgoods_board
        sdgoods_launcher
        driver
        esp_event
        esp_timer
        freertos
        hal
        heap
        log
        lvgl
        nvs_flash
        sugarat_frame
)

# 渲染热路径：-O3，扫描线内循环放进 IRAM（不受 PSRAM/flash 共享 cache 抖动影响）。
set_source_files_properties(render/lz3d.c PROPERTIES COMPILE_OPTIONS "-O3")
set_source_files_properties(apps/ui_helloapp.c bootanim/bootanim.c PROPERTIES COMPILE_OPTIONS "-O2")
target_compile_definitions(${COMPONENT_LIB} PRIVATE "LZ3D_HOT=__attribute__((section(\".iram1.lz3d\")))")

# ---- 自动编译版本号 (MMDDHHMM)，每次编译刷新 ----
# 生成 build_version.h 到组件构建目录，并让组件库在编译前依赖它，
# 同时把构建目录加入头文件搜索路径，使 main/apps/*.c 能 #include "build_version.h"。
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
