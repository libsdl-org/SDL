cmake_minimum_required(VERSION 3.0)
project(SDL3_test)

enable_testing()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/sdlplatform.cmake")
SDL_DetectCMakePlatform()

include(CheckCCompilerFlag)
include(CheckIncludeFile)
include(CMakeParseArguments)
include(CMakePushCheckState)
include(GNUInstallDirs)

set(SDL_TESTS_LINK_SHARED_DEFAULT ON)
if(EMSCRIPTEN OR N3DS OR PS2 OR PSP OR RISCOS OR VITA)
    set(SDL_TESTS_LINK_SHARED_DEFAULT OFF)
endif()

option(SDL_TESTS_LINK_SHARED "link tests to shared SDL library" ${SDL_TESTS_LINK_SHARED_DEFAULT})
set(SDL_TESTS_TIMEOUT_MULTIPLIER "1" CACHE STRING "Timeout multiplier to account for really slow machines")

if(SDL_TESTS_LINK_SHARED)
    set(sdl_name_component SDL3-shared)
else()
    set(sdl_name_component SDL3-static)
endif()

if(NOT TARGET SDL3::${sdl_name_component})
    find_package(SDL3 3.0.0 REQUIRED CONFIG COMPONENTS ${sdl_name_component} SDL3_test)
endif()

if(TARGET sdl-build-options)
    set(SDL3_TESTS_SUBPROJECT ON)
else()
    set(SDL3_TESTS_SUBPROJECT OFF)
endif()

# CMake incorrectly detects opengl32.lib being present on MSVC ARM64
if(NOT MSVC OR NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    # Prefer GLVND, if present
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL)
endif()

set(SDL_TEST_EXECUTABLES)

# FIXME: can be OBJECT library for CMake 3.16
add_library(sdltests_utils STATIC
    testutils.c
)
target_link_libraries(sdltests_utils PRIVATE SDL3::${sdl_name_component})

file(GLOB RESOURCE_FILES *.bmp *.wav *.hex moose.dat utf8.txt)
set(RESOURCE_FILE_NAMES)
foreach(RESOURCE_FILE ${RESOURCE_FILES})
    get_filename_component(res_file_name ${RESOURCE_FILE} NAME)
    list(APPEND RESOURCE_FILE_NAMES "${res_file_name}")
endforeach()

define_property(TARGET PROPERTY SDL_NONINTERACTIVE BRIEF_DOCS "If true, target is a non-interactive test executable." FULL_DOCS "If true, target is a noninteractive test executable.")
define_property(TARGET PROPERTY SDL_NONINTERACTIVE_ARGUMENTS BRIEF_DOCS "Argument(s) to run executable in non-interactive mode." FULL_DOCS "Argument(s) to run executable in non-interactive mode.")
define_property(TARGET PROPERTY SDL_NONINTERACTIVE_TIMEOUT BRIEF_DOCS "Timeout for noninteractive executable." FULL_DOCS "Timeout for noninteractive executable.")

