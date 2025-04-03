function(SDL_DetectTargetCPUArchitectures DETECTED_ARCHS)

  set(known_archs EMSCRIPTEN ARM32 ARM64 ARM64EC LOONGARCH64 POWERPC32 POWERPC64 X86 X64)

  if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    foreach(known_arch IN LISTS known_archs)
      set(SDL_CPU_${known_arch} "0" PARENT_SCOPE)
    endforeach()
    set(detected_archs)
    foreach(osx_arch IN LISTS CMAKE_OSX_ARCHITECTURES)
      if(osx_arch STREQUAL "x86_64")
        set(SDL_CPU_X64 "1" PARENT_SCOPE)
        list(APPEND detected_archs "X64")
      elseif(osx_arch STREQUAL "arm64")
        set(SDL_CPU_ARM64 "1" PARENT_SCOPE)
        list(APPEND detected_archs "ARM64")
      endif()
    endforeach()
    set("${DETECTED_ARCHS}" "${detected_archs}" PARENT_SCOPE)
    return()
  endif()

  set(detected_archs)
  foreach(known_arch IN LISTS known_archs)
    if(SDL_CPU_${known_arch})
      list(APPEND detected_archs "${known_arch}")
    endif()
  endforeach()

  if(detected_archs)
    set("${DETECTED_ARCHS}" "${detected_archs}" PARENT_SCOPE)
    return()
  endif()

  set(arch_check_ARM32 "defined(__arm__) || defined(_M_ARM)")
  set(arch_check_ARM64 "defined(__aarch64__) || defined(_M_ARM64)")
  set(arch_check_ARM64EC "defined(_M_ARM64EC)")
  set(arch_check_EMSCRIPTEN "defined(__EMSCRIPTEN__)")
  set(arch_check_LOONGARCH64 "defined(__loongarch64)")
  set(arch_check_POWERPC32 "(defined(__PPC__) || defined(__powerpc__)) && !defined(__powerpc64__)")
  set(arch_check_POWERPC64 "defined(__PPC64__) || defined(__powerpc64__)")
  set(arch_check_X86 "defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) ||defined( __i386) || defined(_M_IX86)")
  set(arch_check_X64 "(defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)) && !defined(_M_ARM64EC)")

  set(src_vars "")
  set(src_main "")
  foreach(known_arch IN LISTS known_archs)
    set(detected_${known_arch} "0")

    string(APPEND src_vars "
#if ${arch_check_${known_arch}}
#define ARCH_${known_arch} \"1\"
#else
#define ARCH_${known_arch} \"0\"
#endif
const char *arch_${known_arch} = \"INFO<${known_arch}=\" ARCH_${known_arch} \">\";
")
    string(APPEND src_main "
  result += arch_${known_arch}[argc];")
  endforeach()

  set(src_arch_detect "${src_vars}
int main(int argc, char *argv[]) {
  int result = 0;
  (void)argv;
${src_main}
  return result;
}")

  if(CMAKE_C_COMPILER)
    set(ext ".c")
  elseif(CMAKE_CXX_COMPILER)
    set(ext ".cpp")
  else()
    enable_language(C)
    set(ext ".c")
  endif()
  set(path_src_arch_detect "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CMakeTmp/SDL_detect_arch${ext}")
  file(WRITE "${path_src_arch_detect}" "${src_arch_detect}")
  set(path_dir_arch_detect "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CMakeTmp/SDL_detect_arch")
  set(path_bin_arch_detect "${path_dir_arch_detect}/bin")

  set(detected_archs)

  set(msg "Detecting Target CPU Architecture")
  message(STATUS "${msg}")

  include(CMakePushCheckState)

  set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

  cmake_push_check_state(RESET)
  try_compile(SDL_CPU_CHECK_ALL
    "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CMakeTmp/SDL_detect_arch"
    SOURCES "${path_src_arch_detect}"
    COPY_FILE "${path_bin_arch_detect}"
  )
  cmake_pop_check_state()
  if(NOT SDL_CPU_CHECK_ALL)
    message(STATUS "${msg} - <ERROR>")
    message(WARNING "Failed to compile source detecting the target CPU architecture")
  else()
    set(re "INFO<([A-Z0-9]+)=([01])>")
    file(STRINGS "${path_bin_arch_detect}" infos REGEX "${re}")

    foreach(info_arch_01 IN LISTS infos)
      string(REGEX MATCH "${re}" A "${info_arch_01}")
      if(NOT "${CMAKE_MATCH_1}" IN_LIST known_archs)
        message(WARNING "Unknown architecture: \"${CMAKE_MATCH_1}\"")
        continue()
      endif()
      set(arch "${CMAKE_MATCH_1}")
      set(arch_01 "${CMAKE_MATCH_2}")
      set(detected_${arch} "${arch_01}")
    endforeach()

    foreach(known_arch IN LISTS known_archs)
      if(detected_${known_arch})
        list(APPEND detected_archs ${known_arch})
      endif()
    endforeach()
  endif()

  if(detected_archs)
    foreach(known_arch IN LISTS known_archs)
      set("SDL_CPU_${known_arch}" "${detected_${known_arch}}" CACHE BOOL "Detected architecture ${known_arch}")
    endforeach()
    message(STATUS "${msg} - ${detected_archs}")
  else()
    include(CheckCSourceCompiles)
    cmake_push_check_state(RESET)
    foreach(known_arch IN LISTS known_archs)
      if(NOT detected_archs)
        set(cache_variable "SDL_CPU_${known_arch}")
          set(test_src "
        int main(int argc, char *argv[]) {
        #if ${arch_check_${known_arch}}
          return 0;
        #else
          choke
        #endif
        }
        ")
        check_c_source_compiles("${test_src}" "${cache_variable}")
        if(${cache_variable})
          set(SDL_CPU_${known_arch} "1" CACHE BOOL "Detected architecture ${known_arch}")
          set(detected_archs ${known_arch})
        else()
          set(SDL_CPU_${known_arch} "0" CACHE BOOL "Detected architecture ${known_arch}")
        endif()
      endif()
    endforeach()
    cmake_pop_check_state()
  endif()
  set("${DETECTED_ARCHS}" "${detected_archs}" PARENT_SCOPE)
endfunction()
