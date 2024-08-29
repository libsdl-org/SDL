#
# CMake script for building the SDL tests
#

cmake_minimum_required(VERSION 3.16)

set(SDL3_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")

include(CheckIncludeFile)
include(CheckStructHasMember)
include(CMakePushCheckState)
include(sdlcompilers)

find_package(Python3 COMPONENTS Interpreter)
if(NOT PYTHON3_EXECUTABLE)
    set(PYTHON3_EXECUTABLE "python3")
endif()

if(SDL_TESTS_LINK_SHARED)
    set(sdl_name_component SDL3-shared)
else()
    set(sdl_name_component SDL3-static)
endif()
set(HAVE_TESTS_LINK_SHARED "${SDL_TESTS_LINK_SHARED}" PARENT_SCOPE)

# CMake incorrectly detects opengl32.lib being present on MSVC ARM64
if(NOT (MSVC AND SDL_CPU_ARM64))
    # Prefer GLVND, if present
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL)
endif()

set(SDL_TEST_EXECUTABLES)

add_library(sdltests_utils OBJECT
    testutils.c
)
target_link_libraries(sdltests_utils PRIVATE SDL3::Headers)

file(GLOB RESOURCE_FILES *.bmp *.wav *.hex moose.dat utf8.txt)

option(SDLTEST_TRACKMEM "Run tests with --trackmem" OFF)

if(WIN32)
    option(SDLTEST_PROCDUMP "Run tests using sdlprocdump for minidump generation" OFF)
    add_executable(sdlprocdump win32/sdlprocdump.c)
    set_property(TARGET sdlprocdump PROPERTY C_STANDARD "90")
    SDL_AddCommonCompilerFlags(sdlprocdump)
    if(SDLTEST_PROCDUMP)
        set(CMAKE_TEST_LAUNCHER "$<TARGET_FILE:sdlprocdump>;--")
    else()
        set_property(TARGET sdlprocdump PROPERTY EXCLUDE_FROM_ALL "1")
    endif()
endif()

if(EMSCRIPTEN)
    set(SDLTEST_BROWSER "firefox" CACHE STRING "Browser in which to run SDL unit tests (chrome or firefox)")
    set(SDLTEST_PORT "8080" CACHE STRING "Port on which to serve the tests")
    set(SDLTEST_CHROME_BINARY "" CACHE STRING "Chrome/Chromium browser binary (optional)")
    if(TARGET Python3::Interpreter)
        add_custom_target(serve-sdl-tests
                COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/emscripten/server.py"
                "${SDLTEST_PORT}"
                -d "${CMAKE_CURRENT_BINARY_DIR}"
                --map "${SDL3_SOURCE_DIR}:/SDL")
    endif()
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

set(RESOURCE_FILE_NAMES)
set(RESOURCE_FILES_BINDIR)
foreach(resource_file IN LISTS RESOURCE_FILES)
    get_filename_component(res_file_name ${resource_file} NAME)
    list(APPEND RESOURCE_FILE_NAMES "${res_file_name}")
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

define_property(TARGET PROPERTY SDL_NONINTERACTIVE BRIEF_DOCS "If true, target is a non-interactive test executable." FULL_DOCS "If true, target is a noninteractive test executable.")
define_property(TARGET PROPERTY SDL_NONINTERACTIVE_ARGUMENTS BRIEF_DOCS "Argument(s) to run executable in non-interactive mode." FULL_DOCS "Argument(s) to run executable in non-interactive mode.")
define_property(TARGET PROPERTY SDL_NONINTERACTIVE_TIMEOUT BRIEF_DOCS "Timeout for noninteractive executable." FULL_DOCS "Timeout for noninteractive executable.")

