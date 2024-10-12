cmake_minimum_required(VERSION 3.0...3.5)
project(SDL2_test)

include(CheckCCompilerFlag)
include(CMakeParseArguments)
include(CMakePushCheckState)

set(SDL_TEST_EXECUTABLES)
set(SDL_TESTS_NONINTERACTIVE)
set(SDL_TESTS_NEEDS_RESOURCES)

macro(add_sdl_test_executable TARGET)
    cmake_parse_arguments(AST "NONINTERACTIVE;NEEDS_RESOURCES" "" "" ${ARGN})
    if(ANDROID)
        add_library(${TARGET} SHARED ${AST_UNPARSED_ARGUMENTS})
    else()
        add_executable(${TARGET} ${AST_UNPARSED_ARGUMENTS})
    endif()

    list(APPEND SDL_TEST_EXECUTABLES ${TARGET})
    if(AST_NONINTERACTIVE)
        list(APPEND SDL_TESTS_NONINTERACTIVE ${TARGET})
    endif()
    if(AST_NEEDS_RESOURCES)
        list(APPEND SDL_TESTS_NEEDS_RESOURCES ${TARGET})
    endif()

    if(HAVE_GCC_WDOCUMENTATION)
        target_compile_options(${TARGET} PRIVATE "-Wdocumentation")
        if(HAVE_GCC_WERROR_DOCUMENTATION)
            target_compile_options(${TARGET} PRIVATE "-Werror=documentation")
        endif()
    endif()

    if(HAVE_GCC_WDOCUMENTATION_UNKNOWN_COMMAND)
        if(SDL_WERROR)
            if(HAVE_GCC_WERROR_DOCUMENTATION_UNKNOWN_COMMAND)
                target_compile_options(${TARGET} PRIVATE "-Werror=documentation-unknown-command")
            endif()
        endif()
        target_compile_options(${TARGET} PRIVATE "-Wdocumentation-unknown-command")
    endif()

    if(HAVE_GCC_COMMENT_BLOCK_COMMANDS)
        target_compile_options(${TARGET} PRIVATE "-fcomment-block-commands=threadsafety")
        target_compile_options(${TARGET} PRIVATE "-fcomment-block-commands=deprecated")
    else()
        if(HAVE_CLANG_COMMENT_BLOCK_COMMANDS)
            target_compile_options(${TARGET} PRIVATE "/clang:-fcomment-block-commands=threadsafety")
            target_compile_options(${TARGET} PRIVATE "/clang:-fcomment-block-commands=deprecated")
        endif()
    endif()

    if(USE_GCC OR USE_CLANG)
        check_c_compiler_flag(-fno-fast-math HAVE_GCC_FNO_FAST_MATH)
        if(HAVE_GCC_FNO_FAST_MATH)
            target_compile_options(${TARGET} PRIVATE -fno-fast-math)
        endif()
    endif()
endmacro()

if(NOT TARGET SDL2::SDL2-static)
    find_package(SDL2 2.0.23 REQUIRED COMPONENTS SDL2-static SDL2test)
endif()

enable_testing()

if(SDL_INSTALL_TESTS)
    include(GNUInstallDirs)
endif()

if(N3DS)
    link_libraries(SDL2::SDL2main)
endif()

