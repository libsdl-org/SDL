add_library(SDL3-collector INTERFACE)
add_library(SDL3_test-collector INTERFACE)

# Use sdl_glob_sources to add glob sources to SDL3-shared, to SDL3-static, or to both.
function(sdl_glob_sources)
  cmake_parse_arguments(ARGS "" "" "SHARED;STATIC" ${ARGN})
  file(GLOB shared_sources ${ARGS_SHARED})
  file(GLOB static_sources ${ARGS_STATIC})
  file(GLOB both_sources ${ARGS_UNPARSED_ARGUMENTS})
  if(TARGET SDL3-shared)
    target_sources(SDL3-shared PRIVATE ${shared_sources} ${both_sources})
  endif()
  if(TARGET SDL3-static)
    target_sources(SDL3-static PRIVATE ${static_sources} ${both_sources})
  endif()
  set_property(TARGET SDL3-collector APPEND PROPERTY INTERFACE_SOURCES ${shared_sources} ${static_sources} ${both_sources})
endfunction()

# Use sdl_sources to add sources to SDL3-shared, to SDL3-static, or to both.
function(sdl_sources)
  cmake_parse_arguments(ARGS "" "" "SHARED;STATIC" ${ARGN})
  if(TARGET SDL3-shared)
    target_sources(SDL3-shared PRIVATE ${ARGS_SHARED} ${ARGS_UNPARSED_ARGUMENTS})
  endif()
  if(TARGET SDL3-static)
    target_sources(SDL3-static PRIVATE ${ARGS_STATIC} ${ARGS_UNPARSED_ARGUMENTS})
  endif()
  set_property(TARGET SDL3-collector APPEND PROPERTY INTERFACE_SOURCES ${ARGS_SHARED} ${ARGS_STATIC} ${ARGS_UNPARSED_ARGUMENTS})
endfunction()

# Use sdl_generic_link_dependency to describe a private dependency. All options are optional.
# Users should use sdl_link_dependency and sdl_test_link_dependency instead
# - SHARED_TARGETS: shared targets to add this dependency to
# - STATIC_TARGETS: static targets to add this dependency to
# - COLLECTOR: target that stores information, for pc and Config.cmake generation.
# - INCLUDES: the include directories of the dependency
# - PKG_CONFIG_PREFIX: name of the prefix, when using the functions of FindPkgConfig
# - PKG_CONFIG_SPECS: pkg-config spec, used as argument for the functions of FindPkgConfig
# - PKG_CONFIG_LIBS: libs that will only end up in the Libs.private of the .pc file
# - PKG_CONFIG_LINK_OPTIONS: ldflags that will only end up in the Libs.private of sdl3.pc
# - CMAKE_MODULE: CMake module name of the dependency, used as argument of find_package
# - LIBS: list of libraries to link to (cmake and pkg-config)
# - LINK_OPTIONS: list of link options (also used in pc file, unless PKG_CONFIG_LINK_OPTION is used)
function(sdl_generic_link_dependency ID)
  cmake_parse_arguments(ARGS "" "COLLECTOR" "SHARED_TARGETS;STATIC_TARGETS;INCLUDES;PKG_CONFIG_LINK_OPTIONS;PKG_CONFIG_LIBS;PKG_CONFIG_PREFIX;PKG_CONFIG_SPECS;CMAKE_MODULE;LIBS;LINK_OPTIONS" ${ARGN})
  foreach(target IN LISTS ARGS_SHARED_TARGETS)
    if(TARGET ${target})
      target_include_directories(${target} SYSTEM PRIVATE ${ARGS_INCLUDES})
      target_link_libraries(${target} PRIVATE ${ARGS_LIBS})
      target_link_options(${target} PRIVATE ${ARGS_LINK_OPTIONS})
    endif()
  endforeach()
  foreach(target IN LISTS ARGS_STATIC_TARGETS)
    if(TARGET ${target})
      target_include_directories(${target} SYSTEM PRIVATE ${ARGS_INCLUDES})
      target_link_libraries(${target} PRIVATE ${ARGS_LIBS})
      target_link_options(${target} INTERFACE ${ARGS_LINK_OPTIONS})
    endif()
  endforeach()
  get_property(ids TARGET ${ARGS_COLLECTOR} PROPERTY INTERFACE_SDL_DEP_IDS)
  if(NOT ID IN_LIST ids)
    set_property(TARGET ${ARGS_COLLECTOR} APPEND PROPERTY INTERFACE_SDL_DEP_IDS ${ID})
  endif()
  set_property(TARGET ${ARGS_COLLECTOR} APPEND PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_PREFIX ${ARGS_PKG_CONFIG_PREFIX})
  set_property(TARGET ${ARGS_COLLECTOR} APPEND PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_SPECS ${ARGS_PKG_CONFIG_SPECS})
  set_property(TARGET ${ARGS_COLLECTOR} APPEND PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_LIBS ${ARGS_PKG_CONFIG_LIBS})
  set_property(TARGET ${ARGS_COLLECTOR} APPEND PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_LINK_OPTIONS ${ARGS_PKG_CONFIG_LINK_OPTIONS})
  set_property(TARGET ${ARGS_COLLECTOR} APPEND PROPERTY INTERFACE_SDL_DEP_${ID}_LIBS ${ARGS_LIBS})
  set_property(TARGET ${ARGS_COLLECTOR} APPEND PROPERTY INTERFACE_SDL_DEP_${ID}_LINK_OPTIONS ${ARGS_LINK_OPTIONS})
  set_property(TARGET ${ARGS_COLLECTOR} APPEND PROPERTY INTERFACE_SDL_DEP_${ID}_CMAKE_MODULE ${ARGS_CMAKE_MODULE})
  set_property(TARGET ${ARGS_COLLECTOR} APPEND PROPERTY INTERFACE_SDL_DEP_${ID}_INCLUDES ${ARGS_INCLUDES})
