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

if(WINDOWS_STORE)
    cmake_minimum_required(VERSION 3.19)
    # CMP0112: Target file component generator expressions do not add target dependencies.
    cmake_policy(SET CMP0112 NEW)
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

if(WINDOWS_STORE)
    add_library(sdl_example_main_callbacks_uwp OBJECT ../test/main.cpp)
    target_link_libraries(sdl_example_main_callbacks_uwp PRIVATE SDL3::Headers)
    target_compile_options(sdl_example_main_callbacks_uwp PRIVATE "/ZW")
    target_compile_definitions(sdl_example_main_callbacks_uwp PRIVATE "SDL_MAIN_USE_CALLBACKS")

    set_source_files_properties(${RESOURCE_FILES} PROPERTIES VS_DEPLOYENT_LOCATION "Assets")
endif()

macro(add_sdl_example_executable TARGET)
    cmake_parse_arguments(AST "BUILD_DEPENDENT;NEEDS_RESOURCES;" "" "SOURCES" ${ARGN})
    if(AST_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown argument(s): ${AST_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT AST_SOURCES)
        message(FATAL_ERROR "add_sdl_example_executable needs at least one source")
    endif()
    set(EXTRA_SOURCES "")
    if(WINDOWS_STORE)
        set(uwp_bindir "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.dir")
        if(NOT IS_DIRECTORY "${uwp_bindir}")
            execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${uwp_bindir}")
        endif()
        string(REGEX REPLACE "[_]" "" SAFE_TARGET "${TARGET}")
        file(GENERATE OUTPUT "${uwp_bindir}/${TARGET}.appxmanifest"
            INPUT "${CMAKE_CURRENT_SOURCE_DIR}/../test/uwp/Package.appxmanifest.in"
            TARGET "${TARGET}"
        )
        set_property(SOURCE "${uwp_bindir}/${TARGET}.appxmanifest" PROPERTY VS_DEPLOYMENT_CONTENT 1)

        list(APPEND EXTRA_SOURCES "$<TARGET_OBJECTS:sdl_example_main_callbacks_uwp>")

        list(APPEND EXTRA_SOURCES
            "${uwp_bindir}/${TARGET}.appxmanifest"
            "../test/uwp/logo-50x50.png"
            "../test/uwp/square-44x44.png"
            "../test/uwp/square-150x150.png"
            "../test/uwp/splash-620x300.png"
        )
    endif()
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
    target_link_libraries(${TARGET} PRIVATE SDL3::${sdl_name_component})

    list(APPEND SDL_EXAMPLE_EXECUTABLES ${TARGET})
    if(AST_NEEDS_RESOURCES)
        if(PSP OR PS2)
            add_custom_command(TARGET ${TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} ARGS -E make_directory $<TARGET_FILE_DIR:${TARGET}>/sdl-${TARGET}
                COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${RESOURCE_FILES} $<TARGET_FILE_DIR:${TARGET}>/sdl-${TARGET})
        elseif(WINDOWS_STORE)
            # MSVC does build the dependent targets (or POST_BUILD commands) when building an application
            # after starting to debug. By copying the resources in a custom target, the files can be copied afterwards.
            # FIXME: find out proper way to add assets to UWP package
            cmake_minimum_required(VERSION 3.19)
            add_custom_target(zzz-resources-copy-${TARGET}
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET}>/AppX"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RESOURCE_FILES} "$<TARGET_FILE_DIR:${TARGET}>/AppX"
            )
            add_dependencies(${TARGET} zzz-resources-copy-${TARGET})
        else()
            add_dependencies(${TARGET} copy-sdl-example-resources)
        endif()
        if(APPLE)
            # Make sure resource files get installed into macOS/iOS .app bundles.
            set_target_properties(${TARGET} PROPERTIES RESOURCE "${RESOURCE_FILES}")
        endif()
        if(EMSCRIPTEN)
            foreach(res IN LISTS RESOURCE_FILES)
                get_filename_component(res_name "${res}" NAME)
                target_link_options(${TARGET} PRIVATE "SHELL:--embed-file ${res}@${res_name}")
            endforeach()
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
    if(WINDOWS_STORE)
        target_compile_definitions(${TARGET} PRIVATE "SDL_MAIN_NOIMPL")
        set_property(TARGET ${TARGET} PROPERTY WIN32_EXECUTABLE TRUE)
        set_property(TARGET ${TARGET} PROPERTY RUNTIME_OUTPUT_DIRECTORY "${uwp_bindir}")
        target_link_options(${TARGET} PRIVATE
            -nodefaultlib:vccorlib$<$<CONFIG:Debug>:d>
            -nodefaultlib:msvcrt$<$<CONFIG:Debug>:d>
            vccorlib$<$<CONFIG:Debug>:d>.lib
            msvcrt$<$<CONFIG:Debug>:d>.lib
        )
    endif()

    if(EMSCRIPTEN)
        set_property(TARGET ${TARGET} PROPERTY SUFFIX ".html")
    endif()

    if(OPENGL_FOUND)
        target_compile_definitions(${TARGET} PRIVATE HAVE_OPENGL)
    endif()

    # FIXME: only add "${SDL3_BINARY_DIR}/include-config-$<LOWER_CASE:$<CONFIG>>" + include paths of external dependencies
    target_include_directories(${TARGET} PRIVATE "$<TARGET_PROPERTY:SDL3::${sdl_name_component},INCLUDE_DIRECTORIES>")
endmacro()

add_sdl_example_executable(renderer-clear SOURCES renderer/clear/renderer-clear.c)


if(PSP)
    # Build EBOOT files if building for PSP
    foreach(APP ${SDL_EXAMPLE_EXECUTABLES})
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
        sdl_android_compile_resources(SDL_example-resources RESFOLDER ../test/android/res)
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
            configure_file(../test/android/cmake/SDLEntryTestActivity.java.cmake "${JAVA_PACKAGE_DIR}/SDLEntryTestActivity.java" @ONLY)
            configure_file(../test/android/cmake/SDLTestActivity.java.cmake "${JAVA_PACKAGE_DIR}/SDLTestActivity.java" @ONLY)
            configure_file(../test/android/cmake/res/values/strings.xml.cmake android/res/values/strings-${EXAMPLE}.xml @ONLY)
            configure_file(../test/android/cmake/res/xml/shortcuts.xml.cmake "${GENERATED_RES_FOLDER}/xml/shortcuts.xml" @ONLY)
            configure_file(../test/android/cmake/AndroidManifest.xml.cmake "${generated_manifest_path}" @ONLY)
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