if(PSP)
    link_libraries(
        SDL2::SDL2main
        SDL2::SDL2test
        SDL2::SDL2-static
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
elseif(PS2)
    link_libraries(
        SDL2main
        SDL2_test
        SDL2-static
        patches
        gskit
        dmakit
        ps2_drivers
    )
else()
    link_libraries(SDL2::SDL2test SDL2::SDL2-static)
endif()

if(WINDOWS)
    # mingw32 must come before SDL2main to link successfully
    if(MINGW OR CYGWIN)
        link_libraries(mingw32)
    endif()

    # CET support was added in VS 16.7
    if(MSVC_VERSION GREATER 1926 AND CMAKE_GENERATOR_PLATFORM MATCHES "Win32|x64")
        link_libraries(-CETCOMPAT)
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

add_sdl_test_executable(checkkeys checkkeys.c)
add_sdl_test_executable(checkkeysthreads checkkeysthreads.c)
add_sdl_test_executable(loopwave NEEDS_RESOURCES loopwave.c testutils.c)
add_sdl_test_executable(loopwavequeue NEEDS_RESOURCES loopwavequeue.c testutils.c)
add_sdl_test_executable(testsurround testsurround.c)
add_sdl_test_executable(testresample NEEDS_RESOURCES testresample.c)
add_sdl_test_executable(testaudioinfo testaudioinfo.c)

file(GLOB TESTAUTOMATION_SOURCE_FILES testautomation*.c)
add_sdl_test_executable(testautomation NONINTERACTIVE NEEDS_RESOURCES ${TESTAUTOMATION_SOURCE_FILES})
add_sdl_test_executable(testmultiaudio NEEDS_RESOURCES testmultiaudio.c testutils.c)
add_sdl_test_executable(testaudiohotplug NEEDS_RESOURCES testaudiohotplug.c testutils.c)
add_sdl_test_executable(testaudiocapture testaudiocapture.c)
add_sdl_test_executable(testatomic NONINTERACTIVE testatomic.c)
add_sdl_test_executable(testintersections testintersections.c)
add_sdl_test_executable(testrelative testrelative.c)
add_sdl_test_executable(testhittesting testhittesting.c)
add_sdl_test_executable(testdraw2 testdraw2.c)
add_sdl_test_executable(testdrawchessboard testdrawchessboard.c)
add_sdl_test_executable(testdropfile testdropfile.c)
add_sdl_test_executable(testerror NONINTERACTIVE testerror.c)

if(LINUX)
    add_sdl_test_executable(testevdev NONINTERACTIVE testevdev.c)
endif()

add_sdl_test_executable(testfile testfile.c)
add_sdl_test_executable(testgamecontroller NEEDS_RESOURCES testgamecontroller.c testutils.c)
add_sdl_test_executable(testgeometry testgeometry.c testutils.c)
add_sdl_test_executable(testgesture testgesture.c)
add_sdl_test_executable(testgl2 testgl2.c)
add_sdl_test_executable(testgles testgles.c)
add_sdl_test_executable(testgles2 testgles2.c)
add_sdl_test_executable(testgles2_sdf NEEDS_RESOURCES testgles2_sdf.c testutils.c)
add_sdl_test_executable(testhaptic testhaptic.c)
add_sdl_test_executable(testhotplug testhotplug.c)
add_sdl_test_executable(testrumble testrumble.c)
add_sdl_test_executable(testthread NONINTERACTIVE testthread.c)
add_sdl_test_executable(testiconv NEEDS_RESOURCES testiconv.c testutils.c)
add_sdl_test_executable(testime NEEDS_RESOURCES testime.c testutils.c)
add_sdl_test_executable(testjoystick testjoystick.c)
add_sdl_test_executable(testkeys testkeys.c)
add_sdl_test_executable(testloadso testloadso.c)
add_sdl_test_executable(testlocale NONINTERACTIVE testlocale.c)
add_sdl_test_executable(testlock testlock.c)
add_sdl_test_executable(testmouse testmouse.c)

if(APPLE)
    add_sdl_test_executable(testnative NEEDS_RESOURCES
        testnative.c
        testnativecocoa.m
        testnativex11.c
        testutils.c
    )

    cmake_push_check_state(RESET)
    check_c_compiler_flag(-Wno-error=deprecated-declarations HAVE_WNO_ERROR_DEPRECATED_DECLARATIONS)
    cmake_pop_check_state()
    if(HAVE_WNO_ERROR_DEPRECATED_DECLARATIONS)
        set_property(SOURCE "testnativecocoa.m" APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-error=deprecated-declarations")
    endif()
elseif(WINDOWS)
    add_sdl_test_executable(testnative NEEDS_RESOURCES testnative.c testnativew32.c testutils.c)
elseif(HAVE_X11)
    add_sdl_test_executable(testnative NEEDS_RESOURCES testnative.c testnativex11.c testutils.c)
    target_link_libraries(testnative X11)
elseif(OS2)
    add_sdl_test_executable(testnative NEEDS_RESOURCES testnative.c testnativeos2.c testutils.c)
endif()

add_sdl_test_executable(testoverlay2 NEEDS_RESOURCES testoverlay2.c testyuv_cvt.c testutils.c)
add_sdl_test_executable(testplatform NONINTERACTIVE testplatform.c)
add_sdl_test_executable(testpower NONINTERACTIVE testpower.c)
add_sdl_test_executable(testfilesystem NONINTERACTIVE testfilesystem.c)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_sdl_test_executable(testfilesystem_pre NONINTERACTIVE testfilesystem_pre.c)
endif()
add_sdl_test_executable(testrendertarget NEEDS_RESOURCES testrendertarget.c testutils.c)
add_sdl_test_executable(testscale NEEDS_RESOURCES testscale.c testutils.c)
add_sdl_test_executable(testsem testsem.c)
add_sdl_test_executable(testsensor testsensor.c)
add_sdl_test_executable(testshader NEEDS_RESOURCES testshader.c)
add_sdl_test_executable(testshape NEEDS_RESOURCES testshape.c)
add_sdl_test_executable(testsprite2 NEEDS_RESOURCES testsprite2.c testutils.c)
add_sdl_test_executable(testspriteminimal NEEDS_RESOURCES testspriteminimal.c testutils.c)
add_sdl_test_executable(teststreaming NEEDS_RESOURCES teststreaming.c testutils.c)
add_sdl_test_executable(testtimer NONINTERACTIVE testtimer.c)
add_sdl_test_executable(testurl testurl.c)
add_sdl_test_executable(testver NONINTERACTIVE testver.c)
add_sdl_test_executable(testviewport NEEDS_RESOURCES testviewport.c testutils.c)
add_sdl_test_executable(testwm2 testwm2.c)
add_sdl_test_executable(testyuv NEEDS_RESOURCES testyuv.c testyuv_cvt.c)
add_sdl_test_executable(torturethread torturethread.c)
add_sdl_test_executable(testrendercopyex NEEDS_RESOURCES testrendercopyex.c testutils.c)
add_sdl_test_executable(testmessage testmessage.c)
add_sdl_test_executable(testdisplayinfo testdisplayinfo.c)
add_sdl_test_executable(testqsort NONINTERACTIVE testqsort.c)
add_sdl_test_executable(testbounds testbounds.c)
add_sdl_test_executable(testcustomcursor testcustomcursor.c)
add_sdl_test_executable(controllermap NEEDS_RESOURCES controllermap.c testutils.c)
add_sdl_test_executable(testvulkan testvulkan.c)
add_sdl_test_executable(testoffscreen testoffscreen.c)

cmake_push_check_state(RESET)

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
        target_link_libraries(testshader OpenGL::GL)
        target_link_libraries(testgl2 OpenGL::GL)
    else()
        if(EMSCRIPTEN AND OPENGL_gl_LIBRARY STREQUAL "nul")
            set(OPENGL_gl_LIBRARY GL)
        endif()
        # emscripten's FindOpenGL.cmake does not create OpenGL::GL
        target_link_libraries(testshader ${OPENGL_gl_LIBRARY})
        target_link_libraries(testgl2 ${OPENGL_gl_LIBRARY})
    endif()
endif()
if(EMSCRIPTEN)
    target_link_libraries(testshader -sLEGACY_GL_EMULATION)
endif()

file(GLOB RESOURCE_FILES *.bmp *.wav *.hex moose.dat utf8.txt)
file(COPY ${RESOURCE_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if(PSP)
    # Build EBOOT files if building for PSP
    set(BUILD_EBOOT
        ${SDL_TESTS_NEEDS_RESOURCES}
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

if(N3DS)
    foreach(APP IN LISTS SDL_TEST_EXECUTABLES)
        get_target_property(TARGET_BINARY_DIR ${APP} BINARY_DIR)
        set(ROMFS_DIR "${TARGET_BINARY_DIR}/sdl-${APP}")
        set(SMDH_FILE "${TARGET_BINARY_DIR}/${APP}.smdh")
        file(MAKE_DIRECTORY ${ROMFS_DIR})
        ctr_generate_smdh("${SMDH_FILE}"
            NAME "SDL-${APP}"
            DESCRIPTION "SDL2 Test suite"
            AUTHOR "SDL2 Contributors"
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
    foreach(APP IN LISTS SDL_TEST_EXECUTABLES)
        target_link_options(${APP} PRIVATE -static)
        add_custom_command(
            OUTPUT ${APP},ff8
            COMMAND elf2aif ${APP} ${APP},ff8
            DEPENDS ${APP}
        )
        add_custom_target(${APP}-aif ALL DEPENDS ${APP},ff8)
        list(APPEND SDL_TEST_EXECUTABLES_AIF ${CMAKE_CURRENT_BINARY_DIR}/${APP},ff8)
    endforeach()
endif()

if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(test_bin_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    if(NOT IS_ABSOLUTE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        set(test_bin_dir "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()
else()
    set(test_bin_dir "${CMAKE_CURRENT_BINARY_DIR}")
endif()
if(NOT CMAKE_VERSION VERSION_LESS 3.20)
    get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    set(test_bin_dir "${test_bin_dir}$<$<BOOL:${is_multi_config}>:/$<CONFIG>>")
endif()

set(RESOURCE_FILES_BINDIR)
foreach(resource_file IN LISTS RESOURCE_FILES)
    get_filename_component(res_file_name ${resource_file} NAME)
    set(resource_file_bindir "${test_bin_dir}/${res_file_name}")
    add_custom_command(OUTPUT "${resource_file_bindir}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${resource_file}" "${resource_file_bindir}"
        DEPENDS "${resource_file}"
    )
    list(APPEND RESOURCE_FILES_BINDIR "${resource_file_bindir}")
endforeach()
add_custom_target(copy-sdl-test-resources
    DEPENDS "${RESOURCE_FILES_BINDIR}"
)

foreach(APP IN LISTS SDL_TESTS_NEEDS_RESOURCES)
    if(PSP OR PS2 OR N3DS)
        foreach(RESOURCE_FILE ${RESOURCE_FILES})
            add_custom_command(TARGET ${APP} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${RESOURCE_FILE} $<TARGET_FILE_DIR:${APP}>/sdl-${APP})
        endforeach()
    else()
        add_dependencies(${APP} copy-sdl-test-resources)
    endif()
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
        set(installedtestsdir "${CMAKE_INSTALL_FULL_LIBEXECDIR}/installed-tests/SDL2")
        configure_file(template.test.in "${exe}.test" @ONLY)
        install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/${exe}.test"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/installed-tests/SDL2
        )
    endif()
endforeach()

set_tests_properties(testautomation PROPERTIES TIMEOUT 120)
set_tests_properties(testthread PROPERTIES TIMEOUT 40)
set_tests_properties(testtimer PROPERTIES TIMEOUT 60)
if(TARGET testfilesystem_pre)
    set_property(TEST testfilesystem_pre PROPERTY TIMEOUT 60)
    set_property(TEST testfilesystem APPEND PROPERTY DEPENDS testfilesystem_pre)
endif()

if(SDL_INSTALL_TESTS)
    if(RISCOS)
        install(
            FILES ${SDL_TEST_EXECUTABLES_AIF}
            DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL2
        )
    else()
        install(
            TARGETS ${SDL_TEST_EXECUTABLES}
            DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL2
        )
    endif()
    if(MSVC)
        foreach(test ${SDL_TEST_EXECUTABLES})
            SDL_install_pdb(${test} "${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL2")
        endforeach()
    endif()
    install(
        FILES ${RESOURCE_FILES}
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL2
    )
endif()