macro(add_sdl_test_executable TARGET)
    cmake_parse_arguments(AST "NONINTERACTIVE;NEEDS_RESOURCES;TESTUTILS" "" "NONINTERACTIVE_TIMEOUT;NONINTERACTIVE_ARGS;SOURCES" ${ARGN})
    if(AST_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown argument(s): ${AST_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT AST_SOURCES)
        message(FATAL_ERROR "add_sdl_test_executable needs at least one source")
    endif()
    if(AST_NEEDS_RESOURCES)
        list(APPEND AST_SOURCES ${RESOURCE_FILES})
    endif()
    if(ANDROID)
        add_library(${TARGET} SHARED ${AST_SOURCES})
    else()
        add_executable(${TARGET} ${AST_SOURCES})
    endif()
    target_link_libraries(${TARGET} PRIVATE SDL3::SDL3_test SDL3::${sdl_name_component})
    if(AST_TESTUTILS)
        target_link_libraries(${TARGET} PRIVATE sdltests_utils)
    endif()

    list(APPEND SDL_TEST_EXECUTABLES ${TARGET})
    if(AST_NONINTERACTIVE)
        set_property(TARGET ${TARGET} PROPERTY SDL_NONINTERACTIVE 1)
    endif()
    if(AST_NONINTERACTIVE_ARGS)
        set_property(TARGET ${TARGET} PROPERTY SDL_NONINTERACTIVE_ARGUMENTS "${AST_NONINTERACTIVE_ARGS}")
    endif()
    if(AST_NONINTERACTIVE_TIMEOUT)
        set_property(TARGET ${TARGET} PROPERTY SDL_NONINTERACTIVE_TIMEOUT "${AST_NONINTERACTIVE_TIMEOUT}")
    endif()
    if(AST_NEEDS_RESOURCES)
        if(PSP OR PS2)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} ARGS -E make_directory $<TARGET_FILE_DIR:${TARGET}>/sdl-${TARGET}
                COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${RESOURCE_FILES} $<TARGET_FILE_DIR:${TARGET}>/sdl-${TARGET})
        else()
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${RESOURCE_FILES} $<TARGET_FILE_DIR:${TARGET}>)
        endif()
        if(APPLE)
            # Make sure resource files get installed into macOS/iOS .app bundles.
            set_target_properties(${TARGET} PROPERTIES RESOURCE "${RESOURCE_FILES}")
        endif()
        set_property(TARGET ${TARGET} APPEND PROPERTY ADDITIONAL_CLEAN_FILES "$<TARGET_FILE_DIR:${TARGET}>/$<JOIN:${RESOURCE_FILE_NAMES},$<SEMICOLON>$<TARGET_FILE_DIR:${TARGET}>/>")
    endif()

    if(WINDOWS)
        # CET support was added in VS 16.7
        if(MSVC_VERSION GREATER 1926 AND CMAKE_GENERATOR_PLATFORM MATCHES "Win32|x64")
            set_property(TARGET ${TARGET} APPEND_STRING PROPERTY LINK_FLAGS " -CETCOMPAT")
        endif()
    elseif(PSP)
        target_link_libraries(${TARGET} PRIVATE GL)
    endif()

    if(OPENGL_FOUND)
        target_compile_definitions(${TARGET} PRIVATE HAVE_OPENGL)
    endif()

    if(TARGET sdl-global-options)
        target_link_libraries(${TARGET} PRIVATE $<BUILD_INTERFACE:sdl-global-options>)
    endif()

    if(SDL3_TESTS_SUBPROJECT)
        # FIXME: only add "${SDL3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>" + include paths of external dependencies
        target_include_directories(${TARGET} PRIVATE "$<TARGET_PROPERTY:SDL3::${sdl_name_component},INCLUDE_DIRECTORIES>")
    else()
        target_include_directories(${TARGET} PRIVATE "../include")
    endif()
endmacro()

check_include_file(signal.h HAVE_SIGNAL_H)
if(HAVE_SIGNAL_H)
    add_definitions(-DHAVE_SIGNAL_H)
endif()

check_include_file(libudev.h HAVE_LIBUDEV_H)
if(HAVE_LIBUDEV_H)
    add_definitions(-DHAVE_LIBUDEV_H)
endif()

add_sdl_test_executable(checkkeys SOURCES checkkeys.c)
add_sdl_test_executable(checkkeysthreads SOURCES checkkeysthreads.c)
add_sdl_test_executable(loopwave NEEDS_RESOURCES TESTUTILS SOURCES loopwave.c)
add_sdl_test_executable(loopwavequeue NEEDS_RESOURCES TESTUTILS SOURCES loopwavequeue.c)
add_sdl_test_executable(testsurround SOURCES testsurround.c)
add_sdl_test_executable(testresample NEEDS_RESOURCES SOURCES testresample.c)
add_sdl_test_executable(testaudioinfo SOURCES testaudioinfo.c)

