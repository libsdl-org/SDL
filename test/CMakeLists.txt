cmake_minimum_required(VERSION 3.0.0)
project(SDL2 C)
include(CTest)

if(SDL_INSTALL_TESTS)
    include(GNUInstallDirs)
endif()

# Global settings for all of the test targets
# FIXME: is this wrong?
remove_definitions(-DUSING_GENERATED_CONFIG_H)

if(PSP)
    link_libraries(
        SDL2main
        SDL2_test
        SDL2-static
        GL
        pspvram
        pspvfpu
        pspdisplay
        pspgu
        pspge
        pspaudio
        pspctrl
        psphprm
        psppower
    )
else()
    link_libraries(SDL2_test SDL2-static)
endif()

if(WINDOWS)
    # mingw32 must come before SDL2main to link successfully
    if(MINGW OR CYGWIN)
        link_libraries(mingw32)
    endif()

    # FIXME: Parent directory CMakeLists.txt only sets these for mingw/cygwin,
    # but we need them for VS as well.
    link_libraries(SDL2main)
    add_definitions(-Dmain=SDL_main)
endif()

# CMake incorrectly detects opengl32.lib being present on MSVC ARM64
if(NOT MSVC OR NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    # Prefer GLVND, if present
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL)
endif()

if (OPENGL_FOUND)
add_definitions(-DHAVE_OPENGL)
endif()

add_executable(checkkeys checkkeys.c)
add_executable(checkkeysthreads checkkeysthreads.c)
add_executable(loopwave loopwave.c testutils.c)
add_executable(loopwavequeue loopwavequeue.c testutils.c)
add_executable(testsurround testsurround.c)
add_executable(testresample testresample.c)
add_executable(testaudioinfo testaudioinfo.c)

file(GLOB TESTAUTOMATION_SOURCE_FILES testautomation*.c)
add_executable(testautomation ${TESTAUTOMATION_SOURCE_FILES})

add_executable(testmultiaudio testmultiaudio.c testutils.c)
add_executable(testaudiohotplug testaudiohotplug.c testutils.c)
add_executable(testaudiocapture testaudiocapture.c)
add_executable(testatomic testatomic.c)
add_executable(testintersections testintersections.c)
add_executable(testrelative testrelative.c)
add_executable(testhittesting testhittesting.c)
add_executable(testdraw2 testdraw2.c)
add_executable(testdrawchessboard testdrawchessboard.c)
add_executable(testdropfile testdropfile.c)
add_executable(testerror testerror.c)

if(LINUX)
    add_executable(testevdev testevdev.c)
endif()

add_executable(testfile testfile.c)
add_executable(testgamecontroller testgamecontroller.c testutils.c)
add_executable(testgeometry testgeometry.c testutils.c)
add_executable(testgesture testgesture.c)
add_executable(testgl2 testgl2.c)
add_executable(testgles testgles.c)
add_executable(testgles2 testgles2.c)
add_executable(testguid testguid.c)
add_executable(testhaptic testhaptic.c)
add_executable(testhotplug testhotplug.c)
add_executable(testrumble testrumble.c)
add_executable(testthread testthread.c)
add_executable(testiconv testiconv.c testutils.c)
add_executable(testime testime.c testutils.c)
add_executable(testjoystick testjoystick.c)
add_executable(testkeys testkeys.c)
add_executable(testloadso testloadso.c)
add_executable(testlocale testlocale.c)
add_executable(testlock testlock.c)
add_executable(testmouse testmouse.c)

if(APPLE)
    add_executable(testnative testnative.c
                              testnativecocoa.m
                              testnativex11.c
                              testutils.c)
elseif(WINDOWS)
    add_executable(testnative testnative.c testnativew32.c testutils.c)
elseif(SDL_X11)
    add_executable(testnative testnative.c testnativex11.c testutils.c)
    target_link_libraries(testnative X11)
endif()

