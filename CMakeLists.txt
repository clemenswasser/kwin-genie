# SPDX-FileCopyrightText: 2026 Clemens Wasser
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.22)
project(kwin-effect-genie VERSION 1.0.0 LANGUAGES C CXX)

set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.10.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui DBus Widgets Quick)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons Config)
find_package(KWin 6.8 REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)

option(BUILD_TESTING "Build the test client" ON)
if(BUILD_TESTING)
    enable_testing()

    add_executable(genie_test_client tests/client.cpp)
    target_link_libraries(genie_test_client Qt6::Widgets)

    add_executable(genie_mesh_test tests/mesh_test.c src/genie.c)
    target_include_directories(genie_mesh_test PRIVATE src)
    target_compile_options(genie_mesh_test PRIVATE -ffp-contract=off)
    target_link_libraries(genie_mesh_test PRIVATE m)
    add_test(NAME genie_mesh COMMAND genie_mesh_test)

    add_executable(genie_shadow_test tests/shadow_test.c src/genie.c)
    target_include_directories(genie_shadow_test PRIVATE src)
    target_compile_options(genie_shadow_test PRIVATE -ffp-contract=off)
    target_link_libraries(genie_shadow_test PRIVATE m)
    add_test(NAME genie_shadow
        COMMAND genie_shadow_test ${CMAKE_CURRENT_SOURCE_DIR}/tests/shadow_frames.txt)

    add_executable(genie_timeline_test tests/timeline_test.cpp)
    target_link_libraries(genie_timeline_test PRIVATE KWin::kwin Qt6::Core)
    add_test(NAME genie_timeline COMMAND genie_timeline_test)

    add_test(NAME genie_target_matrix
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/target_matrix.sh)
    set_tests_properties(genie_target_matrix PROPERTIES TIMEOUT 300)

    add_test(NAME genie_direction_matrix
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/direction_matrix.sh)
    set_tests_properties(genie_direction_matrix PROPERTIES TIMEOUT 300)
endif()
