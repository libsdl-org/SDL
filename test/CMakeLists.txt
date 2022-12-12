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
if(WINDOWS)
    # Avoid missing dll error by linking to static SDL library,
    # alternatively, copy dll to build directory in a post build event.
    set(SDL_TESTS_LINK_SHARED_DEFAULT OFF)
endif()
if(EMSCRIPTEN OR N3DS OR PS2 OR PSP OR RISCOS OR VITA)
    set(SDL_TESTS_LINK_SHARED_DEFAULT OFF)
endif()

option(SDL_TESTS_LINK_SHARED "link tests to shared SDL library" ${SDL_TESTS_LINK_SHARED_DEFAULT})

if(SDL_TESTS_LINK_SHARED)
    set(sdl_name_component SDL3)
else()
    set(sdl_name_component SDL3-static)
endif()

if(NOT TARGET SDL3::${sdl_name_component})
    find_package(SDL3 3.0.0 REQUIRED COMPONENTS ${sdl_name_component} SDL3_test)
endif()

# CMake incorrectly detects opengl32.lib being present on MSVC ARM64
if(NOT MSVC OR NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    # Prefer GLVND, if present
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL)
endif()

set(SDL_TEST_EXECUTABLES)
set(SDL_TESTS_NONINTERACTIVE)

add_library(sdltests_utils OBJECT
    testutils.c
)
target_link_libraries(sdltests_utils PRIVATE SDL3::${sdl_name_component})

file(GLOB RESOURCE_FILES *.bmp *.wav *.hex moose.dat utf8.txt)

macro(add_sdl_test_executable TARGET)
    cmake_parse_arguments(AST "NONINTERACTIVE;NEEDS_RESOURCES;TESTUTILS" "" "" ${ARGN})
    set(SOURCES ${AST_UNPARSED_ARGUMENTS})
    if(AST_TESTUTILS)
        list(APPEND SOURCES $<TARGET_OBJECTS:sdltests_utils>)
    endif()
    if(AST_NEEDS_RESOURCES)
        list(APPEND SOURCES ${RESOURCE_FILES})
    endif()
    add_executable(${TARGET} ${SOURCES})
    target_link_libraries(${TARGET} PRIVATE SDL3::SDL3_test SDL3::${sdl_name_component})

    list(APPEND SDL_TEST_EXECUTABLES ${TARGET})
    if(AST_NONINTERACTIVE)
        list(APPEND SDL_TESTS_NONINTERACTIVE ${TARGET})
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
endmacro()

check_include_file(signal.h HAVE_SIGNAL_H)
if(HAVE_SIGNAL_H)
    add_definitions(-DHAVE_SIGNAL_H)
endif()

check_include_file(libudev.h HAVE_LIBUDEV_H)
if(HAVE_LIBUDEV_H)
    add_definitions(-DHAVE_LIBUDEV_H)
endif()

add_sdl_test_executable(checkkeys checkkeys.c)
add_sdl_test_executable(checkkeysthreads checkkeysthreads.c)
add_sdl_test_executable(loopwave NEEDS_RESOURCES TESTUTILS loopwave.c)
add_sdl_test_executable(loopwavequeue NEEDS_RESOURCES TESTUTILS loopwavequeue.c)
add_sdl_test_executable(testsurround testsurround.c)
add_sdl_test_executable(testresample NEEDS_RESOURCES testresample.c)
add_sdl_test_executable(testaudioinfo testaudioinfo.c)

file(GLOB TESTAUTOMATION_SOURCE_FILES testautomation*.c)
add_sdl_test_executable(testautomation NEEDS_RESOURCES ${TESTAUTOMATION_SOURCE_FILES})
add_sdl_test_executable(testmultiaudio NEEDS_RESOURCES TESTUTILS testmultiaudio.c)
add_sdl_test_executable(testaudiohotplug NEEDS_RESOURCES TESTUTILS testaudiohotplug.c)
add_sdl_test_executable(testaudiocapture testaudiocapture.c)
add_sdl_test_executable(testatomic NONINTERACTIVE testatomic.c)
add_sdl_test_executable(testintersections testintersections.c)
add_sdl_test_executable(testrelative testrelative.c)
add_sdl_test_executable(testhittesting testhittesting.c)
add_sdl_test_executable(testdraw2 testdraw2.c)
add_sdl_test_executable(testdrawchessboard testdrawchessboard.c)
add_sdl_test_executable(testdropfile testdropfile.c)
add_sdl_test_executable(testerror NONINTERACTIVE testerror.c)