endfunction()

function(sdl_link_dependency )
  sdl_generic_link_dependency(${ARGN} COLLECTOR SDL3-collector SHARED_TARGETS SDL3-shared STATIC_TARGETS SDL3-static)
endfunction()

function(sdl_test_link_dependency )
  sdl_generic_link_dependency(${ARGN} COLLECTOR SDL3_test-collector STATIC_TARGETS SDL3_test)
endfunction()

macro(_get_ARGS_visibility)
  set(_conflict FALSE)
  set(visibility)
  if(ARGS_PRIVATE)
    set(visibility PRIVATE)
  elseif(ARGS_PUBLIC)
    if(visibility)
      set(_conflict TRUE)
    endif()
    set(visibility PUBLIC)
  elseif(ARGS_INTERFACE)
    if(visibility)
      set(_conflict TRUE)
    endif()
    set(visibility INTERFACE)
  endif()
  if(_conflict OR NOT visibility)
    message(FATAL_ERROR "PRIVATE/PUBLIC/INTERFACE must be used exactly once")
  endif()
  unset(_conflict)
endmacro()

# Use sdl_link_dependency to add compile definitions to the SDL3 libraries.
function(sdl_compile_definitions)
  cmake_parse_arguments(ARGS "PRIVATE;PUBLIC;INTERFACE;NO_EXPORT" "" "" ${ARGN})
  _get_ARGS_visibility()
  if(TARGET SDL3-shared)
    target_compile_definitions(SDL3-shared ${visibility} ${ARGS_UNPARSED_ARGUMENTS})
  endif()
  if(TARGET SDL3-static)
    target_compile_definitions(SDL3-static ${visibility} ${ARGS_UNPARSED_ARGUMENTS})
  endif()
  if(NOT ARGS_NO_EXPORT AND (ARGS_PUBLIC OR ARGS_INTERFACE))
    set_property(TARGET SDL3-collector APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "${ARGS_UNPARSED_ARGUMENTS}")
  endif()
endfunction()

# Use sdl_link_dependency to add compile options to the SDL3 libraries.
function(sdl_compile_options)
  cmake_parse_arguments(ARGS "PRIVATE;PUBLIC;INTERFACE;NO_EXPORT" "" "" ${ARGN})
  _get_ARGS_visibility()
  set(escaped_opts ${ARGS_UNPARSED_ARGUMENTS})
  if(ARGS_NO_EXPORT)
    set(escaped_opts "$<BUILD_INTERFACE:${ARGS_UNPARSED_ARGUMENTS}>")
  endif()
  if(TARGET SDL3-shared)
    target_compile_options(SDL3-shared ${visibility} ${escaped_opts})
  endif()
  if(TARGET SDL3-static)
    target_compile_options(SDL3-static ${visibility} ${escaped_opts})
  endif()
  if(NOT ARGS_NO_EXPORT AND (ARGS_PUBLIC OR ARGS_INTERFACE))
    set_property(TARGET SDL3-collector APPEND PROPERTY INTERFACE_COMPILE_OPTIONS "${ARGS_UNPARSED_ARGUMENTS}")
  endif()
endfunction()

