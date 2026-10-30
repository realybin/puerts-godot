# SPDX-FileCopyrightText: Copyright (c) 2026 puerts-godot contributors
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.15)
project(PuertsTestAddon LANGUAGES CXX)
add_library(PuertsTestAddon SHARED counter.cpp)
target_compile_features(PuertsTestAddon PRIVATE cxx_std_14)
target_include_directories(PuertsTestAddon PRIVATE ../../thirdparty/puerts/unity/native/puerts/include)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/addon-path-$<CONFIG>.txt" CONTENT "$<TARGET_FILE:PuertsTestAddon>\n")