file(GLOB TESTAUTOMATION_SOURCE_FILES testautomation*.c)
add_sdl_test_executable(testautomation NEEDS_RESOURCES SOURCES ${TESTAUTOMATION_SOURCE_FILES})
add_sdl_test_executable(testmultiaudio NEEDS_RESOURCES TESTUTILS SOURCES testmultiaudio.c)
add_sdl_test_executable(testaudiohotplug NEEDS_RESOURCES TESTUTILS SOURCES testaudiohotplug.c)
add_sdl_test_executable(testaudiocapture SOURCES testaudiocapture.c)
add_sdl_test_executable(testatomic NONINTERACTIVE SOURCES testatomic.c)
add_sdl_test_executable(testintersections SOURCES testintersections.c)
add_sdl_test_executable(testrelative SOURCES testrelative.c)
add_sdl_test_executable(testhittesting SOURCES testhittesting.c)
add_sdl_test_executable(testdraw SOURCES testdraw.c)
add_sdl_test_executable(testdrawchessboard SOURCES testdrawchessboard.c)
add_sdl_test_executable(testdropfile SOURCES testdropfile.c)
add_sdl_test_executable(testerror NONINTERACTIVE SOURCES testerror.c)

if(SDL3_TESTS_SUBPROJECT)
    set(build_options_dependent_tests )

    add_sdl_test_executable(testevdev NONINTERACTIVE SOURCES testevdev.c)
    list(APPEND build_options_dependent_tests testevdev)

    if(APPLE)
        add_sdl_test_executable(testnative NEEDS_RESOURCES TESTUTILS
            SOURCES
                testnative.c
                testnativecocoa.m
                testnativex11.c
        )

        cmake_push_check_state()
        check_c_compiler_flag(-Wno-error=deprecated-declarations HAVE_WNO_ERROR_DEPRECATED_DECLARATIONS)
        cmake_pop_check_state()
        target_link_libraries(testnative PRIVATE "-Wl,-framework,Cocoa")
        if(HAVE_WNO_ERROR_DEPRECATED_DECLARATIONS)
            set_property(SOURCE "testnativecocoa.m" APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-error=deprecated-declarations")
        endif()
        list(APPEND build_options_dependent_tests testnative)
    elseif(WINDOWS)
        add_sdl_test_executable(testnative NEEDS_RESOURCES TESTUTILS SOURCES testnative.c testnativew32.c)
        list(APPEND build_options_dependent_tests testnative)
    elseif(HAVE_X11)
        add_sdl_test_executable(testnative NEEDS_RESOURCES TESTUTILS SOURCES testnative.c testnativex11.c)
        target_link_libraries(testnative PRIVATE X11)
        list(APPEND build_options_dependent_tests testnative)
    endif()

    foreach(t ${build_options_dependent_tests})
        target_include_directories(${t} BEFORE PRIVATE $<TARGET_PROPERTY:sdl-build-options,INTERFACE_INCLUDE_DIRECTORIES>)
        target_include_directories(${t} BEFORE PRIVATE ${SDL3_SOURCE_DIR}/src)
    endforeach()
endif()

add_sdl_test_executable(testfile NONINTERACTIVE SOURCES testfile.c)
add_sdl_test_executable(testgamepad NEEDS_RESOURCES TESTUTILS SOURCES testgamepad.c)
add_sdl_test_executable(testgeometry TESTUTILS SOURCES testgeometry.c)
add_sdl_test_executable(testgl SOURCES testgl.c)
add_sdl_test_executable(testgles SOURCES testgles.c)
if(ANDROID)
    target_link_libraries(testgles PRIVATE GLESv1_CM)
endif()
add_sdl_test_executable(testgles2 SOURCES testgles2.c)
add_sdl_test_executable(testgles2_sdf TESTUTILS SOURCES testgles2_sdf.c)
add_sdl_test_executable(testhaptic SOURCES testhaptic.c)
add_sdl_test_executable(testhotplug SOURCES testhotplug.c)
add_sdl_test_executable(testrumble SOURCES testrumble.c)
add_sdl_test_executable(testthread NONINTERACTIVE NONINTERACTIVE_TIMEOUT 40 SOURCES testthread.c)
add_sdl_test_executable(testiconv NEEDS_RESOURCES TESTUTILS SOURCES testiconv.c)
add_sdl_test_executable(testime NEEDS_RESOURCES TESTUTILS SOURCES testime.c)
add_sdl_test_executable(testjoystick SOURCES testjoystick.c)
add_sdl_test_executable(testkeys SOURCES testkeys.c)
add_sdl_test_executable(testloadso SOURCES testloadso.c)
add_sdl_test_executable(testlocale NONINTERACTIVE SOURCES testlocale.c)
add_sdl_test_executable(testlock SOURCES testlock.c)
add_sdl_test_executable(testmouse SOURCES testmouse.c)

add_sdl_test_executable(testoverlay NEEDS_RESOURCES TESTUTILS SOURCES testoverlay.c)
add_sdl_test_executable(testplatform NONINTERACTIVE SOURCES testplatform.c)
add_sdl_test_executable(testpower NONINTERACTIVE SOURCES testpower.c)
add_sdl_test_executable(testfilesystem NONINTERACTIVE SOURCES testfilesystem.c)
add_sdl_test_executable(testrendertarget NEEDS_RESOURCES TESTUTILS SOURCES testrendertarget.c)
add_sdl_test_executable(testscale NEEDS_RESOURCES TESTUTILS SOURCES testscale.c)
add_sdl_test_executable(testsem NONINTERACTIVE NONINTERACTIVE_ARGS 10 NONINTERACTIVE_TIMEOUT 30 SOURCES testsem.c)
add_sdl_test_executable(testsensor SOURCES testsensor.c)
add_sdl_test_executable(testshader NEEDS_RESOURCES TESTUTILS SOURCES testshader.c)
add_sdl_test_executable(testshape NEEDS_RESOURCES SOURCES testshape.c)
add_sdl_test_executable(testsprite NEEDS_RESOURCES TESTUTILS SOURCES testsprite.c)
add_sdl_test_executable(testspriteminimal NEEDS_RESOURCES TESTUTILS SOURCES testspriteminimal.c)
add_sdl_test_executable(teststreaming NEEDS_RESOURCES TESTUTILS SOURCES teststreaming.c)
add_sdl_test_executable(testtimer NONINTERACTIVE NONINTERACTIVE_TIMEOUT 60 SOURCES testtimer.c)
add_sdl_test_executable(testurl SOURCES testurl.c)
add_sdl_test_executable(testver NONINTERACTIVE SOURCES testver.c)
add_sdl_test_executable(testviewport NEEDS_RESOURCES TESTUTILS SOURCES testviewport.c)
add_sdl_test_executable(testwm SOURCES testwm.c)
add_sdl_test_executable(testyuv NONINTERACTIVE NONINTERACTIVE_ARGS "--automated" NEEDS_RESOURCES TESTUTILS SOURCES testyuv.c testyuv_cvt.c)
add_sdl_test_executable(torturethread NONINTERACTIVE NONINTERACTIVE_TIMEOUT 30 SOURCES torturethread.c)
add_sdl_test_executable(testrendercopyex NEEDS_RESOURCES TESTUTILS SOURCES testrendercopyex.c)
add_sdl_test_executable(testmessage SOURCES testmessage.c)
add_sdl_test_executable(testdisplayinfo SOURCES testdisplayinfo.c)
add_sdl_test_executable(testqsort NONINTERACTIVE SOURCES testqsort.c)
add_sdl_test_executable(testbounds NONINTERACTIVE SOURCES testbounds.c)
add_sdl_test_executable(testcustomcursor SOURCES testcustomcursor.c)
add_sdl_test_executable(gamepadmap NEEDS_RESOURCES TESTUTILS SOURCES gamepadmap.c)
add_sdl_test_executable(testvulkan SOURCES testvulkan.c)
add_sdl_test_executable(testoffscreen SOURCES testoffscreen.c)
add_sdl_test_executable(testpopup SOURCES testpopup.c)

check_c_compiler_flag(-Wformat-overflow HAVE_WFORMAT_OVERFLOW)
if(HAVE_WFORMAT_OVERFLOW)
    target_compile_definitions(testautomation PRIVATE HAVE_WFORMAT_OVERFLOW)
endif()

check_c_compiler_flag(-Wformat HAVE_WFORMAT)
if(HAVE_WFORMAT)
    target_compile_definitions(testautomation PRIVATE HAVE_WFORMAT)
endif()

cmake_push_check_state()
if(HAVE_WFORMAT)
    # Some compilers ignore -Wformat-extra-args without -Wformat
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Wformat")
endif()
check_c_compiler_flag(-Wformat-extra-args HAVE_WFORMAT_EXTRA_ARGS)
cmake_pop_check_state()
if(HAVE_WFORMAT_EXTRA_ARGS)
    target_compile_definitions(testautomation PRIVATE HAVE_WFORMAT_EXTRA_ARGS)
endif()

if(SDL_DUMMYAUDIO)
    set_property(TARGET testaudioinfo PROPERTY SDL_NONINTERACTIVE 1)
    set_property(TARGET testsurround PROPERTY SDL_NONINTERACTIVE 1)
endif()

if(SDL_DUMMYVIDEO)
    set_property(TARGET testkeys PROPERTY SDL_NONINTERACTIVE 1)
    set_property(TARGET testbounds PROPERTY SDL_NONINTERACTIVE 1)
    set_property(TARGET testdisplayinfo PROPERTY SDL_NONINTERACTIVE 1)
endif()

if(OPENGL_FOUND)
    if(TARGET OpenGL::GL)
        target_link_libraries(testshader PRIVATE OpenGL::GL)
        target_link_libraries(testgl PRIVATE OpenGL::GL)
    else()
        if(EMSCRIPTEN AND OPENGL_gl_LIBRARY STREQUAL "nul")
            set(OPENGL_gl_LIBRARY GL)
        endif()
        # emscripten's FindOpenGL.cmake does not create OpenGL::GL
        target_link_libraries(testshader PRIVATE ${OPENGL_gl_LIBRARY})
        target_link_libraries(testgl PRIVATE ${OPENGL_gl_LIBRARY})
    endif()
endif()
if(EMSCRIPTEN)
    set_property(TARGET testshader APPEND_STRING PROPERTY LINK_FLAGS " -sLEGACY_GL_EMULATION")
endif()

if(PSP)
    # Build EBOOT files if building for PSP
    foreach(APP ${SDL_TEST_EXECUTABLES})
        create_pbp_file(
            TARGET          ${APP}
            TITLE           SDL-${APP}
            ICON_PATH       NULL
            BACKGROUND_PATH NULL
            PREVIEW_PATH    NULL
        )
        add_custom_command(
            TARGET ${APP} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:${ARG_TARGET}>/sdl-${APP}
        )
        add_custom_command(
            TARGET ${APP} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rename
            $<TARGET_FILE_DIR:${ARG_TARGET}>/EBOOT.PBP
            $<TARGET_FILE_DIR:${ARG_TARGET}>/sdl-${APP}/EBOOT.PBP
        )
        if(BUILD_PRX)
            add_custom_command(
                TARGET ${APP} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_FILE_DIR:${ARG_TARGET}>/${APP}
                $<TARGET_FILE_DIR:${ARG_TARGET}>/sdl-${APP}/${APP}
            )
            add_custom_command(
                TARGET ${APP} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E rename
                $<TARGET_FILE_DIR:${ARG_TARGET}>/${APP}.prx
                $<TARGET_FILE_DIR:${ARG_TARGET}>/sdl-${APP}/${APP}.prx
            )
        endif()
        add_custom_command(
            TARGET ${APP} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove
            $<TARGET_FILE_DIR:${ARG_TARGET}>/PARAM.SFO
        )
    endforeach()
endif()

if(N3DS)
    set(ROMFS_DIR "${CMAKE_CURRENT_BINARY_DIR}/romfs")
    file(COPY ${RESOURCE_FILES} DESTINATION "${ROMFS_DIR}")

    foreach(APP ${SDL_TEST_EXECUTABLES})
        get_target_property(TARGET_BINARY_DIR ${APP} BINARY_DIR)
        set(SMDH_FILE "${TARGET_BINARY_DIR}/${APP}.smdh")
        ctr_generate_smdh("${SMDH_FILE}"
            NAME "SDL-${APP}"
            DESCRIPTION "SDL3 Test suite"
            AUTHOR "SDL3 Contributors"
            ICON "${CMAKE_CURRENT_SOURCE_DIR}/n3ds/logo48x48.png"
        )
        ctr_create_3dsx(
            ${APP}
            ROMFS "${ROMFS_DIR}"
            SMDH "${SMDH_FILE}"
        )
    endforeach()
endif()

if(RISCOS)
    set(SDL_TEST_EXECUTABLES_AIF)
    foreach(APP ${SDL_TEST_EXECUTABLES})
        set_property(TARGET ${APP} APPEND_STRING PROPERTY LINK_FLAGS " -static")
        add_custom_command(
            OUTPUT ${APP},ff8
            COMMAND elf2aif ${APP} ${APP},ff8
            DEPENDS ${APP}
        )
        add_custom_target(${APP}-aif ALL DEPENDS ${APP},ff8)
        list(APPEND SDL_TEST_EXECUTABLES_AIF ${CMAKE_CURRENT_BINARY_DIR}/${APP},ff8)
    endforeach()
endif()

# Set Apple App ID / Bundle ID.  This is needed to launch apps on some Apple
# platforms (iOS, for example).
if(APPLE)
    if(CMAKE_VERSION VERSION_LESS "3.7.0")
        # CMake's 'BUILDSYSTEM_TARGETS' property is only available in
        # CMake 3.7 and above.
        message(WARNING "Unable to set Bundle ID for Apple .app builds due to old CMake (pre 3.7).")
    else()
        foreach(CURRENT_TARGET ${SDL_TEST_EXECUTABLES})
            set_target_properties("${CURRENT_TARGET}" PROPERTIES
                MACOSX_BUNDLE_GUI_IDENTIFIER "org.libsdl.${CURRENT_TARGET}"
                MACOSX_BUNDLE_BUNDLE_VERSION "${SDL3_VERSION}"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "${SDL3_VERSION}"
            )
        endforeach()
    endif()
endif()

set(TESTS_ENVIRONMENT
    SDL_AUDIO_DRIVER=dummy
    SDL_VIDEO_DRIVER=dummy
    PATH=$<TARGET_FILE_DIR:SDL3::${sdl_name_component}>
)

foreach(TEST ${SDL_TEST_EXECUTABLES})
    get_property(noninteractive TARGET ${TEST} PROPERTY SDL_NONINTERACTIVE)
    if(noninteractive)
        set(command ${TEST})
        get_property(noninteractive_arguments TARGET ${TEST} PROPERTY SDL_NONINTERACTIVE_ARGUMENTS)
        if(noninteractive_arguments)
            list(APPEND command ${noninteractive_arguments})
        endif()
        add_test(
            NAME ${TEST}
            COMMAND ${command}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
        set_tests_properties(${TEST} PROPERTIES ENVIRONMENT "${TESTS_ENVIRONMENT}")
        get_property(noninteractive_timeout TARGET ${TEST} PROPERTY SDL_NONINTERACTIVE_TIMEOUT)
        if(NOT noninteractive_timeout)
            set(noninteractive_timeout 10)
        endif()
        math(EXPR noninteractive_timeout "${noninteractive_timeout}*${SDL_TESTS_TIMEOUT_MULTIPLIER}")
        set_tests_properties(${TEST} PROPERTIES TIMEOUT "${noninteractive_timeout}")
        if(SDL_INSTALL_TESTS)
            set(exe ${TEST})
            set(installedtestsdir "${CMAKE_INSTALL_FULL_LIBEXECDIR}/installed-tests/SDL3")
            configure_file(template.test.in "${exe}.test" @ONLY)
            install(
                FILES "${CMAKE_CURRENT_BINARY_DIR}/${exe}.test"
                DESTINATION ${CMAKE_INSTALL_DATADIR}/installed-tests/SDL3
            )
        endif()
    endif()
endforeach()

if(SDL_INSTALL_TESTS)
    if(RISCOS)
        install(
            FILES ${SDL_TEST_EXECUTABLES_AIF}
            DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL3
        )
    else()
        install(
            TARGETS ${SDL_TEST_EXECUTABLES}
            DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL3
        )
    endif()
    install(
        FILES ${RESOURCE_FILES}
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL3
    )
endif()
