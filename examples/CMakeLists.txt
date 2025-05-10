#
# CMake script for building the SDL examples
#

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")

include(CheckIncludeFile)
include(CheckStructHasMember)
include(CMakePushCheckState)
include(sdlcompilers)

if(SDL_EXAMPLES_LINK_SHARED)
    set(sdl_name_component SDL3-shared)
else()
    set(sdl_name_component SDL3-static)
endif()
set(HAVE_EXAMPLES_LINK_SHARED "${SDL_EXAMPLES_LINK_SHARED}" PARENT_SCOPE)

# CMake incorrectly detects opengl32.lib being present on MSVC ARM64
if(NOT (MSVC AND SDL_CPU_ARM64))
    # Prefer GLVND, if present
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL)
endif()

set(SDL_EXAMPLE_EXECUTABLES)

if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(example_bin_dir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    if(NOT IS_ABSOLUTE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        set(example_bin_dir "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    endif()
else()
    set(example_bin_dir "${CMAKE_CURRENT_BINARY_DIR}")
endif()
if(NOT CMAKE_VERSION VERSION_LESS 3.20)
    get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    set(example_bin_dir "${example_bin_dir}$<$<BOOL:${is_multi_config}>:/$<CONFIG>>")
endif()

file(GLOB RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/*.bmp ${CMAKE_CURRENT_SOURCE_DIR}/../test/*.wav ${CMAKE_CURRENT_SOURCE_DIR}/../test/*.hex)

set(RESOURCE_FILE_NAMES)
set(RESOURCE_FILES_BINDIR)
foreach(resource_file IN LISTS RESOURCE_FILES)
    get_filename_component(res_file_name ${resource_file} NAME)
    list(APPEND RESOURCE_FILE_NAMES "${res_file_name}")
    set(resource_file_bindir "${example_bin_dir}/${res_file_name}")
    add_custom_command(OUTPUT "${resource_file_bindir}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${resource_file}" "${resource_file_bindir}"
        DEPENDS "${resource_file}"
    )
    list(APPEND RESOURCE_FILES_BINDIR "${resource_file_bindir}")
endforeach()
add_custom_target(copy-sdl-example-resources
    DEPENDS "${RESOURCE_FILES_BINDIR}"
)

macro(add_sdl_example_executable TARGET)
    cmake_parse_arguments(AST "BUILD_DEPENDENT" "" "SOURCES;DATAFILES" ${ARGN})
    if(AST_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown argument(s): ${AST_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT AST_SOURCES)
        message(FATAL_ERROR "add_sdl_example_executable needs at least one source")
    endif()
    set(EXTRA_SOURCES "")
    if(AST_DATAFILES)
        list(APPEND EXTRA_SOURCES ${DATAFILES})
    endif()
    if(ANDROID)
        add_library(${TARGET} SHARED ${AST_SOURCES} ${EXTRA_SOURCES})
    else()
        add_executable(${TARGET} ${AST_SOURCES} ${EXTRA_SOURCES})
    endif()
    SDL_AddCommonCompilerFlags(${TARGET})
    target_include_directories(${TARGET} PRIVATE "${SDL3_SOURCE_DIR}/src/video/khronos")
    target_link_libraries(${TARGET} PRIVATE SDL3::${sdl_name_component})

    list(APPEND SDL_EXAMPLE_EXECUTABLES ${TARGET})
    if(AST_DATAFILES)
        if(PSP OR PS2)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} ARGS -E make_directory $<TARGET_FILE_DIR:${TARGET}>/sdl-${TARGET}
                COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${AST_DATAFILES} $<TARGET_FILE_DIR:${TARGET}>/sdl-${TARGET}
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            )
        else()
            add_dependencies(${TARGET} copy-sdl-example-resources)
        endif()
        if(APPLE)
            # Make sure resource files get installed into macOS/iOS .app bundles.
            set_target_properties(${TARGET} PROPERTIES RESOURCE "${AST_DATAFILES}")
        endif()
        if(EMSCRIPTEN)
            foreach(res IN LISTS AST_DATAFILES)
                get_filename_component(res_name "${res}" NAME)
                target_link_options(${TARGET} PRIVATE "SHELL:--embed-file ${res}@${res_name}")
            endforeach()
        endif()
        set_property(TARGET ${TARGET} APPEND PROPERTY ADDITIONAL_CLEAN_FILES "$<TARGET_FILE_DIR:${TARGET}>/$<JOIN:${AST_DATAFILES},$<SEMICOLON>$<TARGET_FILE_DIR:${TARGET}>/>")
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
        target_link_options(${TARGET} PRIVATE -sALLOW_MEMORY_GROWTH=1)
    elseif(NGAGE)
        string(MD5 TARGET_MD5 "${TARGET}")
        string(SUBSTRING "${TARGET_MD5}" 0 8 TARGET_MD5_8)
        target_link_options(${TARGET} PRIVATE "SHELL:-s UID3=0x${TARGET_MD5_8}")
    endif()

    if(OPENGL_FOUND)
        target_compile_definitions(${TARGET} PRIVATE HAVE_OPENGL)
    endif()

    # FIXME: only add "${SDL3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>" + include paths of external dependencies
    target_include_directories(${TARGET} PRIVATE "$<TARGET_PROPERTY:SDL3::${sdl_name_component},INCLUDE_DIRECTORIES>")
endmacro()

add_sdl_example_executable(renderer-clear SOURCES renderer/01-clear/clear.c)
add_sdl_example_executable(renderer-primitives SOURCES renderer/02-primitives/primitives.c)
add_sdl_example_executable(renderer-lines SOURCES renderer/03-lines/lines.c)
add_sdl_example_executable(renderer-points SOURCES renderer/04-points/points.c)
add_sdl_example_executable(renderer-rectangles SOURCES renderer/05-rectangles/rectangles.c)
add_sdl_example_executable(renderer-textures SOURCES renderer/06-textures/textures.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.bmp)
add_sdl_example_executable(renderer-streaming-textures SOURCES renderer/07-streaming-textures/streaming-textures.c)
add_sdl_example_executable(renderer-rotating-textures SOURCES renderer/08-rotating-textures/rotating-textures.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.bmp)
add_sdl_example_executable(renderer-scaling-textures SOURCES renderer/09-scaling-textures/scaling-textures.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.bmp)
add_sdl_example_executable(renderer-geometry SOURCES renderer/10-geometry/geometry.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.bmp)
add_sdl_example_executable(renderer-color-mods SOURCES renderer/11-color-mods/color-mods.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.bmp)
add_sdl_example_executable(renderer-viewport SOURCES renderer/14-viewport/viewport.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.bmp)
add_sdl_example_executable(renderer-cliprect SOURCES renderer/15-cliprect/cliprect.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.bmp)
add_sdl_example_executable(renderer-read-pixels SOURCES renderer/17-read-pixels/read-pixels.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.bmp)
add_sdl_example_executable(renderer-debug-text SOURCES renderer/18-debug-text/debug-text.c)
add_sdl_example_executable(renderer-affine-textures SOURCES renderer/19-affine-textures/affine-textures.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.bmp)
add_sdl_example_executable(audio-simple-playback SOURCES audio/01-simple-playback/simple-playback.c)
add_sdl_example_executable(audio-simple-playback-callback SOURCES audio/02-simple-playback-callback/simple-playback-callback.c)
add_sdl_example_executable(audio-load-wav SOURCES audio/03-load-wav/load-wav.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.wav)
add_sdl_example_executable(audio-multiple-streams SOURCES audio/04-multiple-streams/multiple-streams.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.wav ${CMAKE_CURRENT_SOURCE_DIR}/../test/sword.wav)
add_sdl_example_executable(audio-planar-data SOURCES audio/05-planar-data/planar-data.c)
add_sdl_example_executable(input-joystick-polling SOURCES input/01-joystick-polling/joystick-polling.c)
add_sdl_example_executable(input-joystick-events SOURCES input/02-joystick-events/joystick-events.c)
add_sdl_example_executable(camera-read-and-draw SOURCES camera/01-read-and-draw/read-and-draw.c)
add_sdl_example_executable(pen-drawing-lines SOURCES pen/01-drawing-lines/drawing-lines.c)
add_sdl_example_executable(asyncio-load-bitmaps SOURCES asyncio/01-load-bitmaps/load-bitmaps.c DATAFILES ${CMAKE_CURRENT_SOURCE_DIR}/../test/sample.bmp ${CMAKE_CURRENT_SOURCE_DIR}/../test/gamepad_front.bmp ${CMAKE_CURRENT_SOURCE_DIR}/../test/speaker.bmp ${CMAKE_CURRENT_SOURCE_DIR}/../test/icon2x.bmp)
add_sdl_example_executable(demo-snake SOURCES demo/01-snake/snake.c)
add_sdl_example_executable(demo-woodeneye-008 SOURCES demo/02-woodeneye-008/woodeneye-008.c)
add_sdl_example_executable(demo-infinite-monkeys SOURCES demo/03-infinite-monkeys/infinite-monkeys.c)
add_sdl_example_executable(demo-bytepusher SOURCES demo/04-bytepusher/bytepusher.c)

# When you add an example, remember to add the Visual Studio project as well:
# - Add a new example in examples/
# - Run python VisualC/examples/generate.py
# - Take note of the newly generated .vcxproj files
# - Modify the .vcxproj files if necessary (adding content such as BMP or WAV files)
# - Open VisualC/SDL.sln in Visual Studio or JetBrains Rider
# - Locate the appropriate folder in the Solution Explorer
# - Add the newly generated projects: Right click -> Add -> Existing project...
# - Test if they work
# - Save the SDL.sln solution

if(PSP)
    # Build EBOOT files if building for PSP
    foreach(APP ${SDL_EXAMPLE_EXECUTABLES})
        create_pbp_file(
            TARGET          ${APP}
            TITLE           SDL-${APP}
            ICON_PATH       NULL
            BACKGROUND_PATH NULL
            PREVIEW_PATH    NULL
            OUTPUT_DIR      $<TARGET_FILE_DIR:${APP}>/sdl-${APP}
        )
    endforeach()
endif()

if(N3DS)
    set(ROMFS_DIR "${CMAKE_CURRENT_BINARY_DIR}/romfs")
    file(MAKE_DIRECTORY "${ROMFS_DIR}")
    file(COPY ${RESOURCE_FILES} DESTINATION "${ROMFS_DIR}")

    foreach(APP ${SDL_EXAMPLE_EXECUTABLES})
        get_target_property(TARGET_BINARY_DIR ${APP} BINARY_DIR)
        set(SMDH_FILE "${TARGET_BINARY_DIR}/${APP}.smdh")
        ctr_generate_smdh("${SMDH_FILE}"
            NAME "SDL-${APP}"
            DESCRIPTION "SDL3 Test suite"
            AUTHOR "SDL3 Contributors"
            ICON "${CMAKE_CURRENT_SOURCE_DIR}/../test/n3ds/logo48x48.png"
        )
        ctr_create_3dsx(
            ${APP}
            ROMFS "${ROMFS_DIR}"
            SMDH "${SMDH_FILE}"
        )
    endforeach()
endif()

if(RISCOS)
    set(SDL_EXAMPLE_EXECUTABLES_AIF)
    foreach(APP ${SDL_EXAMPLE_EXECUTABLES})
        set_property(TARGET ${APP} APPEND_STRING PROPERTY LINK_FLAGS " -static")
        add_custom_command(
            OUTPUT ${APP},ff8
            COMMAND elf2aif ${APP} ${APP},ff8
            DEPENDS ${APP}
        )
        add_custom_target(${APP}-aif ALL DEPENDS ${APP},ff8)
        list(APPEND SDL_EXAMPLE_EXECUTABLES_AIF ${CMAKE_CURRENT_BINARY_DIR}/${APP},ff8)
    endforeach()
endif()

# Set Apple App ID / Bundle ID.  This is needed to launch apps on some Apple
# platforms (iOS, for example).
if(APPLE)
  foreach(CURRENT_TARGET ${SDL_EXAMPLE_EXECUTABLES})
    set_target_properties("${CURRENT_TARGET}" PROPERTIES
      MACOSX_BUNDLE_GUI_IDENTIFIER "org.libsdl.${CURRENT_TARGET}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${SDL3_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${SDL3_VERSION}"
    )
  endforeach()
endif()

if(SDL_INSTALL_EXAMPLES)
    if(RISCOS)
        install(
            FILES ${SDL_EXAMPLE_EXECUTABLES_AIF}
            DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-examples/SDL3
        )
    else()
        install(
            TARGETS ${SDL_EXAMPLE_EXECUTABLES}
            DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-examples/SDL3
        )
    endif()
    if(MSVC)
        foreach(example IN LISTS SDL_EXAMPLE_EXECUTABLES)
            SDL_install_pdb(${example} "${CMAKE_INSTALL_LIBEXECDIR}/installed-examples/SDL3")
        endforeach()
    endif()
    install(
        FILES ${RESOURCE_FILES}
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/installed-examples/SDL3
    )
endif()

if(ANDROID AND TARGET SDL3::Jar)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake/android")
    find_package(SdlAndroid MODULE)
    if(SdlAndroid_FOUND)
        set(apks "")
        set(packages "")

        include(SdlAndroidFunctions)
        sdl_create_android_debug_keystore(SDL_example-debug-keystore)
        sdl_android_compile_resources(SDL_example-resources RESFOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../test/android/res)
        add_custom_target(sdl-example-apks)
        foreach(EXAMPLE ${SDL_EXAMPLE_EXECUTABLES})
            set(ANDROID_MANIFEST_APP_NAME "${EXAMPLE}")
            set(ANDROID_MANIFEST_LABEL "${EXAMPLE}")
            set(ANDROID_MANIFEST_LIB_NAME "$<TARGET_FILE_BASE_NAME:${EXAMPLE}>")
            set(ANDROID_MANIFEST_PACKAGE "org.libsdl.sdl.example.${EXAMPLE}")
            set(generated_manifest_path "${CMAKE_CURRENT_BINARY_DIR}/android/${EXAMPLE}-src/AndroidManifest.xml")
            string(REPLACE "." "/" JAVA_PACKAGE_DIR "${ANDROID_MANIFEST_PACKAGE}")
            set(GENERATED_SRC_FOLDER "${CMAKE_CURRENT_BINARY_DIR}/android/${EXAMPLE}-src")
            set(GENERATED_RES_FOLDER "${GENERATED_SRC_FOLDER}/res")
            set(JAVA_PACKAGE_DIR "${GENERATED_SRC_FOLDER}/${JAVA_PACKAGE_DIR}")
            configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../test/android/cmake/SDLEntryTestActivity.java.cmake "${JAVA_PACKAGE_DIR}/SDLEntryTestActivity.java" @ONLY)
            configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../test/android/cmake/SDLTestActivity.java.cmake "${JAVA_PACKAGE_DIR}/SDLTestActivity.java" @ONLY)
            configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../test/android/cmake/res/values/strings.xml.cmake android/res/values/strings-${EXAMPLE}.xml @ONLY)
            configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../test/android/cmake/res/xml/shortcuts.xml.cmake "${GENERATED_RES_FOLDER}/xml/shortcuts.xml" @ONLY)
            configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../test/android/cmake/AndroidManifest.xml.cmake "${generated_manifest_path}" @ONLY)
            file(GENERATE
                OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/android/${EXAMPLE}-$<CONFIG>/res/values/strings.xml"
                INPUT "${CMAKE_CURRENT_BINARY_DIR}/android/res/values/strings-${EXAMPLE}.xml"
            )

            sdl_android_compile_resources(${EXAMPLE}-resources
                RESOURCES
                    "${CMAKE_CURRENT_BINARY_DIR}/android/${EXAMPLE}-$<CONFIG>/res/values/strings.xml"
                    "${GENERATED_RES_FOLDER}/xml/shortcuts.xml"
            )

            sdl_android_link_resources(${EXAMPLE}-apk-linked
                MANIFEST "${generated_manifest_path}"
                PACKAGE ${ANDROID_MANIFEST_PACKAGE}
                RES_TARGETS SDL_example-resources ${EXAMPLE}-resources
                TARGET_SDK_VERSION 31
            )

            set(CMAKE_JAVA_COMPILE_FLAGS "-encoding;utf-8")
            set(classes_path "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${EXAMPLE}-java.dir/classes")
            # Some CMake versions have a slow `cmake -E make_directory` implementation
            if(NOT IS_DIRECTORY "${classes_path}")
                execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${classes_path}")
            endif()
            set(OUT_JAR "${CMAKE_CURRENT_BINARY_DIR}/${EXAMPLE}.jar")
            add_custom_command(
                OUTPUT "${OUT_JAR}"
                COMMAND ${CMAKE_COMMAND} -E rm -rf "${classes_path}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${classes_path}"
                COMMAND ${Java_JAVAC_EXECUTABLE}
                    -source 1.8 -target 1.8
                    -bootclasspath "$<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>"
                    "${JAVA_PACKAGE_DIR}/SDLEntryTestActivity.java"
                    "${JAVA_PACKAGE_DIR}/SDLTestActivity.java"
                    $<TARGET_PROPERTY:${EXAMPLE}-apk-linked,JAVA_R>
                    -cp "$<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>:${SDL_ANDROID_PLATFORM_ANDROID_JAR}"
                    -d "${classes_path}"
                COMMAND ${Java_JAR_EXECUTABLE} cf "${OUT_JAR}" -C "${classes_path}" .
                DEPENDS $<TARGET_PROPERTY:${EXAMPLE}-apk-linked,OUTPUTS> "$<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>" "${JAVA_PACKAGE_DIR}/SDLTestActivity.java" "${JAVA_PACKAGE_DIR}/SDLEntryTestActivity.java"
            )
            add_custom_target(${EXAMPLE}-jar DEPENDS "${OUT_JAR}")
            set_property(TARGET ${EXAMPLE}-jar PROPERTY OUTPUT "${OUT_JAR}")

            set(dexworkdir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${EXAMPLE}-dex.dir")
            # Some CMake versions have a slow `cmake -E make_directory` implementation
            if(NOT IS_DIRECTORY "${dexworkdir}")
                execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${dexworkdir}")
            endif()
            set(classes_dex_base_name "classes.dex")
            set(classes_dex "${dexworkdir}/${classes_dex_base_name}")
            add_custom_command(
                OUTPUT "${classes_dex}"
                COMMAND SdlAndroid::d8
                    $<TARGET_PROPERTY:${EXAMPLE}-jar,OUTPUT>
                    $<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>
                    --lib "${SDL_ANDROID_PLATFORM_ANDROID_JAR}"
                    --output "${dexworkdir}"
                DEPENDS $<TARGET_PROPERTY:${EXAMPLE}-jar,OUTPUT> $<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>
            )
            add_custom_target(${EXAMPLE}-dex DEPENDS "${classes_dex}")
            set_property(TARGET ${EXAMPLE}-dex PROPERTY OUTPUT "${classes_dex}")
            set_property(TARGET ${EXAMPLE}-dex PROPERTY OUTPUT_BASE_NAME "${classes_dex_base_name}")

            sdl_add_to_apk_unaligned(${EXAMPLE}-unaligned-apk
                APK_IN ${EXAMPLE}-apk-linked
                OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/intermediates"
                ASSETS ${RESOURCE_FILES}
                NATIVE_LIBS SDL3::SDL3-shared ${EXAMPLE}
                DEX ${EXAMPLE}-dex
            )

            sdl_apk_align(${EXAMPLE}-aligned-apk ${EXAMPLE}-unaligned-apk
                OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/intermediates"
            )
            sdl_apk_sign(${EXAMPLE}-apk ${EXAMPLE}-aligned-apk
                KEYSTORE SDL_example-debug-keystore
            )
            add_dependencies(sdl-example-apks ${EXAMPLE}-apk)

            if(TARGET SdlAndroid::adb)
                add_custom_target(install-${EXAMPLE}
                    COMMAND "${CMAKE_COMMAND}" -DACTION=install "-DAPKS=$<TARGET_PROPERTY:${EXAMPLE}-apk,OUTPUT>" -P "${SDL3_SOURCE_DIR}/cmake/android/SdlAndroidScript.cmake"
                    DEPENDS "${EXAMPLE}-apk"
                )
                add_custom_target(start-${EXAMPLE}
                    COMMAND "${ADB_BIN}" shell am start-activity -S "${ANDROID_MANIFEST_PACKAGE}/.SDLTestActivity"
                )
                add_custom_target(build-install-start-${EXAMPLE}
                    COMMAND "${CMAKE_COMMAND}" -DACTION=build-install-run "-DEXECUTABLES=${EXAMPLE}" "-DBUILD_FOLDER=${CMAKE_BINARY_DIR}" -P "${SDL3_SOURCE_DIR}/cmake/android/SdlAndroidScript.cmake"
                )
            endif()

            list(APPEND packages "${ANDROID_MANIFEST_PACKAGE}")
            list(APPEND install_targets install-${EXAMPLE})
        endforeach()

        if(TARGET SdlAndroid::adb)
            add_custom_target(install-sdl-example-apks
                DEPENDS ${install_targets}
                VERBATIM
            )
            add_custom_target(uninstall-sdl-example-apks
                COMMAND "${CMAKE_COMMAND}" "-DADB=$<TARGET_FILE:SdlAndroid::adb>" -DACTION=uninstall "-DPACKAGES=${packages}" -P "${SDL3_SOURCE_DIR}/cmake/android/SdlAndroidScript.cmake"
                VERBATIM
            )
        endif()
    endif()
endif()
