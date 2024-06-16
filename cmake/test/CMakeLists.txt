# This cmake build script is meant for verifying the various CMake configuration scripts.

cmake_minimum_required(VERSION 3.12)
project(sdl_test LANGUAGES C)

message(STATUS "CMAKE_SYSTEM_NAME=      ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR= ${CMAKE_SYSTEM_PROCESSOR}")

if(WINDOWS_STORE)
    enable_language(CXX)
    add_compile_options(/ZW)
    set_source_files_properties(ain_cli.c main_gui.c PROPERTIES LANGUAGE CXX)
endif()

include(GenerateExportHeader)

if(ANDROID)
    macro(add_executable NAME)
        set(args ${ARGN})
        list(REMOVE_ITEM args WIN32)
        add_library(${NAME} SHARED ${args})
        unset(args)
    endmacro()
endif()

cmake_policy(SET CMP0074 NEW)

# Override CMAKE_FIND_ROOT_PATH_MODE to allow search for SDL3 outside of sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)

include(FeatureSummary)

option(TEST_SHARED "Test linking to shared SDL3 library" ON)
add_feature_info("TEST_SHARED" TEST_SHARED "Test linking with shared library")

option(TEST_STATIC "Test linking to static SDL3 library" ON)
add_feature_info("TEST_STATIC" TEST_STATIC "Test linking with static library")

option(TEST_TEST "Test linking to SDL3_test library" ON)
add_feature_info("TEST_TEST" TEST_STATIC "Test linking to SDL test library")

option(TEST_FULL "Run complete SDL test suite" OFF)
add_feature_info("TEST_FULL" TEST_FULL "Build full SDL testsuite")

find_package(SDL3 REQUIRED CONFIG COMPONENTS Headers)
add_library(headers_test_slash OBJECT inc_sdl_slash.c)
target_link_libraries(headers_test_slash PRIVATE SDL3::Headers)

if(TEST_SHARED)
    find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-shared)
    add_executable(gui-shared WIN32 main_gui.c)
    target_link_libraries(gui-shared PRIVATE SDL3::SDL3-shared)
    if(WIN32)
        add_custom_command(TARGET gui-shared POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:gui-shared>"
        )
    endif()

    add_library(sharedlib-shared SHARED main_lib.c)
    target_link_libraries(sharedlib-shared PRIVATE SDL3::SDL3-shared)
    generate_export_header(sharedlib-shared EXPORT_MACRO_NAME MYLIBRARY_EXPORT)
    target_compile_definitions(sharedlib-shared PRIVATE "EXPORT_HEADER=\"${CMAKE_CURRENT_BINARY_DIR}/sharedlib-shared_export.h\"")
    set_target_properties(sharedlib-shared PROPERTIES C_VISIBILITY_PRESET "hidden")

    add_executable(cli-shared main_cli.c)
    target_link_libraries(cli-shared PRIVATE SDL3::SDL3-shared)
    if(WIN32)
        add_custom_command(TARGET cli-shared POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SDL3::SDL3-shared>" "$<TARGET_FILE_DIR:cli-shared>"
        )
    endif()

    if(TEST_TEST)
        add_executable(sdltest-shared sdltest.c)
        target_link_libraries(sdltest-shared PRIVATE SDL3::SDL3_test SDL3::SDL3-shared)
    endif()
endif()

if(TEST_STATIC)
    find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-static)
    add_executable(gui-static WIN32 main_gui.c)
    target_link_libraries(gui-static PRIVATE SDL3::SDL3-static)

    # Assume SDL library has been built with `set(CMAKE_POSITION_INDEPENDENT_CODE ON)`
    add_library(sharedlib-static SHARED main_lib.c)
    target_link_libraries(sharedlib-static PRIVATE SDL3::SDL3-static)
    generate_export_header(sharedlib-static EXPORT_MACRO_NAME MYLIBRARY_EXPORT)
    target_compile_definitions(sharedlib-static PRIVATE "EXPORT_HEADER=\"${CMAKE_CURRENT_BINARY_DIR}/sharedlib-static_export.h\"")
    set_target_properties(sharedlib-static PROPERTIES C_VISIBILITY_PRESET "hidden")

    if(TEST_TEST)
        add_executable(sdltest-static sdltest.c)
        target_link_libraries(sdltest-static PRIVATE SDL3::SDL3_test SDL3::SDL3-static)
    endif()
endif()

find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
add_executable(gui-whatever WIN32 main_gui.c)
target_link_libraries(gui-whatever PRIVATE SDL3::SDL3)

if(TEST_FULL)
    enable_testing()
    set(SDL_TESTS_TIMEOUT_MULTIPLIER "1" CACHE STRING "Test timeout multiplier")
    set(SDL_TESTS_LINK_SHARED ${TEST_SHARED})

    add_definitions(-DNO_BUILD_CONFIG)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../test" SDL_test)
endif()

if(ANDROID)
    find_package(SDL3 REQUIRED CONFIG COMPONENTS Jar)
endif()

feature_summary(WHAT ALL)
