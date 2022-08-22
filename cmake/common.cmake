function(sdl_find_soname_library SOFILENAME LIB_FILEPATH)
  if(WIN32 OR OS2)
    set(${SOFILENAME} "${SOFILENAME}-NOTFOUND" PARENT_SCOPE)
  else()
    # reduce the library name for dynamic linking
    get_filename_component(LIB_REALPATH "${LIB_FILEPATH}" REALPATH)  # resolves symlinks
    get_filename_component(LIB_REALNAME "${LIB_REALPATH}" NAME)
    if(LIB_REALPATH STREQUAL "${LIB_FILEPATH}")
      set(${SOFILENAME} "${SOFILENAME}-NOTFOUND" PARENT_SCOPE)
    else()
      if(APPLE)
        string(REGEX REPLACE "(\\.[0-9]*)\\.[0-9\\.]*dylib$" "\\1.dylib" LIB_REGEXD "${LIB_REALNAME}")
      else()
        string(REGEX REPLACE "(\\.[0-9]*)\\.[0-9\\.]*$" "\\1" LIB_REGEXD "${LIB_REALNAME}")
      endif()
      set(${SOFILENAME} "${LIB_REGEXD}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

function(sdl_add_soname_library TARGET)
  get_target_property(lib_filepath ${TARGET} IMPORTED_LOCATION)
  sdl_find_soname_library(soname "${lib_filepath}")
  if(soname)
    set_target_properties(${TARGET}
      PROPERTIES
        IMPORTED_SONAME "${soname}"
        IMPORTED_NO_SONAME "0"
    )
  else()
    set_target_properties(${TARGET}
      PROPERTIES
        IMPORTED_NO_SONAME "1"
    )
  endif()
endfunction()