macro(add_sdl_test_executable TARGET)
    cmake_parse_arguments(AST "BUILD_DEPENDENT;NONINTERACTIVE;NEEDS_RESOURCES;TESTUTILS;THREADS;NO_C90;MAIN_CALLBACKS;NOTRACKMEM" "" "DISABLE_THREADS_ARGS;NONINTERACTIVE_TIMEOUT;NONINTERACTIVE_ARGS;SOURCES" ${ARGN})
    if(AST_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown argument(s): ${AST_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT AST_SOURCES)
        message(FATAL_ERROR "add_sdl_test_executable needs at least one source")
    endif()
    if(AST_TESTUTILS)
        list(APPEND AST_SOURCES $<TARGET_OBJECTS:sdltests_utils>)
    endif()
    set(EXTRA_SOURCES "")
    if(AST_NEEDS_RESOURCES)
        list(APPEND EXTRA_SOURCES ${RESOURCE_FILES})
    endif()
    if(ANDROID)
        add_library(${TARGET} SHARED ${AST_SOURCES} ${EXTRA_SOURCES})
    else()
        add_executable(${TARGET} ${AST_SOURCES} ${EXTRA_SOURCES})
    endif()
    SDL_AddCommonCompilerFlags(${TARGET})
    target_include_directories(${TARGET} PRIVATE "${SDL3_SOURCE_DIR}/src/video/khronos")
    target_link_libraries(${TARGET} PRIVATE SDL3::SDL3_test SDL3::${sdl_name_component})
    if(NOT AST_NO_C90 AND NOT SDL_CMAKE_PLATFORM MATCHES "^(n3ds|ps2|psp)$")
        set_property(TARGET ${TARGET} PROPERTY C_STANDARD 90)
        set_property(TARGET ${TARGET} PROPERTY C_EXTENSIONS FALSE)
    endif()

    list(APPEND SDL_TEST_EXECUTABLES ${TARGET})
    set_property(TARGET ${TARGET} PROPERTY SDL_NOTRACKMEM ${AST_NOTRACKMEM})
    if(AST_NONINTERACTIVE)
        set_property(TARGET ${TARGET} PROPERTY SDL_NONINTERACTIVE 1)
    endif()
    set_property(TARGET ${TARGET} PROPERTY SDL_DISABLE_THREADS_ARGS "${AST_DISABLE_THREADS_ARGS}")
    set_property(TARGET ${TARGET} PROPERTY SDL_THREADS "${AST_THREADS}")
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
            add_dependencies(${TARGET} copy-sdl-test-resources)
        endif()
        if(APPLE)
            # Make sure resource files get installed into macOS/iOS .app bundles.
            set_target_properties(${TARGET} PROPERTIES RESOURCE "${RESOURCE_FILES}")
        endif()
        if(EMSCRIPTEN)
            foreach(res IN LISTS RESOURCE_FILES)
                get_filename_component(res_name "${res}" NAME)
                target_link_options(${TARGET} PRIVATE "SHELL:--embed-file ${res}@${res_name}")
                set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS "${res}")
            endforeach()
        endif()
        set_property(TARGET ${TARGET} APPEND PROPERTY ADDITIONAL_CLEAN_FILES "$<TARGET_FILE_DIR:${TARGET}>/$<JOIN:${RESOURCE_FILE_NAMES},$<SEMICOLON>$<TARGET_FILE_DIR:${TARGET}>/>")
    endif()
    if(AST_BUILD_DEPENDENT)
        target_include_directories(${TARGET} BEFORE PRIVATE $<TARGET_PROPERTY:SDL3::${sdl_name_component},INCLUDE_DIRECTORIES>)
        target_include_directories(${TARGET} BEFORE PRIVATE ${SDL3_SOURCE_DIR}/src)
        if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.20")
            target_include_directories(${TARGET} AFTER PRIVATE "${SDL3_SOURCE_DIR}/include/build_config")
        endif()
    endif()

    if(WINDOWS)
        # CET support was added in VS 16.7
        if(MSVC_VERSION GREATER 1926 AND CMAKE_GENERATOR_PLATFORM MATCHES "Win32|x64")
            set_property(TARGET ${TARGET} APPEND_STRING PROPERTY LINK_FLAGS " -CETCOMPAT")
        endif()
    elseif(PSP)
        target_link_libraries(${TARGET} PRIVATE GL)
    elseif(EMSCRIPTEN)
        set_property(TARGET ${TARGET} PROPERTY SUFFIX ".html")
        target_link_options(${TARGET} PRIVATE "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/emscripten/pre.js")
        target_link_options(${TARGET} PRIVATE "-sEXIT_RUNTIME=1")
        set_property(TARGET ${TARGET} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/emscripten/pre.js")
    endif()

    if(OPENGL_FOUND)
        target_compile_definitions(${TARGET} PRIVATE HAVE_OPENGL)
    endif()

    # FIXME: only add "${SDL3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>" + include paths of external dependencies
    target_include_directories(${TARGET} PRIVATE "$<TARGET_PROPERTY:SDL3::${sdl_name_component},INCLUDE_DIRECTORIES>")
endmacro()

check_include_file(signal.h HAVE_SIGNAL_H)
if(HAVE_SIGNAL_H)
    add_definitions(-DHAVE_SIGNAL_H)
endif()

check_include_file(libudev.h HAVE_LIBUDEV_H)
if(HAVE_LIBUDEV_H)
    add_definitions(-DHAVE_LIBUDEV_H)
endif()

set(FFmpeg_FIND_COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/FindFFmpeg.cmake")
if(FFmpeg_FOUND)
    cmake_push_check_state()
    list(APPEND CMAKE_REQUIRED_INCLUDES "${FFmpeg_AVUTIL_INCLUDE_DIRS}")
    list(APPEND CMAKE_REQUIRED_INCLUDES "${SDL3_SOURCE_DIR}/src/video/khronos")
    check_struct_has_member("AVFrame" "ch_layout" "libavutil/frame.h" LIBAVUTIL_AVFRAME_HAS_CH_LAYOUT)
    check_struct_has_member("AVVulkanFramesContext" "format" "libavutil/hwcontext_vulkan.h" LIBAVUTIL_AVFULKANFRAMESCONTEXT_HAS_FORMAT)
    cmake_pop_check_state()
endif()
if(FFmpeg_FOUND AND LIBAVUTIL_AVFRAME_HAS_CH_LAYOUT)
    add_sdl_test_executable(testffmpeg NO_C90 SOURCES testffmpeg.c testffmpeg_vulkan.c ${icon_bmp_header})
    if(LIBAVUTIL_AVFULKANFRAMESCONTEXT_HAS_FORMAT)
      target_compile_definitions(testffmpeg PRIVATE FFMPEG_VULKAN_SUPPORT)
    endif()
    if(APPLE)
        target_link_options(testffmpeg PRIVATE "-Wl,-framework,CoreVideo")
    endif()
    if(TARGET OpenGL::EGL)
        message(DEBUG "Enabling EGL support in testffmpeg")
        target_link_libraries(testffmpeg PRIVATE OpenGL::EGL)
        target_compile_definitions(testffmpeg PRIVATE HAVE_EGL)
    endif()
    target_include_directories(testffmpeg SYSTEM BEFORE PRIVATE ${SDL3_SOURCE_DIR}/src/video/khronos)
    target_link_libraries(testffmpeg PRIVATE ${FFMPEG_LIBRARIES})
else()
    message(STATUS "Can't find ffmpeg 5.1.3 or newer, skipping testffmpeg")
endif()

add_sdl_test_executable(checkkeys SOURCES checkkeys.c)
add_sdl_test_executable(loopwave NEEDS_RESOURCES TESTUTILS MAIN_CALLBACKS SOURCES loopwave.c)
add_sdl_test_executable(testsurround SOURCES testsurround.c)
add_sdl_test_executable(testresample NEEDS_RESOURCES SOURCES testresample.c)
add_sdl_test_executable(testaudioinfo SOURCES testaudioinfo.c)
add_sdl_test_executable(testaudiostreamdynamicresample NEEDS_RESOURCES TESTUTILS SOURCES testaudiostreamdynamicresample.c)

file(GLOB TESTAUTOMATION_SOURCE_FILES testautomation*.c)
add_sdl_test_executable(testautomation NONINTERACTIVE NONINTERACTIVE_TIMEOUT 120 NEEDS_RESOURCES BUILD_DEPENDENT NO_C90 SOURCES ${TESTAUTOMATION_SOURCE_FILES})
if(EMSCRIPTEN)
    target_link_options(testautomation PRIVATE -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=1gb)
endif()
add_sdl_test_executable(testmultiaudio NEEDS_RESOURCES TESTUTILS SOURCES testmultiaudio.c)
add_sdl_test_executable(testaudiohotplug NEEDS_RESOURCES TESTUTILS SOURCES testaudiohotplug.c)
add_sdl_test_executable(testaudiorecording MAIN_CALLBACKS SOURCES testaudiorecording.c)
add_sdl_test_executable(testatomic NONINTERACTIVE DISABLE_THREADS_ARGS "--no-threads" SOURCES testatomic.c)
add_sdl_test_executable(testintersections SOURCES testintersections.c)
add_sdl_test_executable(testrelative SOURCES testrelative.c)
add_sdl_test_executable(testhittesting SOURCES testhittesting.c)
add_sdl_test_executable(testdraw SOURCES testdraw.c)
add_sdl_test_executable(testdrawchessboard SOURCES testdrawchessboard.c)
add_sdl_test_executable(testdropfile MAIN_CALLBACKS SOURCES testdropfile.c)
add_sdl_test_executable(testerror NONINTERACTIVE DISABLE_THREADS_ARGS "--no-threads" SOURCES testerror.c)

set(build_options_dependent_tests )

add_sdl_test_executable(testevdev BUILD_DEPENDENT NONINTERACTIVE NO_C90 SOURCES testevdev.c)

if(MACOS)
    add_sdl_test_executable(testnative BUILD_DEPENDENT NEEDS_RESOURCES TESTUTILS
        SOURCES
            testnative.c
            testnativecocoa.m
            testnativex11.c
    )
elseif(WINDOWS)
    add_sdl_test_executable(testnative BUILD_DEPENDENT NEEDS_RESOURCES TESTUTILS SOURCES testnative.c testnativew32.c)
elseif(HAVE_X11 OR HAVE_WAYLAND)
    add_sdl_test_executable(testnative BUILD_DEPENDENT NO_C90 NEEDS_RESOURCES TESTUTILS SOURCES testnative.c)
    if(HAVE_X11)
        target_sources(testnative PRIVATE testnativex11.c)
        target_link_libraries(testnative PRIVATE X11)
    endif()
    if(HAVE_WAYLAND)
        set_property(SOURCE ${SDL3_BINARY_DIR}/wayland-generated-protocols/xdg-shell-protocol.c PROPERTY GENERATED 1)
        target_sources(testnative PRIVATE testnativewayland.c ${SDL3_BINARY_DIR}/wayland-generated-protocols/xdg-shell-protocol.c)

        # Needed to silence the documentation warning in the generated header file
        target_compile_options(testnative PRIVATE -Wno-documentation-unknown-command)
        target_link_libraries(testnative PRIVATE wayland-client)
    endif ()
endif()

function(files2headers OUTPUT)
    set(xxd "${SDL3_SOURCE_DIR}/cmake/xxd.py")
    set(inputs ${ARGN})
    set(outputs )
    foreach(input IN LISTS inputs)
        get_filename_component(file_we "${input}" NAME_WE)
        set(intermediate "${CMAKE_CURRENT_BINARY_DIR}/${file_we}.h")
        set(output "${CMAKE_CURRENT_SOURCE_DIR}/${file_we}.h")
        list(APPEND outputs "${output}")
        if(TARGET Python3::Interpreter AND NOT CMAKE_CROSSCOMPILING)
            list(APPEND outputs  "${intermediate}")
            # Don't add the 'output' header to the output, to avoid marking them as GENERATED
            # (generated files are removed when running the CLEAN target)
            add_custom_command(OUTPUT "${intermediate}"
                COMMAND Python3::Interpreter "${xxd}" -i "${CMAKE_CURRENT_SOURCE_DIR}/${input}" "-o" "${intermediate}"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${intermediate}" "${output}"
                DEPENDS "${xxd}" "${bmp}"
            )
        endif()
    endforeach()
    set(${OUTPUT} "${outputs}" PARENT_SCOPE)
endfunction()

files2headers(gamepad_image_headers
    gamepad_axis_arrow.bmp
    gamepad_axis.bmp
    gamepad_back.bmp
    gamepad_battery.bmp
    gamepad_battery_wired.bmp
    gamepad_button_background.bmp
    gamepad_button.bmp
    gamepad_button_small.bmp
    gamepad_face_abxy.bmp
    gamepad_face_bayx.bmp
    gamepad_face_sony.bmp
    gamepad_front.bmp
    gamepad_touchpad.bmp
)
files2headers(icon_bmp_header icon.bmp)
files2headers(glass_bmp_header glass.bmp)

add_sdl_test_executable(testaudio MAIN_CALLBACKS NEEDS_RESOURCES TESTUTILS SOURCES testaudio.c)
add_sdl_test_executable(testcolorspace SOURCES testcolorspace.c)
add_sdl_test_executable(testfile NONINTERACTIVE SOURCES testfile.c)
add_sdl_test_executable(testcontroller TESTUTILS SOURCES testcontroller.c gamepadutils.c ${gamepad_image_headers})
add_sdl_test_executable(testgeometry TESTUTILS SOURCES testgeometry.c)
add_sdl_test_executable(testgl SOURCES testgl.c)
add_sdl_test_executable(testgles SOURCES testgles.c)
add_sdl_test_executable(testgpu_simple_clear SOURCES testgpu_simple_clear.c)
add_sdl_test_executable(testgpu_spinning_cube SOURCES testgpu_spinning_cube.c)
if(ANDROID)
    target_link_libraries(testgles PRIVATE GLESv1_CM)
elseif(IOS OR TVOS)
    find_library(GLES_LIB OpenGLES REQUIRED)
    target_link_libraries(testgles PRIVATE "${GLES_LIB}")
endif()
check_include_file("GLES2/gl2platform.h" HAVE_GLES2_GL2PLATFORM_H)
if(HAVE_GLES2_GL2PLATFORM_H OR (TARGET SDL3-static OR SDL3-shared))
    add_sdl_test_executable(testgles2 SOURCES testgles2.c)
    add_sdl_test_executable(testgles2_sdf NEEDS_RESOURCES TESTUTILS SOURCES testgles2_sdf.c)
endif()
add_sdl_test_executable(testhaptic SOURCES testhaptic.c)
add_sdl_test_executable(testhotplug SOURCES testhotplug.c)
add_sdl_test_executable(testpen SOURCES testpen.c)
add_sdl_test_executable(testrumble SOURCES testrumble.c)
add_sdl_test_executable(testthread NONINTERACTIVE THREADS NONINTERACTIVE_TIMEOUT 40 SOURCES testthread.c)
add_sdl_test_executable(testiconv NEEDS_RESOURCES TESTUTILS SOURCES testiconv.c)
add_sdl_test_executable(testime NEEDS_RESOURCES TESTUTILS SOURCES testime.c)
add_sdl_test_executable(testkeys SOURCES testkeys.c)
add_sdl_test_executable(testloadso SOURCES testloadso.c)
add_sdl_test_executable(testlocale NONINTERACTIVE SOURCES testlocale.c)
add_sdl_test_executable(testlock NO_C90 SOURCES testlock.c)
add_sdl_test_executable(testrwlock SOURCES testrwlock.c)
add_sdl_test_executable(testmouse SOURCES testmouse.c)

add_sdl_test_executable(testoverlay NEEDS_RESOURCES TESTUTILS SOURCES testoverlay.c)
add_sdl_test_executable(testplatform NONINTERACTIVE SOURCES testplatform.c)
add_sdl_test_executable(testpower NONINTERACTIVE SOURCES testpower.c)
add_sdl_test_executable(testprocess NONINTERACTIVE THREADS NONINTERACTIVE_ARGS $<TARGET_FILE:childprocess> SOURCES testprocess.c)
add_sdl_test_executable(childprocess SOURCES childprocess.c)
add_dependencies(testprocess childprocess)
add_sdl_test_executable(testfilesystem NONINTERACTIVE SOURCES testfilesystem.c)
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_sdl_test_executable(pretest SOURCES pretest.c NONINTERACTIVE NONINTERACTIVE_TIMEOUT 60)
endif()
add_sdl_test_executable(testrendertarget NEEDS_RESOURCES TESTUTILS SOURCES testrendertarget.c)
add_sdl_test_executable(testscale NEEDS_RESOURCES TESTUTILS SOURCES testscale.c)
add_sdl_test_executable(testsem NONINTERACTIVE DISABLE_THREADS_ARGS "--no-threads" NONINTERACTIVE_ARGS 10 NONINTERACTIVE_TIMEOUT 30 SOURCES testsem.c)
add_sdl_test_executable(testsensor SOURCES testsensor.c)
add_sdl_test_executable(testshader NEEDS_RESOURCES TESTUTILS SOURCES testshader.c)
if(EMSCRIPTEN)
    target_link_options(testshader PRIVATE "-sLEGACY_GL_EMULATION")
endif()
add_sdl_test_executable(testshape NEEDS_RESOURCES SOURCES testshape.c ${glass_bmp_header})
add_sdl_test_executable(testsprite MAIN_CALLBACKS NEEDS_RESOURCES TESTUTILS SOURCES testsprite.c)
add_sdl_test_executable(testspriteminimal SOURCES testspriteminimal.c ${icon_bmp_header})
add_sdl_test_executable(testspritesurface SOURCES testspritesurface.c ${icon_bmp_header})
add_sdl_test_executable(teststreaming NEEDS_RESOURCES TESTUTILS SOURCES teststreaming.c)
add_sdl_test_executable(testtimer NONINTERACTIVE NONINTERACTIVE_ARGS --no-interactive NONINTERACTIVE_TIMEOUT 60 SOURCES testtimer.c)
add_sdl_test_executable(testurl SOURCES testurl.c)
add_sdl_test_executable(testver NONINTERACTIVE NOTRACKMEM SOURCES testver.c)
add_sdl_test_executable(testcamera MAIN_CALLBACKS SOURCES testcamera.c)
add_sdl_test_executable(testviewport NEEDS_RESOURCES TESTUTILS SOURCES testviewport.c)
add_sdl_test_executable(testwm SOURCES testwm.c)
add_sdl_test_executable(testyuv NONINTERACTIVE NONINTERACTIVE_ARGS "--automated" NEEDS_RESOURCES TESTUTILS SOURCES testyuv.c testyuv_cvt.c)
add_sdl_test_executable(torturethread NONINTERACTIVE THREADS NONINTERACTIVE_TIMEOUT 30 SOURCES torturethread.c)
add_sdl_test_executable(testrendercopyex NEEDS_RESOURCES TESTUTILS SOURCES testrendercopyex.c)
add_sdl_test_executable(testmessage SOURCES testmessage.c)
add_sdl_test_executable(testdisplayinfo SOURCES testdisplayinfo.c)
add_sdl_test_executable(testqsort NONINTERACTIVE SOURCES testqsort.c)
add_sdl_test_executable(testbounds NONINTERACTIVE SOURCES testbounds.c)
add_sdl_test_executable(testcustomcursor SOURCES testcustomcursor.c)
add_sdl_test_executable(testvulkan NO_C90 SOURCES testvulkan.c)
add_sdl_test_executable(testoffscreen SOURCES testoffscreen.c)
add_sdl_test_executable(testpopup SOURCES testpopup.c)
add_sdl_test_executable(testdialog SOURCES testdialog.c)
add_sdl_test_executable(testtime SOURCES testtime.c)
add_sdl_test_executable(testmanymouse SOURCES testmanymouse.c)
add_sdl_test_executable(testmodal SOURCES testmodal.c)

if (HAVE_WAYLAND)
    # Set the GENERATED property on the protocol file, since it is first created at build time
    set_property(SOURCE ${SDL3_BINARY_DIR}/wayland-generated-protocols/xdg-shell-protocol.c PROPERTY GENERATED 1)
    add_sdl_test_executable(testwaylandcustom NO_C90 NEEDS_RESOURCES SOURCES testwaylandcustom.c ${SDL3_BINARY_DIR}/wayland-generated-protocols/xdg-shell-protocol.c)
    # Needed to silence the documentation warning in the generated header file
    target_compile_options(testwaylandcustom PRIVATE -Wno-documentation-unknown-command)
    target_link_libraries(testwaylandcustom PRIVATE wayland-client)
endif()

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
    string(APPEND CMAKE_REQUIRED_FLAGS " -Wformat")
endif()
check_c_compiler_flag(-Wformat-extra-args HAVE_WFORMAT_EXTRA_ARGS)
cmake_pop_check_state()
if(HAVE_WFORMAT_EXTRA_ARGS)
    target_compile_definitions(testautomation PRIVATE HAVE_WFORMAT_EXTRA_ARGS)
endif()

if(SDL_DUMMYAUDIO)
    set_property(TARGET testaudioinfo PROPERTY SDL_NONINTERACTIVE 1)
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
if(MACOS)
    target_link_options(testnative PRIVATE "-Wl,-framework,Cocoa")
endif()
if(APPLE)
    cmake_push_check_state()
    check_c_compiler_flag(-Wno-error=deprecated-declarations HAVE_WNO_ERROR_DEPRECATED_DECLARATIONS)
    cmake_pop_check_state()
    if(HAVE_WNO_ERROR_DEPRECATED_DECLARATIONS)
        set_property(SOURCE "testnativecocoa.m" APPEND PROPERTY COMPILE_OPTIONS "-Wno-error=deprecated-declarations")
        set_property(TARGET testgles APPEND PROPERTY COMPILE_OPTIONS "-Wno-error=deprecated-declarations")
    endif()
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
  foreach(CURRENT_TARGET ${SDL_TEST_EXECUTABLES})
    set_target_properties("${CURRENT_TARGET}" PROPERTIES
      MACOSX_BUNDLE_GUI_IDENTIFIER "org.libsdl.${CURRENT_TARGET}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${SDL3_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${SDL3_VERSION}"
    )
  endforeach()
endif()

set(SDLTEST_TIMEOUT_MULTIPLIER "1" CACHE STRING "SDL test time-out multiplier")

set(SDLTEST_AUDIO_DRIVER_DEFAULT "dummy")
set(SDLTEST_VIDEO_DRIVER_DEFAULT "dummy")
if(EMSCRIPTEN)
    set(SDLTEST_AUDIO_DRIVER_DEFAULT "emscripten")
    set(SDLTEST_VIDEO_DRIVER_DEFAULT "emscripten")
endif()
set(SDLTEST_AUDIO_DRIVER "${SDLTEST_AUDIO_DRIVER_DEFAULT}" CACHE STRING "SDL audio driver for CTest")
set(SDLTEST_VIDEO_DRIVER "${SDLTEST_VIDEO_DRIVER_DEFAULT}" CACHE STRING "SDL video driver for CTest")

set(TESTS_ENVIRONMENT
    "SDL_AUDIO_DRIVER=${SDLTEST_AUDIO_DRIVER}"
    "SDL_VIDEO_DRIVER=${SDLTEST_VIDEO_DRIVER}"
    "SDL_ASSERT=abort"
)

function(add_sdl_test TEST TARGET)
    cmake_parse_arguments(ast "INSTALL" "" "" ${ARGN})
    get_property(noninteractive TARGET ${TARGET} PROPERTY SDL_NONINTERACTIVE)
    if(noninteractive)
        if(EMSCRIPTEN)
            set(command "${PYTHON3_EXECUTABLE};${CMAKE_CURRENT_SOURCE_DIR}/emscripten/driver.py;--server;http://localhost:${SDLTEST_PORT};--browser;${SDLTEST_BROWSER}")
            if(SDLTEST_CHROME_BINARY)
                list(APPEND command "--chrome-binary;${SDLTEST_CHROME_BINARY}")
            endif()
            list(APPEND command "--;${TARGET}")
        else()
            set(command ${TARGET})
        endif()
        get_property(noninteractive_arguments TARGET ${TARGET} PROPERTY SDL_NONINTERACTIVE_ARGUMENTS)
        get_property(disable_threads_args TARGET ${TARGET} PROPERTY SDL_DISABLE_THREADS_ARGS)
        get_property(uses_threads TARGET ${TARGET} PROPERTY SDL_THREADS)
        if(noninteractive_arguments)
            list(APPEND command ${noninteractive_arguments})
        endif()
        if(SDLTEST_TRACKMEM)
            get_property(notrackmem TARGET ${TARGET} PROPERTY SDL_NOTRACKMEM)
            if(NOT notrackmem)
                list(APPEND command --trackmem)
            endif()
        endif()
        if(EMSCRIPTEN)
            list(APPEND command ${disable_threads_args})
        endif()
        add_test(
            NAME ${TEST}
            COMMAND ${command}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
        if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.27")
            set_property(TEST ${TEST} APPEND PROPERTY ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_RUNTIME_DLL_DIRS:${TARGET}>")
        endif()
        if(NOT notrackmem)
            set_property(TEST ${TEST} PROPERTY FAIL_REGULAR_EXPRESSION "Total: [0-9]+\\.[0-9]+ Kb in [1-9][0-9]* allocations")
        endif()
        set_tests_properties(${TEST} PROPERTIES ENVIRONMENT "${TESTS_ENVIRONMENT}")
        if(EMSCRIPTEN AND uses_threads)
            set_tests_properties(${TEST} PROPERTIES DISABLED 1)
        endif()
        get_property(noninteractive_timeout TARGET ${TARGET} PROPERTY SDL_NONINTERACTIVE_TIMEOUT)
        if(NOT noninteractive_timeout)
            set(noninteractive_timeout 10)
        endif()
        math(EXPR noninteractive_timeout "${noninteractive_timeout}*${SDLTEST_TIMEOUT_MULTIPLIER}")
        set_tests_properties(${TEST} PROPERTIES TIMEOUT "${noninteractive_timeout}")
        if(ast_INSTALL AND SDL_INSTALL_TESTS)
            set(exe ${TARGET})
            set(installedtestsdir "${CMAKE_INSTALL_FULL_LIBEXECDIR}/installed-tests/SDL3")
            configure_file(template.test.in "${exe}.test" @ONLY)
            install(
                    FILES "${CMAKE_CURRENT_BINARY_DIR}/${exe}.test"
                    DESTINATION ${CMAKE_INSTALL_DATADIR}/installed-tests/SDL3
            )
        endif()
        if(TARGET pretest AND NOT "${TARGET}" MATCHES "pretest")
            set_property(TEST ${TEST} APPEND PROPERTY DEPENDS pretest)
        endif()
    endif()
endfunction()

foreach(TARGET ${SDL_TEST_EXECUTABLES})
    add_sdl_test(${TARGET} ${TARGET} INSTALL)
endforeach()

if(NOT EMSCRIPTEN)
    add_sdl_test(testautomation-no-simd testautomation)
    add_sdl_test(testplatform-no-simd testplatform)
    set_property(TEST testautomation-no-simd testplatform-no-simd APPEND PROPERTY ENVIRONMENT "SDL_CPU_FEATURE_MASK=-all")

    # testautomation creates temporary files which might conflict
    set_property(TEST testautomation-no-simd testautomation PROPERTY RUN_SERIAL TRUE)
endif()

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
    if(MSVC)
        foreach(test IN LISTS SDL_TEST_EXECUTABLES)
            SDL_install_pdb(${test} "${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL3")
        endforeach()
    endif()
    install(
        FILES ${RESOURCE_FILES}
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-tests/SDL3
    )
endif()

if(ANDROID AND TARGET SDL3::Jar)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake/android")
    find_package(SdlAndroid MODULE)
    if(SdlAndroid_FOUND)
        set(apks "")
        set(packages "")

        include(SdlAndroidFunctions)
        sdl_create_android_debug_keystore(SDL_test-debug-keystore)
        sdl_android_compile_resources(SDL_test-resources RESFOLDER android/res)
        add_custom_target(sdl-test-apks)
        foreach(TEST ${SDL_TEST_EXECUTABLES})
            set(ANDROID_MANIFEST_APP_NAME "${TEST}")
            set(ANDROID_MANIFEST_LABEL "${TEST}")
            set(ANDROID_MANIFEST_LIB_NAME "$<TARGET_FILE_BASE_NAME:${TEST}>")
            set(ANDROID_MANIFEST_PACKAGE "org.libsdl.sdl.test.${TEST}")
            set(generated_manifest_path "${CMAKE_CURRENT_BINARY_DIR}/android/${TEST}-src/AndroidManifest.xml")
            string(REPLACE "." "/" JAVA_PACKAGE_DIR "${ANDROID_MANIFEST_PACKAGE}")
            set(GENERATED_SRC_FOLDER "${CMAKE_CURRENT_BINARY_DIR}/android/${TEST}-src")
            set(GENERATED_RES_FOLDER "${GENERATED_SRC_FOLDER}/res")
            set(JAVA_PACKAGE_DIR "${GENERATED_SRC_FOLDER}/${JAVA_PACKAGE_DIR}")
            configure_file(android/cmake/SDLEntryTestActivity.java.cmake "${JAVA_PACKAGE_DIR}/SDLEntryTestActivity.java" @ONLY)
            configure_file(android/cmake/SDLTestActivity.java.cmake "${JAVA_PACKAGE_DIR}/SDLTestActivity.java" @ONLY)
            configure_file(android/cmake/res/values/strings.xml.cmake android/res/values/strings-${TEST}.xml @ONLY)
            configure_file(android/cmake/res/xml/shortcuts.xml.cmake "${GENERATED_RES_FOLDER}/xml/shortcuts.xml" @ONLY)
            configure_file(android/cmake/AndroidManifest.xml.cmake "${generated_manifest_path}" @ONLY)
            file(GENERATE
                OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/android/${TEST}-$<CONFIG>/res/values/strings.xml"
                INPUT "${CMAKE_CURRENT_BINARY_DIR}/android/res/values/strings-${TEST}.xml"
            )

            sdl_android_compile_resources(${TEST}-resources
                RESOURCES
                    "${CMAKE_CURRENT_BINARY_DIR}/android/${TEST}-$<CONFIG>/res/values/strings.xml"
                    "${GENERATED_RES_FOLDER}/xml/shortcuts.xml"
            )

            sdl_android_link_resources(${TEST}-apk-linked
                MANIFEST "${generated_manifest_path}"
                PACKAGE ${ANDROID_MANIFEST_PACKAGE}
                RES_TARGETS SDL_test-resources ${TEST}-resources
                TARGET_SDK_VERSION 31
            )

            set(CMAKE_JAVA_COMPILE_FLAGS "-encoding;utf-8")
            set(classes_path "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${TEST}-java.dir/classes")
            # Some CMake versions have a slow `cmake -E make_directory` implementation
            if(NOT IS_DIRECTORY "${classes_path}")
                execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${classes_path}")
            endif()
            set(OUT_JAR "${CMAKE_CURRENT_BINARY_DIR}/${TEST}.jar")
            add_custom_command(
                OUTPUT "${OUT_JAR}"
                COMMAND ${CMAKE_COMMAND} -E rm -rf "${classes_path}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${classes_path}"
                COMMAND ${Java_JAVAC_EXECUTABLE}
                    -source 1.8 -target 1.8
                    -bootclasspath "$<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>"
                    "${JAVA_PACKAGE_DIR}/SDLEntryTestActivity.java"
                    "${JAVA_PACKAGE_DIR}/SDLTestActivity.java"
                    $<TARGET_PROPERTY:${TEST}-apk-linked,JAVA_R>
                    -cp "$<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>:${SDL_ANDROID_PLATFORM_ANDROID_JAR}"
                    -d "${classes_path}"
                COMMAND ${Java_JAR_EXECUTABLE} cf "${OUT_JAR}" -C "${classes_path}" .
                DEPENDS $<TARGET_PROPERTY:${TEST}-apk-linked,OUTPUTS> "$<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>" "${JAVA_PACKAGE_DIR}/SDLTestActivity.java" "${JAVA_PACKAGE_DIR}/SDLEntryTestActivity.java"
            )
            add_custom_target(${TEST}-jar DEPENDS "${OUT_JAR}")
            set_property(TARGET ${TEST}-jar PROPERTY OUTPUT "${OUT_JAR}")

            set(dexworkdir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${TEST}-dex.dir")
            # Some CMake versions have a slow `cmake -E make_directory` implementation
            if(NOT IS_DIRECTORY "${dexworkdir}")
                execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${dexworkdir}")
            endif()
            set(classes_dex_base_name "classes.dex")
            set(classes_dex "${dexworkdir}/${classes_dex_base_name}")
            add_custom_command(
                OUTPUT "${classes_dex}"
                COMMAND SdlAndroid::d8
                    $<TARGET_PROPERTY:${TEST}-jar,OUTPUT>
                    $<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>
                    --lib "${SDL_ANDROID_PLATFORM_ANDROID_JAR}"
                    --output "${dexworkdir}"
                DEPENDS $<TARGET_PROPERTY:${TEST}-jar,OUTPUT> $<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>
            )
            add_custom_target(${TEST}-dex DEPENDS "${classes_dex}")
            set_property(TARGET ${TEST}-dex PROPERTY OUTPUT "${classes_dex}")
            set_property(TARGET ${TEST}-dex PROPERTY OUTPUT_BASE_NAME "${classes_dex_base_name}")

            sdl_add_to_apk_unaligned(${TEST}-unaligned-apk
                APK_IN ${TEST}-apk-linked
                OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/intermediates"
                ASSETS ${RESOURCE_FILES}
                NATIVE_LIBS SDL3::SDL3-shared ${TEST}
                DEX ${TEST}-dex
            )

            sdl_apk_align(${TEST}-aligned-apk ${TEST}-unaligned-apk
                OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/intermediates"
            )
            sdl_apk_sign(${TEST}-apk ${TEST}-aligned-apk
                KEYSTORE SDL_test-debug-keystore
            )
            add_dependencies(sdl-test-apks ${TEST}-apk)

            if(TARGET SdlAndroid::adb)
                add_custom_target(install-${TEST}
                    COMMAND "${CMAKE_COMMAND}" -DACTION=install "-DAPKS=$<TARGET_PROPERTY:${TEST}-apk,OUTPUT>" -P "${SDL3_SOURCE_DIR}/cmake/android/SdlAndroidScript.cmake"
                    DEPENDS "${TEST}-apk"
                )
                add_custom_target(start-${TEST}
                    COMMAND "${ADB_BIN}" shell am start-activity -S "${ANDROID_MANIFEST_PACKAGE}/.SDLTestActivity"
                )
                add_custom_target(build-install-start-${TEST}
                    COMMAND "${CMAKE_COMMAND}" -DACTION=build-install-run "-DEXECUTABLES=${TEST}" "-DBUILD_FOLDER=${CMAKE_BINARY_DIR}" -P "${SDL3_SOURCE_DIR}/cmake/android/SdlAndroidScript.cmake"
                )
            endif()

            list(APPEND packages "${ANDROID_MANIFEST_PACKAGE}")
            list(APPEND install_targets install-${TEST})
        endforeach()

        if(TARGET SdlAndroid::adb)
            add_custom_target(install-sdl-test-apks
                DEPENDS ${install_targets}
                VERBATIM
            )
            add_custom_target(uninstall-sdl-test-apks
                COMMAND "${CMAKE_COMMAND}" "-DADB=$<TARGET_FILE:SdlAndroid::adb>" -DACTION=uninstall "-DPACKAGES=${packages}" -P "${SDL3_SOURCE_DIR}/cmake/android/SdlAndroidScript.cmake"
                VERBATIM
            )
        endif()
    endif()
endif()
