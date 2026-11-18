# Copyright (c) 2026 SDGOODS. SPDX-License-Identifier: Apache-2.0
# Modified by Sugarat: a named, version-pinned product application.
cmake_minimum_required(VERSION 3.16)
include("${CMAKE_CURRENT_LIST_DIR}/kit-components.cmake" OPTIONAL)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "0.2.1")
project(sugarat_race)