add_executable(testoverlay2 testoverlay2.c testyuv_cvt.c testutils.c)
add_executable(testplatform testplatform.c)
add_executable(testpower testpower.c)
add_executable(testfilesystem testfilesystem.c)
add_executable(testrendertarget testrendertarget.c testutils.c)
add_executable(testscale testscale.c testutils.c)
add_executable(testsem testsem.c)
add_executable(testsensor testsensor.c)
add_executable(testshader testshader.c)
add_executable(testshape testshape.c)
add_executable(testsprite2 testsprite2.c testutils.c)
add_executable(testspriteminimal testspriteminimal.c testutils.c)
add_executable(teststreaming teststreaming.c testutils.c)
add_executable(testtimer testtimer.c)
add_executable(testurl testurl.c)
add_executable(testver testver.c)
add_executable(testviewport testviewport.c testutils.c)
add_executable(testwm2 testwm2.c)
add_executable(testyuv testyuv.c testyuv_cvt.c)
add_executable(torturethread torturethread.c)
add_executable(testrendercopyex testrendercopyex.c testutils.c)
add_executable(testmessage testmessage.c)
add_executable(testdisplayinfo testdisplayinfo.c)
add_executable(testqsort testqsort.c)
add_executable(testbounds testbounds.c)
add_executable(testcustomcursor testcustomcursor.c)
add_executable(controllermap controllermap.c testutils.c)
add_executable(testvulkan testvulkan.c)
add_executable(testoffscreen testoffscreen.c)

SET(ALL_TESTS
    checkkeys
    checkkeysthreads
    controllermap
    loopwave
    loopwavequeue
    testatomic
    testaudiocapture
    testaudiohotplug
    testaudioinfo
    testautomation
    testbounds
    testcustomcursor
    testdisplayinfo
    testdraw2
    testdrawchessboard
    testdropfile
    testerror
    testfile
    testfilesystem
    testgamecontroller
    testgeometry
    testgesture
    testgl2
    testgles
    testgles2
    testguid
    testhaptic
    testhittesting
    testhotplug
    testiconv
    testime
    testintersections
    testjoystick
    testkeys
    testloadso
    testlocale
    testlock
    testmessage
    testmouse
    testmultiaudio
    testoffscreen
    testoverlay2
    testplatform
    testpower
    testqsort
    testrelative
    testrendercopyex
    testrendertarget
    testresample
    testrumble
    testscale
    testsem
    testsensor
    testshader
    testshape
    testsprite2
    testspriteminimal
    teststreaming
    testsurround
    testthread
    testtimer
    testurl
    testver
    testviewport
    testvulkan
    testwm2
    testyuv
    torturethread
)

set(NONINTERACTIVE
    testatomic
    testerror
    testfilesystem
    testguid
    testlocale
    testplatform
    testpower
    testqsort
    testthread
    testtimer
    testver
)

if(WINDOWS OR APPLE OR SDL_X11)
    list(APPEND ALL_TESTS testnative)
endif()

if(LINUX)
    list(APPEND ALL_TESTS testevdev)
    list(APPEND NONINTERACTIVE testevdev)
endif()

if(SDL_DUMMYAUDIO)
  set(NEEDS_AUDIO
    testaudioinfo
    testsurround
  )
endif()

if(SDL_DUMMYVIDEO)
  set(NEEDS_DISPLAY
    testkeys
    testbounds
    testdisplayinfo
  )
endif()

if(OPENGL_FOUND)
    target_link_libraries(testshader OpenGL::GL)
    target_link_libraries(testgl2 OpenGL::GL)
endif()

