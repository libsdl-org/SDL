cmake_minimum_required(VERSION 3.0.0)
project(SDL2 C)

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
    find_package(OpenGL)
endif()

if (OPENGL_FOUND)
add_definitions(-DHAVE_OPENGL)
endif()

add_executable(checkkeys checkkeys.c)
add_executable(checkkeysthreads checkkeysthreads.c)
add_executable(loopwave loopwave.c)
add_executable(loopwavequeue loopwavequeue.c)
add_executable(testsurround testsurround.c)
add_executable(testresample testresample.c)
add_executable(testaudioinfo testaudioinfo.c)

file(GLOB TESTAUTOMATION_SOURCE_FILES testautomation*.c)
add_executable(testautomation ${TESTAUTOMATION_SOURCE_FILES})

add_executable(testmultiaudio testmultiaudio.c)
add_executable(testaudiohotplug testaudiohotplug.c)
add_executable(testaudiocapture testaudiocapture.c)
add_executable(testatomic testatomic.c)
add_executable(testintersections testintersections.c)
add_executable(testrelative testrelative.c)
add_executable(testhittesting testhittesting.c)
add_executable(testdraw2 testdraw2.c)
add_executable(testdrawchessboard testdrawchessboard.c)
add_executable(testdropfile testdropfile.c)
add_executable(testerror testerror.c)
add_executable(testfile testfile.c)
add_executable(testgamecontroller testgamecontroller.c)
add_executable(testgeometry testgeometry.c)
add_executable(testgesture testgesture.c)
add_executable(testgl2 testgl2.c)
add_executable(testgles testgles.c)
add_executable(testgles2 testgles2.c)
add_executable(testhaptic testhaptic.c)
add_executable(testhotplug testhotplug.c)
add_executable(testrumble testrumble.c)
add_executable(testthread testthread.c)
add_executable(testiconv testiconv.c)
add_executable(testime testime.c)
add_executable(testjoystick testjoystick.c)
add_executable(testkeys testkeys.c)
add_executable(testloadso testloadso.c)
add_executable(testlock testlock.c)
add_executable(testmouse testmouse.c)

if(APPLE)
    add_executable(testnative testnative.c
                              testnativecocoa.m
                              testnativex11.c)
elseif(WINDOWS)
    add_executable(testnative testnative.c testnativew32.c)
elseif(UNIX)
    add_executable(testnative testnative.c testnativex11.c)
endif()

add_executable(testoverlay2 testoverlay2.c testyuv_cvt.c)
add_executable(testplatform testplatform.c)
add_executable(testpower testpower.c)
add_executable(testfilesystem testfilesystem.c)
add_executable(testrendertarget testrendertarget.c)
add_executable(testscale testscale.c)
add_executable(testsem testsem.c)
add_executable(testshader testshader.c)
add_executable(testshape testshape.c)
add_executable(testsprite2 testsprite2.c)
add_executable(testspriteminimal testspriteminimal.c)
add_executable(teststreaming teststreaming.c)
add_executable(testtimer testtimer.c)
add_executable(testver testver.c)
add_executable(testviewport testviewport.c)
add_executable(testwm2 testwm2.c)
add_executable(testyuv testyuv.c testyuv_cvt.c)
add_executable(torturethread torturethread.c)
add_executable(testrendercopyex testrendercopyex.c)
add_executable(testmessage testmessage.c)
add_executable(testdisplayinfo testdisplayinfo.c)
add_executable(testqsort testqsort.c)
add_executable(testbounds testbounds.c)
add_executable(testcustomcursor testcustomcursor.c)
add_executable(controllermap controllermap.c)
add_executable(testvulkan testvulkan.c)
add_executable(testoffscreen testoffscreen.c)

if(OPENGL_FOUND)
add_dependencies(testshader OpenGL::GL)
add_dependencies(testgl2 OpenGL::GL)
target_link_libraries(testshader OpenGL::GL)
target_link_libraries(testgl2 OpenGL::GL)
endif()

file(GLOB RESOURCE_FILES *.bmp *.wav *.hex moose.dat utf8.txt)
file(COPY ${RESOURCE_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
if(PSP)
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
        testnative
        testiconv
        testoverlay2
        teststreaming
        testviewport
    )
endif()

if(PSP)
    # Build EBOOT files if building for PSP
    set(BUILD_EBOOT
        ${NEEDS_RESOURCES}
        testbounds
        testgl2
        testsem
        testdisplayinfo
        teststreaming
        testgeometry
        testfile
        testdraw2
        testviewport
        testhittesting
        testoverlay2
        testver
        testdrawchessboard
        testsurround
        testintersections
        testmessage
        testaudiocapture
        testerror
        testatomic
        testjoystick
        testiconv
        testfilesystem
        testplatform
        testthread
        testloadso
        testqsort
        testaudioinfo
        testlock
        testtimer
        testpower
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

foreach(APP IN LISTS NEEDS_RESOURCES)
    foreach(RESOURCE_FILE ${RESOURCE_FILES})
        if(PSP)
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
