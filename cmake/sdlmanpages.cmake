include(CMakeParseArguments)
include(GNUInstallDirs)

function(SDL_generate_manpages)
  cmake_parse_arguments(ARG "" "RESULT_VARIABLE;NAME;BUILD_DOCDIR;HEADERS_DIR;SOURCE_DIR;SYMBOL;OPTION_FILE;WIKIHEADERS_PL_PATH;REVISION" "" ${ARGN})

  set(wikiheaders_extra_args)

  if(NOT ARG_NAME)
    set(ARG_NAME "${PROJECT_NAME}")
  endif()

  if(NOT ARG_SOURCE_DIR)
    set(ARG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()

  if(NOT ARG_OPTION_FILE)
    set(ARG_OPTION_FILE "${PROJECT_SOURCE_DIR}/.wikiheaders-options")
  endif()

  if(NOT ARG_HEADERS_DIR)
    message(FATAL_ERROR "Missing required HEADERS_DIR argument")
  endif()

  # FIXME: get rid of SYMBOL and let the perl script figure out the dependencies
  if(NOT ARG_SYMBOL)
    message(FATAL_ERROR "Missing required SYMBOL argument")
  endif()

  if(ARG_REVISION)
    list(APPEND wikiheaders_extra_args "--rev=${ARG_REVISION}")
  endif()

  if(NOT ARG_BUILD_DOCDIR)
    set(ARG_BUILD_DOCDIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
  endif()
  set(BUILD_WIKIDIR "${ARG_BUILD_DOCDIR}/wiki")
  set(BUILD_MANDIR "${ARG_BUILD_DOCDIR}/man")

  find_package(Perl)
  file(GLOB HEADER_FILES "${ARG_HEADERS_DIR}/*.h")

  set(result FALSE)

  if(PERL_FOUND AND EXISTS "${ARG_WIKIHEADERS_PL_PATH}")
    add_custom_command(
      OUTPUT "${BUILD_WIKIDIR}/${ARG_SYMBOL}.md"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${BUILD_WIKIDIR}"
      COMMAND "${PERL_EXECUTABLE}" "${ARG_WIKIHEADERS_PL_PATH}" "${ARG_SOURCE_DIR}" "${BUILD_WIKIDIR}" "--options=${ARG_OPTION_FILE}" --copy-to-wiki ${wikiheaders_extra_args}
      DEPENDS ${HEADER_FILES} "${ARG_WIKIHEADERS_PL_PATH}" "${ARG_OPTION_FILE}"
      COMMENT "Generating ${ARG_NAME} wiki markdown files"
    )
    add_custom_command(
      OUTPUT "${BUILD_MANDIR}/man3/${ARG_SYMBOL}.3"
      COMMAND "${PERL_EXECUTABLE}" "${ARG_WIKIHEADERS_PL_PATH}" "${ARG_SOURCE_DIR}" "${BUILD_WIKIDIR}" "--options=${ARG_OPTION_FILE}" "--manpath=${BUILD_MANDIR}" --copy-to-manpages ${wikiheaders_extra_args}
      DEPENDS  "${BUILD_WIKIDIR}/${ARG_SYMBOL}.md" "${ARG_WIKIHEADERS_PL_PATH}" "${ARG_OPTION_FILE}"
      COMMENT "Generating ${ARG_NAME} man pages"
    )
    add_custom_target(${ARG_NAME}-docs ALL DEPENDS "${BUILD_MANDIR}/man3/${ARG_SYMBOL}.3")

    install(DIRECTORY "${BUILD_MANDIR}/" DESTINATION "${CMAKE_INSTALL_MANDIR}")
    set(result TRUE)
  endif()

  if(ARG_RESULT_VARIABLE)
    set(${ARG_RESULT_VARIABLE} ${result} PARENT_SCOPE)
  endif()
endfunction()