file(GLOB RESOURCE_FILES *.bmp *.wav *.hex moose.dat utf8.txt)
file(COPY ${RESOURCE_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
if(PSP OR PS2)
    set(NEEDS_RESOURCES
        testscale
        testrendercopyex
        controllermap
        testyuv
        testgamecontroller
        testshape
        testshader
        testspriteminimal
        testautomation
        testrendertarget
        testsprite2
        loopwave
        loopwavequeue
        testresample
        testaudiohotplug
        testmultiaudio
        testiconv
        testoverlay2
        teststreaming
        testviewport
    )
else()
    set(NEEDS_RESOURCES
        testscale
        testrendercopyex
        controllermap
        testyuv
        testgamecontroller
        testshape
        testshader
        testspriteminimal
        testautomation
        testcustomcursor
        testrendertarget
        testsprite2
        loopwave
        loopwavequeue
        testresample
        testaudiohotplug
        testmultiaudio
        testime
        testiconv
        testoverlay2
        teststreaming
        testviewport
    )
    if(WINDOWS OR APPLE OR SDL_X11)
        list(APPEND NEEDS_RESOURCES testnative)
    endif()
endif()

if(PSP)
    # Build EBOOT files if building for PSP
    set(BUILD_EBOOT
        ${NEEDS_RESOURCES}
        testatomic
        testaudiocapture
        testaudioinfo
        testbounds
        testdisplayinfo
        testdraw2
        testdrawchessboard
        testerror
        testfile
        testfilesystem
        testgeometry
        testgl2
        testguid
        testhittesting
        testiconv
        testintersections
        testjoystick
        testlock
        testmessage
        testoverlay2
        testplatform
        testpower
        testqsort
        testsem
        teststreaming
        testsurround
        testthread
        testtimer
        testver
        testviewport
        testwm2
        torturethread
    )
    foreach(APP IN LISTS BUILD_EBOOT)
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
        if(${BUILD_PRX})
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

if(RISCOS)
    set(ALL_TESTS_AIF "")
    foreach(APP IN LISTS ALL_TESTS)
        target_link_options(${APP} PRIVATE -static)
        add_custom_command(
            OUTPUT ${APP},ff8
            COMMAND elf2aif ${APP} ${APP},ff8
            DEPENDS ${APP}
        )
        add_custom_target(${APP}-aif ALL DEPENDS ${APP},ff8)
        list(APPEND ALL_TESTS_AIF ${CMAKE_CURRENT_BINARY_DIR}/${APP},ff8)
    endforeach()
endif()

foreach(APP IN LISTS NEEDS_RESOURCES)
    foreach(RESOURCE_FILE ${RESOURCE_FILES})
        if(PSP OR PS2)
            add_custom_command(TARGET ${APP} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${RESOURCE_FILE} $<TARGET_FILE_DIR:${APP}>/sdl-${APP})
        else()
            add_custom_command(TARGET ${APP} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${RESOURCE_FILE} $<TARGET_FILE_DIR:${APP}>)
        endif()
    endforeach(RESOURCE_FILE)
    if(APPLE)
        # Make sure resource files get installed into macOS/iOS .app bundles.  
        target_sources(${APP} PRIVATE "${RESOURCE_FILES}")
        set_target_properties(${APP} PROPERTIES RESOURCE "${RESOURCE_FILES}")
    endif()
endforeach()

# Set Apple App ID / Bundle ID.  This is needed to launch apps on some Apple
# platforms (iOS, for example).
if(APPLE)
    if(${CMAKE_VERSION} VERSION_LESS "3.7.0")
        # CMake's 'BUILDSYSTEM_TARGETS' property is only available in
        # CMake 3.7 and above.
        message(WARNING "Unable to set Bundle ID for Apple .app builds due to old CMake (pre 3.7).")
    else()
        get_property(TARGETS DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY BUILDSYSTEM_TARGETS)
        foreach(CURRENT_TARGET IN LISTS TARGETS)
            get_property(TARGET_TYPE TARGET ${CURRENT_TARGET} PROPERTY TYPE)
            if(TARGET_TYPE STREQUAL "EXECUTABLE")
                set_target_properties("${CURRENT_TARGET}" PROPERTIES
                    MACOSX_BUNDLE_GUI_IDENTIFIER "org.libsdl.${CURRENT_TARGET}"
                    MACOSX_BUNDLE_BUNDLE_VERSION "${SDL_VERSION}"
                    MACOSX_BUNDLE_SHORT_VERSION_STRING "${SDL_VERSION}"
                )
            endif()
        endforeach()
    endif()
endif()

set(TESTS_ENVIRONMENT
    SDL_AUDIODRIVER=dummy
    SDL_VIDEODRIVER=dummy
)

foreach(TESTCASE ${NONINTERACTIVE} ${NEEDS_AUDIO} ${NEEDS_DISPLAY})
    add_test(
        NAME ${TESTCASE}
        COMMAND ${TESTCASE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    set_tests_properties(
        ${TESTCASE}
        PROPERTIES ENVIRONMENT "${TESTS_ENVIRONMENT}"
    )
    if(SDL_INSTALL_TESTS)
        set(exe ${TESTCASE})
        set(installedtestsdir "${CMAKE_INSTALL_FULL_LIBEXECDIR}/installed-tests/SDL2")
        configure_file(template.test.in "${exe}.test" @ONLY)
        install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/${exe}.test"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/installed-tests/SDL2
        )
    endif()
endforeach()

if(SDL_INSTALL_TESTS)
    if(RISCOS)
        install(
            FILES ${ALL_TESTS_AIF}
            DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL2
        )
    else()
        install(
            TARGETS ${ALL_TESTS}
            DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL2
        )
    endif()
    install(
        FILES ${RESOURCE_FILES}
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL2
    )
endif()