if(LINUX AND TARGET sdl-build-options)
    add_sdl_test_executable(testevdev NONINTERACTIVE testevdev.c)
    target_include_directories(testevdev BEFORE PRIVATE $<TARGET_PROPERTY:sdl-build-options,INTERFACE_INCLUDE_DIRECTORIES>)
    target_include_directories(testevdev BEFORE PRIVATE ${SDL3_SOURCE_DIR}/src)
endif()

add_sdl_test_executable(testfile testfile.c)
add_sdl_test_executable(testgamecontroller NEEDS_RESOURCES TESTUTILS testgamecontroller.c)
add_sdl_test_executable(testgeometry TESTUTILS testgeometry.c)
add_sdl_test_executable(testgl2 testgl2.c)
add_sdl_test_executable(testgles testgles.c)
add_sdl_test_executable(testgles2 testgles2.c)
add_sdl_test_executable(testgles2_sdf TESTUTILS testgles2_sdf.c)
add_sdl_test_executable(testhaptic testhaptic.c)
add_sdl_test_executable(testhotplug testhotplug.c)
add_sdl_test_executable(testrumble testrumble.c)
add_sdl_test_executable(testthread NONINTERACTIVE testthread.c)
add_sdl_test_executable(testiconv NEEDS_RESOURCES TESTUTILS testiconv.c)
add_sdl_test_executable(testime NEEDS_RESOURCES TESTUTILS testime.c)
add_sdl_test_executable(testjoystick testjoystick.c)
add_sdl_test_executable(testkeys testkeys.c)
add_sdl_test_executable(testloadso testloadso.c)
add_sdl_test_executable(testlocale NONINTERACTIVE testlocale.c)
add_sdl_test_executable(testlock testlock.c)
add_sdl_test_executable(testmouse testmouse.c)

