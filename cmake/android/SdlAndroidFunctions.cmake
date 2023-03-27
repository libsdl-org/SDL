#[=======================================================================[

This CMake script contains functions to build an Android APK.
It is (currently) limited to packaging binaries for a single architecture.

#]=======================================================================]

cmake_minimum_required(VERSION 3.7)

if(NOT PROJECT_NAME MATCHES "^SDL.*")
  message(WARNING "This module is internal to SDL and is currently not supported.")
endif()

function(_sdl_create_outdir_for_target OUTDIRECTORY TARGET)
  set(outdir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${TARGET}.dir")
  # Some CMake versions have a slow `cmake -E make_directory` implementation
  if(NOT IS_DIRECTORY "${outdir}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E make_directory "${outdir}")
  endif()
  set("${OUTDIRECTORY}" "${outdir}" PARENT_SCOPE)
endfunction()

function(sdl_create_android_debug_keystore TARGET)
  set(output "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_debug.keystore")
  add_custom_command(OUTPUT ${output}
    COMMAND ${CMAKE_COMMAND} -E rm -f "${output}"
    COMMAND SdlAndroid::keytool -genkey -keystore "${output}" -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "C=US, O=Android, CN=Android Debug"
  )
  add_custom_target(${TARGET} DEPENDS "${output}")
  set_property(TARGET ${TARGET} PROPERTY OUTPUT "${output}")
endfunction()

function(sdl_android_compile_resources TARGET)
  cmake_parse_arguments(arg "" "RESFOLDER" "RESOURCES" ${ARGN})

  if(NOT arg_RESFOLDER AND NOT arg_RESOURCES)
    message(FATAL_ERROR "Missing RESFOLDER or RESOURCES argument (need one or both)")
  endif()
  _sdl_create_outdir_for_target(outdir "${TARGET}")
  set(out_files "")

  set(res_files "")
  if(arg_RESFOLDER)
    get_filename_component(arg_RESFOLDER "${arg_RESFOLDER}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    file(GLOB_RECURSE res_folder_files "${arg_RESFOLDER}/*")
    list(APPEND res_files ${res_folder_files})

    foreach(res_file IN LISTS res_files)
      file(RELATIVE_PATH rel_res_file "${arg_RESFOLDER}" "${res_file}")
      string(REPLACE "/" "_" rel_comp_path "${rel_res_file}")
      if(res_file MATCHES ".*res/values.*\\.xml$")
        string(REGEX REPLACE "\\.xml" ".arsc" rel_comp_path "${rel_comp_path}")
      endif()
      set(comp_path "${outdir}/${rel_comp_path}.flat")
      add_custom_command(
        OUTPUT "${comp_path}"
        COMMAND SdlAndroid::aapt2 compile -o "${outdir}" "${res_file}"
        DEPENDS ${res_file}
      )
      list(APPEND out_files "${comp_path}")
    endforeach()
  endif()

  if(arg_RESOURCES)
    list(APPEND res_files ${arg_RESOURCES})
    foreach(res_file IN LISTS arg_RESOURCES)
      string(REGEX REPLACE ".*/res/" "" rel_res_file ${res_file})
      string(REPLACE "/" "_" rel_comp_path "${rel_res_file}")
      if(res_file MATCHES ".*res/values.*\\.xml$")
        string(REGEX REPLACE "\\.xml" ".arsc" rel_comp_path "${rel_comp_path}")
      endif()
      set(comp_path "${outdir}/${rel_comp_path}.flat")
      add_custom_command(
        OUTPUT "${comp_path}"
        COMMAND SdlAndroid::aapt2 compile -o "${outdir}" "${res_file}"
        DEPENDS ${res_file}
      )
      list(APPEND out_files "${comp_path}")
    endforeach()
  endif()

  add_custom_target(${TARGET} DEPENDS ${out_files})
  set_property(TARGET "${TARGET}" PROPERTY OUTPUTS "${out_files}")
  set_property(TARGET "${TARGET}" PROPERTY SOURCES "${res_files}")
endfunction()

function(sdl_android_link_resources TARGET)
  cmake_parse_arguments(arg "NO_DEBUG" "MIN_SDK_VERSION;TARGET_SDK_VERSION;ANDROID_JAR;OUTPUT_APK;MANIFEST;PACKAGE" "RES_TARGETS" ${ARGN})

  if(arg_MANIFEST)
    get_filename_component(arg_MANIFEST "${arg_MANIFEST}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  else()
    message(FATAL_ERROR "sdl_add_android_link_resources_target requires a Android MANIFEST path (${arg_MANIFEST})")
  endif()
  if(NOT arg_PACKAGE)
    file(READ "${arg_MANIFEST}" manifest_contents)
    string(REGEX MATCH "package=\"([a-zA-Z0-9_.]+)\"" package_match "${manifest_contents}")
    if(NOT package_match)
      message(FATAL_ERROR "Could not extract package from Android manifest (${arg_MANIFEST})")
    endif()
    set(arg_PACKAGE "${CMAKE_MATCH_1}")
  endif()

  set(depends "")

  _sdl_create_outdir_for_target(outdir "${TARGET}")
  string(REPLACE "." "/" java_r_path "${arg_PACKAGE}")
  get_filename_component(java_r_path "${java_r_path}" ABSOLUTE BASE_DIR "${outdir}")
  set(java_r_path "${java_r_path}/R.java")

  set(command SdlAndroid::aapt2 link)
  if(NOT arg_NO_DEBUG)
    list(APPEND command --debug-mode)
  endif()
  if(arg_MIN_SDK_VERSION)
    list(APPEND command --min-sdk-version ${arg_MIN_SDK_VERSION})
  endif()
  if(arg_TARGET_SDK_VERSION)
    list(APPEND command --target-sdk-version ${arg_TARGET_SDK_VERSION})
  endif()
  if(arg_ANDROID_JAR)
    list(APPEND command -I "${arg_ANDROID_JAR}")
  else()
    list(APPEND command -I "${SDL_ANDROID_PLATFORM_ANDROID_JAR}")
  endif()
  if(NOT arg_OUTPUT_APK)
    set(arg_OUTPUT_APK "${TARGET}.apk")
  endif()
  get_filename_component(arg_OUTPUT_APK "${arg_OUTPUT_APK}" ABSOLUTE BASE_DIR "${outdir}")
  list(APPEND command -o "${arg_OUTPUT_APK}")
  list(APPEND command --java "${outdir}")
  list(APPEND command --manifest "${arg_MANIFEST}")
  foreach(res_target IN LISTS arg_RES_TARGETS)
    list(APPEND command $<TARGET_PROPERTY:${res_target},OUTPUTS>)
    list(APPEND depends $<TARGET_PROPERTY:${res_target},OUTPUTS>)
  endforeach()
  add_custom_command(
    OUTPUT "${arg_OUTPUT_APK}" "${java_r_path}"
    COMMAND ${command}
    DEPENDS ${depends} ${arg_MANIFEST}
    COMMAND_EXPAND_LISTS
    VERBATIM
  )
  add_custom_target(${TARGET} DEPENDS "${arg_OUTPUT_APK}" "${java_r_path}")
  set_property(TARGET ${TARGET} PROPERTY OUTPUT "${arg_OUTPUT_APK}")
  set_property(TARGET ${TARGET} PROPERTY JAVA_R "${java_r_path}")
  set_property(TARGET ${TARGET} PROPERTY OUTPUTS "${${arg_OUTPUT_APK}};${java_r_path}")
endfunction()

function(sdl_add_to_apk_unaligned TARGET)
  cmake_parse_arguments(arg "" "APK_IN;NAME;OUTDIR" "ASSETS;NATIVE_LIBS;DEX" ${ARGN})

  if(NOT arg_APK_IN)
    message(FATAL_ERROR "Missing APK_IN argument")
  endif()

  if(NOT TARGET ${arg_APK_IN})
    message(FATAL_ERROR "APK_IN (${arg_APK_IN}) must be a target providing an apk")
  endif()

  _sdl_create_outdir_for_target(workdir ${TARGET})

  if(NOT arg_OUTDIR)
    set(arg_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}")
  endif()

  if(NOT arg_NAME)
    string(REGEX REPLACE "[:-]+" "." arg_NAME "${TARGET}")
    if(NOT arg_NAME MATCHES "\\.apk")
      set(arg_NAME "${arg_NAME}.apk")
    endif()
  endif()
  get_filename_component(apk_file "${arg_NAME}" ABSOLUTE BASE_DIR "${arg_OUTDIR}")

  set(apk_libdir "lib/${ANDROID_ABI}")

  set(depends "")

  set(commands
    COMMAND "${CMAKE_COMMAND}" -E remove_directory -rf "${apk_libdir}" "assets"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${apk_libdir}" "assets"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_PROPERTY:${arg_APK_IN},OUTPUT>" "${apk_file}"
  )

  set(dex_i "1")
  foreach(dex IN LISTS arg_DEX)
    set(suffix "${dex_i}")
    if(suffix STREQUAL "1")
      set(suffix "")
    endif()
    list(APPEND commands
      COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_PROPERTY:${dex},OUTPUT>" "classes${suffix}.dex"
      COMMAND SdlAndroid::zip -u -q -j "${apk_file}" "classes${suffix}.dex"
    )
    math(EXPR dex_i "${dex_i}+1")
    list(APPEND depends "$<TARGET_PROPERTY:${dex},OUTPUT>")
  endforeach()

  foreach(native_lib IN LISTS arg_NATIVE_LIBS)
    list(APPEND commands
      COMMAND "${CMAKE_COMMAND}" -E copy $<TARGET_FILE:${native_lib}> "${apk_libdir}/$<TARGET_FILE_NAME:${native_lib}>"
      COMMAND SdlAndroid::zip -u -q "${apk_file}" "${apk_libdir}/$<TARGET_FILE_NAME:${native_lib}>"
    )
  endforeach()
  if(arg_ASSETS)
    list(APPEND commands
      COMMAND "${CMAKE_COMMAND}" -E copy ${arg_ASSETS} "assets"
      COMMAND SdlAndroid::zip -u -r -q "${apk_file}" "assets"
    )
  endif()

  add_custom_command(OUTPUT "${apk_file}"
    ${commands}
    DEPENDS ${arg_NATIVE_LIBS} ${depends} "$<TARGET_PROPERTY:${arg_APK_IN},OUTPUT>"
    WORKING_DIRECTORY "${workdir}"
  )
  add_custom_target(${TARGET} DEPENDS "${apk_file}")
  set_property(TARGET ${TARGET} PROPERTY OUTPUT "${apk_file}")
endfunction()

function(sdl_apk_align TARGET APK_IN)
  cmake_parse_arguments(arg "" "NAME;OUTDIR" "" ${ARGN})

  if(NOT TARGET ${arg_APK_IN})
    message(FATAL_ERROR "APK_IN (${arg_APK_IN}) must be a target providing an apk")
  endif()

  if(NOT arg_OUTDIR)
    set(arg_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}")
  endif()

  if(NOT arg_NAME)
    string(REGEX REPLACE "[:-]+" "." arg_NAME "${TARGET}")
    if(NOT arg_NAME MATCHES "\\.apk")
      set(arg_NAME "${arg_NAME}.apk")
    endif()
  endif()
  get_filename_component(apk_file "${arg_NAME}" ABSOLUTE BASE_DIR "${arg_OUTDIR}")

  add_custom_command(OUTPUT "${apk_file}"
    COMMAND SdlAndroid::zipalign -f 4 "$<TARGET_PROPERTY:${APK_IN},OUTPUT>" "${apk_file}"
    DEPENDS "$<TARGET_PROPERTY:${APK_IN},OUTPUT>"
  )
  add_custom_target(${TARGET} DEPENDS "${apk_file}")
  set_property(TARGET ${TARGET} PROPERTY OUTPUT "${apk_file}")
endfunction()

function(sdl_apk_sign TARGET APK_IN)
  cmake_parse_arguments(arg "" "OUTPUT;KEYSTORE" "" ${ARGN})

  if(NOT TARGET ${arg_APK_IN})
    message(FATAL_ERROR "APK_IN (${arg_APK_IN}) must be a target providing an apk")
  endif()

  if(NOT TARGET ${arg_KEYSTORE})
    message(FATAL_ERROR "APK_KEYSTORE (${APK_KEYSTORE}) must be a target providing a keystore")
  endif()

  if(NOT arg_OUTPUT)
    string(REGEX REPLACE "[:-]+" "." arg_OUTPUT "${TARGET}")
    if(NOT arg_OUTPUT MATCHES "\\.apk")
      set(arg_OUTPUT "${arg_OUTPUT}.apk")
    endif()
  endif()
  get_filename_component(apk_file "${arg_OUTPUT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

  add_custom_command(OUTPUT "${apk_file}"
    COMMAND SdlAndroid::apksigner sign
      --ks "$<TARGET_PROPERTY:${arg_KEYSTORE},OUTPUT>"
      --ks-pass pass:android --in "$<TARGET_PROPERTY:${APK_IN},OUTPUT>" --out "${apk_file}"
    DEPENDS "$<TARGET_PROPERTY:${APK_IN},OUTPUT>" "$<TARGET_PROPERTY:${arg_KEYSTORE},OUTPUT>"
    BYPRODUCTS "${apk_file}.idsig"
  )
  add_custom_target(${TARGET} DEPENDS "${apk_file}")
  set_property(TARGET ${TARGET} PROPERTY OUTPUT "${apk_file}")
endfunction()
