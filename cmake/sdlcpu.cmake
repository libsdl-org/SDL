function(SDL_DetectTargetCPUArchitectures DETECTED_ARCHS)

  set(DETECTABLE_ARCHS ARM32 ARM64 EMSCRIPTEN LOONGARCH64 POWERPC32 POWERPC64 X86 X64)

  if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    foreach(arch IN LISTS DETECTABLE_ARCHS)
      set(SDL_CPU_${arch} "0")
    endforeach()
    set(detected_archs)
    foreach(osx_arch IN LISTS CMAKE_OSX_ARCHITECTURES)
      if(osx_arch STREQUAL "x86_64")
        set(SDL_CPU_X64 "1")
        list(APPEND detected_archs "X64")
      elseif(osx_arch STREQUAL "arm64")
        set(SDL_CPU_ARM64 "1")
        list(APPEND detected_archs "ARM64")
      endif()
    endforeach()
    set("${DETECTED_ARCHS}" "${detected_archs}" PARENT_SCOPE)
    return()
  endif()

  set(detected_archs)
  foreach(arch IN LISTS DETECTABLE_ARCHS)
    if(SDL_CPU_${arch})
      list(APPEND detected_archs "${arch}")
    endif()
  endforeach()

  if(detected_archs)
    set("${DETECTED_ARCHS}" "${detected_archs}" PARENT_SCOPE)
    return()
  endif()

  set(src_arch_detect [=====[

#if defined(__arm__) || defined(_M_ARM)
#define ARCH_ARM32 "1"
#else
#define ARCH_ARM32 "0"
#endif
const char *arch_arm32 = "INFO<ARM32=" ARCH_ARM32 ">";

#if defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_ARM64 "1"
#else
#define ARCH_ARM64 "0"
#endif
const char *arch_arm64 = "INFO<ARM64=" ARCH_ARM64 ">";

#if defined(__EMSCRIPTEN__)
#define ARCH_EMSCRIPTEN "1"
#else
#define ARCH_EMSCRIPTEN "0"
#endif
const char *arch_emscripten = "INFO<EMSCRIPTEN=" ARCH_EMSCRIPTEN ">";

#if defined(__loongarch64)
#define ARCH_LOONGARCH64 "1"
#else
#define ARCH_LOONGARCH64 "0"
#endif
const char *arch_loongarch64 = "INFO<LOONGARCH64=" ARCH_LOONGARCH64 ">";

#if (defined(__PPC__) || defined(__powerpc__)) && !defined(__powerpc64__)
#define ARCH_POWERPC32 "1"
#else
#define ARCH_POWERPC32 "0"
#endif
const char *arch_powerpc32 = "INFO<ARCH_POWERPC32=" ARCH_POWERPC32 ">";

#if defined(__PPC64__) || defined(__powerpc64__)
#define ARCH_POWERPC64 "1"
#else
#define ARCH_POWERPC64 "0"
#endif
const char *arch_powerpc64 = "INFO<POWERPC64=" ARCH_POWERPC64 ">";

#if defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) ||defined( __i386) || defined(_M_IX86)
#define ARCH_X86 "1"
#else
#define ARCH_X86 "0"
#endif
const char *arch_x86 = "INFO<X86=" ARCH_X86 ">";

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
#define ARCH_X64 "1"
#else
#define ARCH_X64 "0"
#endif
const char *arch_x64 = "INFO<X64=" ARCH_X64 ">";

int main(int argc, char *argv[]) {
  (void) argv;
  int result = argc;
  result += arch_arm32[argc];
  result += arch_arm64[argc];
  result += arch_emscripten[argc];
  result += arch_loongarch64[argc];
  result += arch_powerpc32[argc];
  result += arch_powerpc64[argc];
  result += arch_x86[argc];
  result += arch_x64[argc];
  return result;
}
]=====])

  set(path_src_arch_detect "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CMakeTmp/SDL_detect_arch.c")
  file(WRITE "${path_src_arch_detect}" "${src_arch_detect}")
  set(path_dir_arch_detect "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CMakeTmp/SDL_detect_arch")
  set(path_bin_arch_detect "${path_dir_arch_detect}/bin")

  set(msg "Detecting Target CPU Architecture")
  message(STATUS "${msg}")

  try_compile(COMPILED_RES
          "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/CMakeTmp/SDL_detect_arch"
          SOURCES "${path_src_arch_detect}"
          COPY_FILE "${path_bin_arch_detect}"
          )
  if(NOT COMPILED_RES)
    message(STATUS "${msg} - <ERROR>")
    message(WARNING "Failed to compile source detecting the target CPU architecture")
  endif()

  set(re "INFO<([A-Z0-9]+)=([01])>")
  file(STRINGS "${path_bin_arch_detect}" infos REGEX "${re}")

  set(detected_archs)

  foreach(info_arch_01 IN LISTS infos)
    string(REGEX MATCH "${re}" A "${info_arch_01}")
    if(NOT "${CMAKE_MATCH_1}" IN_LIST DETECTABLE_ARCHS)
      message(WARNING "Unknown architecture: \"${CMAKE_MATCH_1}\"")
      continue()
    endif()
    set(arch "${CMAKE_MATCH_1}")
    set(arch_01 "${CMAKE_MATCH_2}")
    if(arch_01)
      list(APPEND detected_archs "${arch}")
    endif()
    set("SDL_CPU_${arch}" "${arch_01}" CACHE BOOL "Detected architecture ${arch}")
  endforeach()

  message(STATUS "${msg} - ${detected_archs}")

  set("${DETECTED_ARCHS}" "${detected_archs}" PARENT_SCOPE)

endfunction()
