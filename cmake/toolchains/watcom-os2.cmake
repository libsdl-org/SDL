set(CMAKE_SYSTEM_NAME "OS2")

include("${CMAKE_CURRENT_LIST_DIR}/watcom-common.cmake")

set(os2_include_dirs "${WATCOM_ROOT}/h/os2;${WATCOM_ROOT}/h")
list(APPEND CMAKE_C_STANDARD_INCLUDE_DIRECTORIES "${os2_include_dirs}")
list(APPEND CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "${os2_include_dirs}")
list(APPEND CMAKE_SYSTEM_INCLUDE_PATH "${os2_include_dirs}")

set(CMAKE_C_FLAGS_INIT "")
foreach(incdir IN LISTS os2_include_dirs)
    set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -I\"${incdir}\"")
endforeach()

list(APPEND CMAKE_SYSTEM_LIBRARY_PATH "${WATCOM_ROOT}/lib386;${WATCOM_ROOT}/lib386/os2")
