set(CMAKE_SYSTEM_NAME "Windows")

include("${CMAKE_CURRENT_LIST_DIR}/watcom-common.cmake")

set(windows_include_dirs "${WATCOM_ROOT}/h/nt/directx;${WATCOM_ROOT}/h/nt;${WATCOM_ROOT}/h")
list(APPEND CMAKE_C_STANDARD_INCLUDE_DIRECTORIES "${windows_include_dirs}")
list(APPEND CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "${windows_include_dirs}")
list(APPEND CMAKE_SYSTEM_INCLUDE_PATH "${windows_include_dirs}")

set(CMAKE_C_FLAGS_INIT "")
foreach(incdir IN LISTS windows_include_dirs)
    set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -I\"${incdir}\"")
endforeach()

list(APPEND CMAKE_SYSTEM_LIBRARY_PATH "${WATCOM_ROOT}/lib386/nt/directx;${WATCOM_ROOT}/lib386/nt;${WATCOM_ROOT}/lib386")
