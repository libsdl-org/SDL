# This cmake build script is meant for verifying the various CMake configuration script.

cmake_minimum_required(VERSION 3.12)
project(sdl_test LANGUAGES C)
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

find_package(SDL3 REQUIRED CONFIG COMPONENTS Headers)
add_library(headers_test OBJECT inc_sdl_slash.c inc_sdl_noslash.c)
target_link_libraries(headers_test PRIVATE SDL3::Headers)

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

    option(SDL_STATIC_PIC "SDL static library has been built with PIC")
    if(SDL_STATIC_PIC OR WIN32)
        add_library(sharedlib-static SHARED main_lib.c)
        target_link_libraries(sharedlib-static PRIVATE SDL3::SDL3-static)
        generate_export_header(sharedlib-static EXPORT_MACRO_NAME MYLIBRARY_EXPORT)
        target_compile_definitions(sharedlib-static PRIVATE "EXPORT_HEADER=\"${CMAKE_CURRENT_BINARY_DIR}/sharedlib-static_export.h\"")
        set_target_properties(sharedlib-static PROPERTIES C_VISIBILITY_PRESET "hidden")
    endif()

    if(TEST_TEST)
        add_executable(sdltest-static sdltest.c)
        target_link_libraries(sdltest-static PRIVATE SDL3::SDL3_test SDL3::SDL3-static)
    endif()
endif()

find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
add_executable(gui-whatever WIN32 main_gui.c)
target_link_libraries(gui-whatever PRIVATE SDL3::SDL3)

if(ANDROID)
    find_package(SDL3 REQUIRED CONFIG COMPONENTS Jar)
endif()

feature_summary(WHAT ALL)