# Use sdl_link_dependency to add include directories to the SDL3 libraries.
function(sdl_include_directories)
  cmake_parse_arguments(ARGS "SYSTEM;BEFORE;AFTER;PRIVATE;PUBLIC;INTERFACE;NO_EXPORT" "" "" ${ARGN})
  set(system "")
  if(ARGS_SYSTEM)
    set(system "SYSTEM")
  endif()
  set(before_after )
  if(ARGS_AFTER)
    set(before_after "AFTER")
  endif()
  if(ARGS_BEFORE)
    if(before_after)
      message(FATAL_ERROR "before and after are exclusive options")
    endif()
    set(before_after "BEFORE")
  endif()
  _get_ARGS_visibility()
  if(TARGET SDL3-shared)
    target_include_directories(SDL3-shared ${system} ${before_after} ${visibility} ${ARGS_UNPARSED_ARGUMENTS})
  endif()
  if(TARGET SDL3-static)
    target_include_directories(SDL3-static ${system} ${before_after} ${visibility} ${ARGS_UNPARSED_ARGUMENTS})
  endif()
  if(NOT NO_EXPORT AND (ARGS_PUBLIC OR ARGS_INTERFACE))
    set_property(TARGET SDL3-collector APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${ARGS_UNPARSED_ARGUMENTS}")
  endif()
endfunction()

# Use sdl_link_dependency to add link directories to the SDL3 libraries.
function(sdl_link_directories)
  if(TARGET SDL3-shared)
    target_link_directories(SDL3-shared PRIVATE ${ARGN})
  endif()
  if(TARGET SDL3-static)
    target_link_directories(SDL3-static INTERFACE ${ARGN})
  endif()
endfunction()

# Use sdl_pc_link_options to add a link option, only visible in sdl3.pc
function(sdl_pc_link_options)
  set_property(TARGET SDL3-collector APPEND PROPERTY INTERFACE_SDL_PC_LINK_OPTIONS "${ARGN}")
endfunction()

# Use sdl_pc_link_options to add a link option only to SDL3-shared
function(sdl_shared_link_options)
  if(TARGET SDL3-shared)
    target_link_options(SDL3-shared PRIVATE ${ARGN})
  endif()
endfunction()

# Return minimum list of custom SDL CMake modules, used for finding dependencies of SDL.
function(sdl_cmake_config_required_modules OUTPUT)
  set(cmake_modules)
  foreach(collector SDL3-collector SDL3_test-collector)
    get_property(ids TARGET ${collector} PROPERTY INTERFACE_SDL_DEP_IDS)
    foreach(ID IN LISTS ids)
      get_property(CMAKE_MODULE TARGET ${collector} PROPERTY INTERFACE_SDL_DEP_${ID}_CMAKE_MODULE)
      if(CMAKE_MODULE)
        if(EXISTS "${SDL3_SOURCE_DIR}/cmake/Find${CMAKE_MODULE}.cmake")
          list(APPEND cmake_modules "${SDL3_SOURCE_DIR}/cmake/Find${CMAKE_MODULE}.cmake")
        endif()
      endif()
    endforeach()
    if(cmake_modules)
      list(APPEND cmake_modules "${SDL3_SOURCE_DIR}/cmake/PkgConfigHelper.cmake")
    endif()
  endforeach()
  set(${OUTPUT} "${cmake_modules}" PARENT_SCOPE)
endfunction()

# Generate string for SDL3Config.cmake, finding all pkg-config dependencies of SDL3.
function(sdl_cmake_config_find_pkg_config_commands OUTPUT)
  cmake_parse_arguments(ARGS "" "COLLECTOR;CONFIG_COMPONENT_FOUND_NAME" "" ${ARGN})
  if(NOT ARGS_COLLECTOR OR NOT ARGS_CONFIG_COMPONENT_FOUND_NAME)
    message(FATAL_ERROR "COLLECTOR AND CONFIG_COMPONENT_FOUND_NAME are required arguments")
  endif()
  get_property(ids TARGET ${ARGS_COLLECTOR} PROPERTY INTERFACE_SDL_DEP_IDS)

  set(static_pkgconfig_deps_checks)
  set(static_module_deps_checks)
  set(cmake_modules_seen)

  foreach(ID IN LISTS ids)
    get_property(PKG_CONFIG_PREFIX  TARGET ${ARGS_COLLECTOR} PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_PREFIX)
    get_property(PKG_CONFIG_SPECS   TARGET ${ARGS_COLLECTOR} PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_SPECS)
    get_property(CMAKE_MODULE       TARGET ${ARGS_COLLECTOR} PROPERTY INTERFACE_SDL_DEP_${ID}_CMAKE_MODULE)
    if(CMAKE_MODULE AND NOT CMAKE_MODULE IN_LIST cmake_modules_seen)
      list(APPEND static_module_deps_checks
        "find_package(${CMAKE_MODULE})"
        "if(NOT ${CMAKE_MODULE}_FOUND)"
        "  set(${ARGS_CONFIG_COMPONENT_FOUND_NAME} OFF)"
        "endif()"
        )
      list(APPEND cmake_modules_seen ${CMAKE_MODULE})
    endif()
    if(PKG_CONFIG_PREFIX AND PKG_CONFIG_SPECS)
      string(JOIN " " pkg_config_specs_str ${PKG_CONFIG_SPECS})
      list(APPEND static_pkgconfig_deps_checks
        "  pkg_check_modules(${PKG_CONFIG_PREFIX} QUIET IMPORTED_TARGET ${pkg_config_specs_str})"
        "  if(NOT ${PKG_CONFIG_PREFIX}_FOUND)"
        "    set(${ARGS_CONFIG_COMPONENT_FOUND_NAME} OFF)"
        "  endif()"
      )
    endif()
  endforeach()

  set(prefix "  ")

  set(static_module_deps_texts)
  if(static_module_deps_checks)
    set(static_module_deps_texts
      [[set(_original_module_path "${CMAKE_MODULE_PATH}")]]
      [[list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")]]
      ${static_module_deps_checks}
      [[set(CMAKE_MODULE_PATH "${_original_module_path}")]]
      [[unset(_original_module_path)]]
    )
  endif()

  set(static_pkgconfig_deps_texts)
  if(static_pkgconfig_deps_checks)
    string(JOIN "\n${prefix}" static_deps_texts_str ${static_deps_texts})
    list(APPEND static_pkgconfig_deps_texts
      "find_package(PkgConfig)"
      "if(PkgConfig_FOUND)"
      ${static_pkgconfig_deps_checks}
      "else()"
      "  set(${ARGS_CONFIG_COMPONENT_FOUND_NAME} OFF)"
      "endif()"
  )
  endif()

  set(text)
  string(JOIN "\n${prefix}" text ${static_module_deps_texts} ${static_pkgconfig_deps_texts})
  if(text)
    set(text "${prefix}${text}")
  endif()

  set(${OUTPUT} "${text}" PARENT_SCOPE)
endfunction()

# Create sdl3.pc.
function(configure_sdl3_pc)
  # Clean up variables for sdl3.pc
  if(TARGET SDL3-shared)
    set(SDL_PC_SECTION_LIBS_PRIVATE "\nLibs.private:")
  else()
    set(SDL_PC_SECTION_LIBS_PRIVATE "")
  endif()

  get_property(ids TARGET SDL3-collector PROPERTY SDL3-collector PROPERTY INTERFACE_SDL_DEP_IDS)

  set(private_requires)
  set(private_libs)
  set(private_ldflags)

  foreach(ID IN LISTS ids)
    get_property(CMAKE_MODULE       TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_CMAKE_MODULE)
    get_property(PKG_CONFIG_SPECS   TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_SPECS)
    get_property(PKG_CONFIG_LIBS    TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_LIBS)
    get_property(PKG_CONFIG_LDFLAGS TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_LINK_OPTIONS)
    get_property(LIBS               TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_LIBS)
    get_property(LINK_OPTIONS       TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_LINK_OPTIONS)

    list(APPEND private_requires ${PKG_CONFIG_SPECS})
    list(APPEND private_libs ${PKG_CONFIG_LIBS})
    if(PKG_CONFIG_SPECS OR PKG_CONFIG_LIBS OR PKG_CONFIG_LDFLAGS)
      list(APPEND private_ldflags ${PKG_CONFIG_LDFLAGS})
    else()
      list(APPEND private_ldflags ${LINK_OPTIONS})
      if(NOT CMAKE_MODULE)
        list(APPEND private_libs ${LIBS})
      endif()
    endif()
  endforeach()

  list(TRANSFORM private_libs PREPEND "-l")
  set(SDL_PC_STATIC_LIBS ${private_ldflags} ${private_libs})
  list(REMOVE_DUPLICATES SDL_PC_STATIC_LIBS)
  string(JOIN " " SDL_PC_STATIC_LIBS ${SDL_PC_STATIC_LIBS})

  string(JOIN " " SDL_PC_PRIVATE_REQUIRES ${private_requires})
  string(REGEX REPLACE "(>=|>|=|<|<=)" [[ \1 ]] SDL_PC_PRIVATE_REQUIRES "${SDL_PC_PRIVATE_REQUIRES}")

  get_property(interface_defines TARGET SDL3-collector PROPERTY INTERFACE_COMPILE_DEFINITIONS)
  list(TRANSFORM interface_defines PREPEND "-D")
  get_property(interface_includes TARGET SDL3-collector PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
  list(TRANSFORM interface_includes PREPEND "-I")
  set(SDL_PC_CFLAGS ${interface_defines} ${interface_includes})
  string(JOIN " " SDL_PC_CFLAGS ${SDL_PC_CFLAGS})

  get_property(SDL_PC_LIBS TARGET SDL3-collector PROPERTY INTERFACE_SDL_PC_LINK_OPTIONS)
  string(JOIN " " SDL_PC_LIBS ${SDL_PC_LIBS})

  string(REGEX REPLACE "-lSDL3( |$)" "-l${sdl_static_libname} " SDL_PC_STATIC_LIBS "${SDL_PC_STATIC_LIBS}")
  if(NOT SDL_SHARED)
    string(REGEX REPLACE "-lSDL3( |$)" "-l${sdl_static_libname} " SDL_PC_LIBS "${SDL_PC_LIBS}")
  endif()
  if(TARGET SDL3-shared AND TARGET SDL3-static AND NOT sdl_static_libname STREQUAL "SDL3")
    message(STATUS "\"pkg-config --static --libs sdl3\" will return invalid information")
  endif()

  if(SDL_RELOCATABLE)
    # Calculate prefix relative to location of sdl3.pc
    if(NOT IS_ABSOLUTE "${CMAKE_INSTALL_PREFIX}")
      set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_PREFIX}")
    endif()
    file(RELATIVE_PATH SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG "${CMAKE_INSTALL_PREFIX}/${SDL_PKGCONFIG_INSTALLDIR}" "${CMAKE_INSTALL_PREFIX}")
    string(REGEX REPLACE "[/]+$" "" SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG "${SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG}")
    set(SDL_PKGCONFIG_PREFIX "\${pcfiledir}/${SDL_PATH_PREFIX_RELATIVE_TO_PKGCONFIG}")
  else()
    set(SDL_PKGCONFIG_PREFIX "${CMAKE_INSTALL_PREFIX}")
  endif()

  if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(INCLUDEDIR_FOR_PKG_CONFIG "${CMAKE_INSTALL_INCLUDEDIR}")
  else()
    set(INCLUDEDIR_FOR_PKG_CONFIG "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
  endif()
  if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(LIBDIR_FOR_PKG_CONFIG "${CMAKE_INSTALL_LIBDIR}")
  else()
    set(LIBDIR_FOR_PKG_CONFIG "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
  endif()

  configure_file("${SDL3_SOURCE_DIR}/cmake/sdl3.pc.in" "${SDL3_BINARY_DIR}/sdl3.pc" @ONLY)
endfunction()

# Write list of dependencies to output. Only visible when configuring with --log-level=DEBUG.
function(debug_show_sdl_deps)
  get_property(ids TARGET SDL3-collector PROPERTY SDL3-collector PROPERTY INTERFACE_SDL_DEP_IDS)

  foreach(ID IN LISTS ids)
    message(DEBUG "- id: ${ID}")
    get_property(INCLUDES           TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_INCLUDES)
    get_property(CMAKE_MODULE       TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_CMAKE_MODULE)
    get_property(PKG_CONFIG_PREFIX  TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_PREFIX)
    get_property(PKG_CONFIG_SPECS   TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_PKG_CONFIG_SPECS)
    get_property(LIBS               TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_LIBS)
    get_property(LINK_OPTIONS       TARGET SDL3-collector PROPERTY INTERFACE_SDL_DEP_${ID}_LINK_OPTIONS)
    message(DEBUG "    INCLUDES: ${INCLUDES}")
    message(DEBUG "    CMAKE_MODULE: ${CMAKE_MODULE}")
    message(DEBUG "    PKG_CONFIG_PREFIX: ${PKG_CONFIG_PREFIX}")
    message(DEBUG "    PKG_CONFIG_SPECS: ${PKG_CONFIG_SPECS}")
    message(DEBUG "    LIBS: ${LIBS}")
    message(DEBUG "    LINK_OPTIONS: ${LINK_OPTIONS}")
  endforeach()
endfunction()
