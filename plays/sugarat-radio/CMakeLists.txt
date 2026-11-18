# SPDX-License-Identifier: Apache-2.0
# Modified for Sugarat Radio by Deotaland, 2026.
cmake_minimum_required(VERSION 3.16)
set(IDF_TARGET esp32s3 CACHE STRING "Sugarat round badge target")
include("${CMAKE_CURRENT_LIST_DIR}/kit-components.cmake" OPTIONAL)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sugarat_radio VERSION 0.1.1)