if(APPLE)
    add_sdl_test_executable(testnative NEEDS_RESOURCES TESTUTILS
        testnative.c
        testnativecocoa.m
        testnativex11.c
    )

    cmake_push_check_state()
    check_c_compiler_flag(-Wno-error=deprecated-declarations HAVE_WNO_ERROR_DEPRECATED_DECLARATIONS)
    cmake_pop_check_state()
    if(HAVE_WNO_ERROR_DEPRECATED_DECLARATIONS)
        set_property(SOURCE "testnativecocoa.m" APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-error=deprecated-declarations")
    endif()
elseif(WINDOWS)
    add_sdl_test_executable(testnative NEEDS_RESOURCES TESTUTILS testnative.c testnativew32.c)
elseif(HAVE_X11)
    add_sdl_test_executable(testnative NEEDS_RESOURCES TESTUTILS testnative.c testnativex11.c)
    target_link_libraries(testnative PRIVATE X11)
endif()

add_sdl_test_executable(testoverlay2 NEEDS_RESOURCES TESTUTILS testoverlay2.c)
add_sdl_test_executable(testplatform NONINTERACTIVE testplatform.c)
add_sdl_test_executable(testpower NONINTERACTIVE testpower.c)
add_sdl_test_executable(testfilesystem NONINTERACTIVE testfilesystem.c)
add_sdl_test_executable(testrendertarget NEEDS_RESOURCES TESTUTILS testrendertarget.c)
add_sdl_test_executable(testscale NEEDS_RESOURCES TESTUTILS testscale.c)
add_sdl_test_executable(testsem testsem.c)
add_sdl_test_executable(testsensor testsensor.c)
add_sdl_test_executable(testshader NEEDS_RESOURCES testshader.c)
add_sdl_test_executable(testshape NEEDS_RESOURCES testshape.c)
add_sdl_test_executable(testsprite2 NEEDS_RESOURCES TESTUTILS testsprite2.c)
add_sdl_test_executable(testspriteminimal NEEDS_RESOURCES TESTUTILS testspriteminimal.c)
add_sdl_test_executable(teststreaming NEEDS_RESOURCES TESTUTILS teststreaming.c)
add_sdl_test_executable(testtimer NONINTERACTIVE testtimer.c)
add_sdl_test_executable(testurl testurl.c)
add_sdl_test_executable(testver NONINTERACTIVE testver.c)
add_sdl_test_executable(testviewport NEEDS_RESOURCES TESTUTILS testviewport.c)
add_sdl_test_executable(testwm2 testwm2.c)
add_sdl_test_executable(testyuv NEEDS_RESOURCES testyuv.c testyuv_cvt.c)
add_sdl_test_executable(torturethread torturethread.c)
add_sdl_test_executable(testrendercopyex NEEDS_RESOURCES TESTUTILS testrendercopyex.c)
add_sdl_test_executable(testmessage testmessage.c)
add_sdl_test_executable(testdisplayinfo testdisplayinfo.c)
add_sdl_test_executable(testqsort NONINTERACTIVE testqsort.c)
add_sdl_test_executable(testbounds testbounds.c)
add_sdl_test_executable(testcustomcursor testcustomcursor.c)
add_sdl_test_executable(controllermap NEEDS_RESOURCES TESTUTILS controllermap.c)
add_sdl_test_executable(testvulkan testvulkan.c)
add_sdl_test_executable(testoffscreen testoffscreen.c)

cmake_push_check_state()

check_c_compiler_flag(-Wformat-overflow HAVE_WFORMAT_OVERFLOW)
if(HAVE_WFORMAT_OVERFLOW)
    target_compile_definitions(testautomation PRIVATE HAVE_WFORMAT_OVERFLOW)
endif()

check_c_compiler_flag(-Wformat HAVE_WFORMAT)
if(HAVE_WFORMAT)
    target_compile_definitions(testautomation PRIVATE HAVE_WFORMAT)
endif()

check_c_compiler_flag(-Wformat-extra-args HAVE_WFORMAT_EXTRA_ARGS)
if(HAVE_WFORMAT_EXTRA_ARGS)
    target_compile_definitions(testautomation PRIVATE HAVE_WFORMAT_EXTRA_ARGS)
endif()

cmake_pop_check_state()

if(SDL_DUMMYAUDIO)
  list(APPEND SDL_TESTS_NONINTERACTIVE
    testaudioinfo
    testsurround
  )
endif()

if(SDL_DUMMYVIDEO)
  list(APPEND SDL_TESTS_NONINTERACTIVE
    testkeys
    testbounds
    testdisplayinfo
  )
endif()

if(OPENGL_FOUND)
    if(TARGET OpenGL::GL)
        target_link_libraries(testshader PRIVATE OpenGL::GL)
        target_link_libraries(testgl2 PRIVATE OpenGL::GL)
    else()
        if(EMSCRIPTEN AND OPENGL_gl_LIBRARY STREQUAL "nul")
            set(OPENGL_gl_LIBRARY GL)
        endif()
        # emscripten's FindOpenGL.cmake does not create OpenGL::GL
        target_link_libraries(testshader PRIVATE ${OPENGL_gl_LIBRARY})
        target_link_libraries(testgl2 PRIVATE ${OPENGL_gl_LIBRARY})
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
                MACOSX_BUNDLE_BUNDLE_VERSION "${SDL_VERSION}"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "${SDL_VERSION}"
            )
        endforeach()
    endif()
endif()

set(TESTS_ENVIRONMENT
    SDL_AUDIO_DRIVER=dummy
    SDL_VIDEO_DRIVER=dummy
)

foreach(TESTCASE ${SDL_TESTS_NONINTERACTIVE})
    add_test(
        NAME ${TESTCASE}
        COMMAND ${TESTCASE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    set_tests_properties(${TESTCASE}
        PROPERTIES
            ENVIRONMENT "${TESTS_ENVIRONMENT}"
            TIMEOUT 10
    )
    if(SDL_INSTALL_TESTS)
        set(exe ${TESTCASE})
        set(installedtestsdir "${CMAKE_INSTALL_FULL_LIBEXECDIR}/installed-tests/SDL3")
        configure_file(template.test.in "${exe}.test" @ONLY)
        install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/${exe}.test"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/installed-tests/SDL3
        )
    endif()
endforeach()

set_tests_properties(testthread PROPERTIES TIMEOUT 40)
set_tests_properties(testtimer PROPERTIES TIMEOUT 60)

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
