# Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)
include("${CMAKE_CURRENT_LIST_DIR}/kit-components.cmake" OPTIONAL)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "0.1.4")
if(COVER_UI_DIAGNOSTICS)
    list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/tests/device")
endif()
project(sugarat_cover)
