# This cmake build script is meant for verifying the various CMake configuration script.

cmake_minimum_required(VERSION 3.12)
project(sdl_test LANGUAGES C)

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

if(TEST_SHARED)
    find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
    add_executable(gui-shared WIN32 main_gui.c)
    target_link_libraries(gui-shared PRIVATE SDL3::SDL3)
    if(WIN32)
        add_custom_command(TARGET gui-shared POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:gui-shared>"
        )
    endif()

    add_library(sharedlib-shared SHARED main_lib.c)
    target_link_libraries(sharedlib-shared PRIVATE SDL3::SDL3)
    generate_export_header(sharedlib-shared EXPORT_MACRO_NAME MYLIBRARY_EXPORT)
    target_compile_definitions(sharedlib-shared PRIVATE "EXPORT_HEADER=\"${CMAKE_CURRENT_BINARY_DIR}/sharedlib-shared_export.h\"")
    set_target_properties(sharedlib-shared PROPERTIES C_VISIBILITY_PRESET "hidden")

    add_executable(gui-shared-vars WIN32 main_gui.c)
    target_link_libraries(gui-shared-vars PRIVATE ${SDL3_LIBRARIES})
    target_include_directories(gui-shared-vars PRIVATE ${SDL3_INCLUDE_DIRS})

    add_executable(cli-shared main_cli.c)
    target_link_libraries(cli-shared PRIVATE SDL3::SDL3)
    if(WIN32)
        add_custom_command(TARGET cli-shared POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:cli-shared>"
        )
    endif()

    add_executable(cli-shared-vars main_cli.c)
    target_link_libraries(cli-shared-vars PRIVATE ${SDL3_LIBRARIES})
    target_include_directories(cli-shared-vars PRIVATE ${SDL3_INCLUDE_DIRS})

    add_library(sharedlib-shared-vars SHARED main_lib.c)
    target_link_libraries(sharedlib-shared-vars PRIVATE ${SDL3_LIBRARIES})
    target_include_directories(sharedlib-shared-vars PRIVATE ${SDL3_INCLUDE_DIRS})
    generate_export_header(sharedlib-shared-vars EXPORT_MACRO_NAME MYLIBRARY_EXPORT)
    target_compile_definitions(sharedlib-shared-vars PRIVATE "EXPORT_HEADER=\"${CMAKE_CURRENT_BINARY_DIR}/sharedlib-shared-vars_export.h\"")
    set_target_properties(sharedlib-shared-vars PROPERTIES C_VISIBILITY_PRESET "hidden")
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

    add_executable(gui-static-vars WIN32 main_gui.c)
    target_link_libraries(gui-static-vars PRIVATE ${SDL3_STATIC_LIBRARIES})
    target_include_directories(gui-static-vars PRIVATE ${SDL3_INCLUDE_DIRS})

    add_executable(cli-static main_cli.c)
    target_link_libraries(cli-static PRIVATE SDL3::SDL3-static)

    add_executable(cli-static-vars main_cli.c)
    target_link_libraries(cli-static-vars PRIVATE ${SDL3_STATIC_LIBRARIES})
    target_include_directories(cli-static-vars PRIVATE ${SDL3_INCLUDE_DIRS})

endif()

message(STATUS "SDL3_PREFIX:            ${SDL3_PREFIX}")
message(STATUS "SDL3_INCLUDE_DIR:       ${SDL3_INCLUDE_DIR}")
message(STATUS "SDL3_INCLUDE_DIRS:      ${SDL3_INCLUDE_DIRS}")
message(STATUS "SDL3_LIBRARIES:         ${SDL3_LIBRARIES}")
message(STATUS "SDL3_STATIC_LIBRARIES:  ${SDL3_STATIC_LIBRARIES}")
message(STATUS "SDL3TEST_LIBRARY:       ${SDL3TEST_LIBRARY}")

feature_summary(WHAT ALL)
